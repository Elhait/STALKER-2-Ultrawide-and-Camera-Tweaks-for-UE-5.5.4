// Experimental integration wrapper for 2.0.4.
// Reuses the stable gameplay implementation unchanged and adds the validated
// cinematic lifecycle probe as a separate, temporary diagnostic component.
#define DllMain GameplayFixDllMain
#include "gameplay_aspect_fix.cpp"
#undef DllMain

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace cinematic_integration
{
    constexpr std::size_t kSetterOffset = 0x19;
    constexpr std::uint8_t kSetterBytes[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F };
    constexpr std::uintptr_t kExitRva = 0x6B6C482;
    constexpr std::uint8_t kExitBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE };
    constexpr float kWideAspect = 32.0f / 9.0f;
    constexpr float kDiagnosticFov = 127.3927f;
    constexpr std::uint64_t kSteadyMs = 150;
    constexpr std::uint64_t kPostExitMs = 3000;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_enterHook, g_exitHook;
    std::mutex g_mutex;
    std::uintptr_t g_inner{};
    std::atomic_bool g_active{};
    std::atomic_bool g_exitSeen{};
    std::atomic_bool g_correctionApplied{};
    std::atomic_bool g_stop{};
    std::atomic<std::uint64_t> g_exitDeadline{};
    std::uint64_t g_sequence{};

    struct State { float fov{}; float aspect{}; std::uint8_t flags{}; };

    template <typename T>
    bool Read(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(T) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    template <typename T>
    bool Write(std::uintptr_t address, const T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(T) > end - address) return false;
        DWORD oldProtection{};
        if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), PAGE_READWRITE, &oldProtection)) return false;
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
        DWORD ignored{};
        VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), oldProtection, &ignored);
        return true;
    }

    bool Capture(std::uintptr_t inner, State& state)
    {
        return inner && Read(inner + 0x230, state.fov) && Read(inner + 0x254, state.aspect) &&
            Read(inner + 0x259, state.flags);
    }

    void LogState(const char* phase, const char* stage, std::uintptr_t inner,
        const State& before, const State& after, std::uint64_t dtMs)
    {
        g_logger->info("TRACE seq={} phase={} stage={} inner=0x{:X} dtMs={} fov={} -> {} aspect={} -> {} flags=0x{:02X} -> 0x{:02X}",
            ++g_sequence, phase, stage, inner, dtMs, before.fov, after.fov,
            before.aspect, after.aspect, before.flags, after.flags);
    }

    void Enter(SafetyHookContext& context)
    {
        const auto inner = static_cast<std::uintptr_t>(context.rax);
        State state{};
        if (!Capture(inner, state)) return;
        std::lock_guard lock(g_mutex);
        g_inner = inner;
        g_active.store(true, std::memory_order_release);
        g_exitSeen.store(false, std::memory_order_release);
        g_correctionApplied.store(false, std::memory_order_release);
        g_exitDeadline.store(0, std::memory_order_release);
        g_logger->info("TRACE seq={} phase=ENTER_TRANSITION stage=enter inner=0x{:X} fov={} aspect={} flags=0x{:02X}",
            ++g_sequence, inner, state.fov, state.aspect, state.flags);
    }

    void Exit(SafetyHookContext& context)
    {
        std::uintptr_t inner{};
        if (!Read(static_cast<std::uintptr_t>(context.rcx) + 0xF8, inner) || !inner) return;
        std::lock_guard lock(g_mutex);
        if (g_active.load(std::memory_order_acquire) && inner == g_inner) {
            g_exitSeen.store(true, std::memory_order_release);
            g_exitDeadline.store(GetTickCount64() + kPostExitMs, std::memory_order_release);
            g_logger->info("TRACE seq={} phase=EXIT_TRANSITION stage=exit-begin inner=0x{:X}", ++g_sequence, inner);
        }
    }

    void ApplyCorrection(std::uintptr_t inner, const State& before)
    {
        const bool fovWritten = Write(inner + 0x230, kDiagnosticFov);
        const bool aspectWritten = Write(inner + 0x254, kWideAspect);
        State after{};
        const bool readable = Capture(inner, after);
        g_logger->info("TRACE seq={} phase=CINEMATIC_CORRECTION stage=auto-f3 inner=0x{:X} beforeFov={} beforeAspect={} beforeFlags=0x{:02X} afterFov={} afterAspect={} afterFlags=0x{:02X} fovWritten={} aspectWritten={} readable={}",
            ++g_sequence, inner, before.fov, before.aspect, before.flags,
            after.fov, after.aspect, after.flags, fovWritten, aspectWritten, readable);
    }

    DWORD WINAPI Sampler(void*)
    {
        State previous{};
        bool hadPrevious = false;
        bool steadyLogged = false;
        std::uint64_t enterTime = 0;
        std::uint64_t lastChange = 0;
        while (!g_stop.load(std::memory_order_acquire)) {
            std::uintptr_t inner{};
            { std::lock_guard lock(g_mutex); inner = g_inner; }
            if (g_active.load(std::memory_order_acquire) && inner) {
                const auto now = GetTickCount64();
                State current{};
                if (Capture(inner, current)) {
                    if (!hadPrevious) {
                        previous = current; hadPrevious = true; enterTime = now; lastChange = now;
                    } else if (current.fov != previous.fov || current.aspect != previous.aspect || current.flags != previous.flags) {
                        const auto phase = g_exitSeen.load(std::memory_order_acquire) ? "EXIT_TRANSITION" : "ENTER_TRANSITION";
                        LogState(phase, "state-change", inner, previous, current, now - enterTime);
                        previous = current; lastChange = now; steadyLogged = false;
                    } else if (!steadyLogged && !g_exitSeen.load(std::memory_order_acquire) &&
                        now - lastChange >= kSteadyMs && std::fabs(current.fov - 90.0f) <= 0.02f && current.flags == 0x05) {
                        g_logger->info("TRACE seq={} phase=STEADY_CINEMATIC stage=stable-before-auto-f3 inner=0x{:X} dtMs={} fov={} aspect={} flags=0x{:02X}",
                            ++g_sequence, inner, now - enterTime, current.fov, current.aspect, current.flags);
                        steadyLogged = true;
                        if (!g_correctionApplied.exchange(true, std::memory_order_acq_rel))
                            ApplyCorrection(inner, current);
                    }
                }
                const auto deadline = g_exitDeadline.load(std::memory_order_acquire);
                if (deadline && now >= deadline) {
                    State post{};
                    if (Capture(inner, post))
                        g_logger->info("TRACE seq={} phase=POST_STEADY stage=post-exit inner=0x{:X} fov={} aspect={} flags=0x{:02X}",
                            ++g_sequence, inner, post.fov, post.aspect, post.flags);
                    std::lock_guard lock(g_mutex);
                    g_inner = 0;
                    g_active.store(false, std::memory_order_release);
                    g_exitDeadline.store(0, std::memory_order_release);
                    hadPrevious = false;
                    steadyLogged = false;
                }
            }
            Sleep(5);
        }
        return 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2GlobalUltrawideExperimental204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2GlobalUltrawideExperimental204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            const auto matches = Memory::PatternScanAll(g_executable,
                "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
            if (matches.size() != 1 || std::memcmp(matches.front() + kSetterOffset, kSetterBytes, sizeof(kSetterBytes)) != 0)
                throw std::runtime_error("ENTER validation failed");
            auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitRva;
            if (std::memcmp(exit, kExitBytes, sizeof(kExitBytes)) != 0) throw std::runtime_error("EXIT validation failed");
            g_enterHook = safetyhook::create_mid(matches.front() + kSetterOffset, Enter);
            g_exitHook = safetyhook::create_mid(exit, Exit);
            if (!g_enterHook || !g_exitHook) throw std::runtime_error("cinematic hook creation failed");
            const auto sampler = CreateThread(nullptr, 0, Sampler, nullptr, 0, nullptr);
            if (!sampler) throw std::runtime_error("cinematic sampler thread could not start");
            CloseHandle(sampler);
            g_logger->info("TRACE installed: one experimental global ASI; stable gameplay hook plus cinematic auto-F3 after native convergence.");
        } catch (const std::exception& error) {
            g_stop.store(true, std::memory_order_release);
            g_enterHook.reset(); g_exitHook.reset();
            if (g_logger) g_logger->error("TRACE cinematic setup refused safely: {}", error.what());
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    cinematic_integration::g_module = module;
    DisableThreadLibraryCalls(module);
    GameplayFixDllMain(module, reason, reserved);
    if (const auto thread = CreateThread(nullptr, 0, cinematic_integration::Initialize, nullptr, 0, nullptr))
        CloseHandle(thread);
    return TRUE;
}
