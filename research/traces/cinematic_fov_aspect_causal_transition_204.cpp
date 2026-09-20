#include "helper.hpp"

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
#include <thread>

namespace
{
    constexpr std::size_t kSetterOffset = 0x19;
    constexpr std::uint8_t kSetterBytes[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F };
    constexpr std::uintptr_t kExitRva = 0x6B6C482;
    constexpr std::uint8_t kExitBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE };
    constexpr float kWideAspect = 5120.0f / 1440.0f;
    constexpr float kDiagnosticFov = 127.3927f;
#ifdef AUTO_F3_AFTER_STEADY
    constexpr bool kAutoF3AfterSteady = true;
    constexpr std::uint64_t kPostExitObservationMs = 3000;
#else
    constexpr bool kAutoF3AfterSteady = false;
    constexpr std::uint64_t kPostExitObservationMs = 500;
#endif

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_enterHook, g_exitHook;
    std::mutex g_stateMutex;
    std::uintptr_t g_inner{};
    std::atomic<bool> g_active{};
    std::atomic<bool> g_stop{};
    std::atomic<std::uint64_t> g_postUntil{};
    std::uint64_t g_sequence{};
    HANDLE g_commandEvent{};
    std::atomic<bool> g_f3Pending{};

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

    bool Capture(std::uintptr_t inner, State& value)
    {
        return inner && Read(inner + 0x230, value.fov) && Read(inner + 0x254, value.aspect) &&
            Read(inner + 0x259, value.flags);
    }

    void LogState(const char* phase, const char* stage, std::uintptr_t inner,
        const State& previous, const State& current, std::uint64_t deltaMs)
    {
        g_logger->info(
            "TRACE seq={} phase={} stage={} inner=0x{:X} dtMs={} fov={} -> {} aspect={} -> {} flags=0x{:02X} -> 0x{:02X} thread={}",
            ++g_sequence, phase, stage, inner, deltaMs, previous.fov, current.fov,
            previous.aspect, current.aspect, previous.flags, current.flags, GetCurrentThreadId());
    }

    void Enter(SafetyHookContext& context)
    {
        const auto inner = static_cast<std::uintptr_t>(context.rax);
        State state{};
        if (!Capture(inner, state)) {
            g_logger->warn("TRACE stage=enter rejected=invalid-inner");
            return;
        }
        std::lock_guard lock(g_stateMutex);
        g_inner = inner;
        g_active.store(true, std::memory_order_release);
        g_postUntil.store(0, std::memory_order_release);
        g_logger->info("TRACE seq={} phase=ENTER_TRANSITION stage=enter inner=0x{:X} fov={} aspect={} flags=0x{:02X} thread={}",
            ++g_sequence, inner, state.fov, state.aspect, state.flags, GetCurrentThreadId());
    }

    void Exit(SafetyHookContext& context)
    {
        std::uintptr_t inner{};
        if (!Read(static_cast<std::uintptr_t>(context.rcx) + 0xF8, inner) || !inner) return;
        std::lock_guard lock(g_stateMutex);
        if (g_active.load(std::memory_order_acquire) && inner == g_inner) {
            g_logger->info("TRACE seq={} phase=EXIT_TRANSITION stage=exit-begin inner=0x{:X} thread={}",
                ++g_sequence, inner, GetCurrentThreadId());
            g_postUntil.store(GetTickCount64() + kPostExitObservationMs, std::memory_order_release);
        }
    }

    void ApplyDiagnosticF3()
    {
        std::uintptr_t inner{};
        { std::lock_guard lock(g_stateMutex); inner = g_inner; }
        State before{}, after{};
        if (!Capture(inner, before)) {
            g_logger->warn("TRACE stage=F3 rejected=no-active-target");
            return;
        }
        g_logger->info("TRACE seq={} phase=MANUAL_WRITE stage=F3_BEGIN inner=0x{:X} fov={} aspect={} flags=0x{:02X}",
            ++g_sequence, inner, before.fov, before.aspect, before.flags);
        const bool fovWritten = Write(inner + 0x230, kDiagnosticFov);
        const bool aspectWritten = Write(inner + 0x254, kWideAspect);
        const bool readable = Capture(inner, after);
        g_logger->info("TRACE seq={} phase=MANUAL_WRITE stage=F3_END inner=0x{:X} fov={} aspect={} flags=0x{:02X} fovWritten={} aspectWritten={} readable={}",
            ++g_sequence, inner, after.fov, after.aspect, after.flags, fovWritten, aspectWritten, readable);
    }

    DWORD WINAPI CommandWorker(void*)
    {
        while (!g_stop.load(std::memory_order_acquire)) {
            if (WaitForSingleObject(g_commandEvent, 100) != WAIT_OBJECT_0) continue;
            if (g_f3Pending.exchange(false, std::memory_order_acq_rel)) ApplyDiagnosticF3();
        }
        return 0;
    }

    LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
            if (event && event->vkCode == VK_F3 && !(event->flags & LLKHF_UP)) {
                g_f3Pending.store(true, std::memory_order_release);
                if (g_commandEvent) SetEvent(g_commandEvent);
            }
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    DWORD WINAPI KeyboardThread(void*)
    {
        const auto hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, g_module, 0);
        g_logger->info("TRACE keyboard-hook-installed={} error={}", hook != nullptr, hook ? 0 : GetLastError());
        if (!hook) return 0;
        MSG message{};
        while (!g_stop.load(std::memory_order_acquire) && GetMessage(&message, nullptr, 0, 0) > 0) {}
        UnhookWindowsHookEx(hook);
        return 0;
    }

    DWORD WINAPI SamplerThread(void*)
    {
        State previous{};
        bool hadPrevious = false;
        bool steadyLogged = false;
        bool autoF3Applied = false;
        std::uint64_t lastChange = 0;
        std::uint64_t enterTime = 0;
        while (!g_stop.load(std::memory_order_acquire)) {
            std::uintptr_t inner{};
            { std::lock_guard lock(g_stateMutex); inner = g_inner; }
            if (g_active.load(std::memory_order_acquire) && inner) {
                State current{};
                const auto now = GetTickCount64();
                if (Capture(inner, current)) {
                    if (!hadPrevious) { previous = current; hadPrevious = true; enterTime = now; lastChange = now; }
                    else if (current.fov != previous.fov || current.aspect != previous.aspect || current.flags != previous.flags) {
                        const auto postUntil = g_postUntil.load(std::memory_order_acquire);
                        LogState(postUntil ? "EXIT_TRANSITION" : "ENTER_TRANSITION", "state-change", inner, previous, current, now - enterTime);
                        previous = current; lastChange = now; steadyLogged = false;
                    } else if (!steadyLogged && now - lastChange >= 150) {
                        g_logger->info("TRACE seq={} phase=STEADY_CINEMATIC stage=fov-aspect-stable inner=0x{:X} dtMs={} fov={} aspect={} flags=0x{:02X}",
                            ++g_sequence, inner, now - enterTime, current.fov, current.aspect, current.flags);
                        steadyLogged = true;
                        if (kAutoF3AfterSteady && !autoF3Applied && current.flags == 0x05 &&
                            std::fabs(current.fov - 90.0f) <= 0.02f) {
                            g_logger->info("TRACE seq={} phase=STEADY_CINEMATIC stage=AUTO_F3_TRIGGER inner=0x{:X} fov={} aspect={} flags=0x{:02X}",
                                ++g_sequence, inner, current.fov, current.aspect, current.flags);
                            autoF3Applied = true;
                            ApplyDiagnosticF3();
                        }
                    }
                }
                const auto postUntil = g_postUntil.load(std::memory_order_acquire);
                if (postUntil && now >= postUntil) {
                    State post{};
                    if (Capture(inner, post)) g_logger->info("TRACE seq={} phase=POST_CINEMATIC stage=post-exit inner=0x{:X} fov={} aspect={} flags=0x{:02X}",
                        ++g_sequence, inner, post.fov, post.aspect, post.flags);
                    std::lock_guard lock(g_stateMutex);
                    g_inner = 0; g_active.store(false, std::memory_order_release); g_postUntil.store(0, std::memory_order_release);
                    hadPrevious = false; steadyLogged = false;
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
        const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2CinematicFovAspectCausalTransition204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicFovAspectCausalTransition204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v"); g_logger->flush_on(spdlog::level::info);
            const auto matches = Memory::PatternScanAll(g_executable, "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
            if (matches.size() != 1 || std::memcmp(matches.front() + kSetterOffset, kSetterBytes, sizeof(kSetterBytes)) != 0) throw std::runtime_error("ENTER validation failed");
            auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitRva;
            if (std::memcmp(exit, kExitBytes, sizeof(kExitBytes)) != 0) throw std::runtime_error("EXIT validation failed");
            g_enterHook = safetyhook::create_mid(matches.front() + kSetterOffset, Enter);
            g_exitHook = safetyhook::create_mid(exit, Exit);
            if (!g_enterHook || !g_exitHook) throw std::runtime_error("hook creation failed");
            g_commandEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!g_commandEvent) throw std::runtime_error("event creation failed");
            const auto worker = CreateThread(nullptr, 0, CommandWorker, nullptr, 0, nullptr);
            const auto keyboard = CreateThread(nullptr, 0, KeyboardThread, nullptr, 0, nullptr);
            const auto sampler = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
            if (!worker || !keyboard || !sampler) throw std::runtime_error("worker creation failed");
            if (worker) CloseHandle(worker); if (keyboard) CloseHandle(keyboard); if (sampler) CloseHandle(sampler);
            g_logger->info("TRACE installed: 2.0.4 causal FOV/aspect transition; read-only sampler plus one manual F3 write.");
        } catch (const std::exception& error) {
            g_stop.store(true, std::memory_order_release); g_enterHook.reset(); g_exitHook.reset();
            if (g_commandEvent) { CloseHandle(g_commandEvent); g_commandEvent = nullptr; }
            if (g_logger) g_logger->error("TRACE setup refused safely: {}", error.what());
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module; DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
