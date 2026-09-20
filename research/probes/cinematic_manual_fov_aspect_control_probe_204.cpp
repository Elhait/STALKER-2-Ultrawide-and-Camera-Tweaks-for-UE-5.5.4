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

namespace {
constexpr std::size_t kSetterOffset = 0x19;
constexpr std::uint8_t kSetterBytes[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F };
constexpr std::uintptr_t kAspectOffset = 0x254;
constexpr std::uintptr_t kFlagsOffset = 0x259;
constexpr std::uintptr_t kFovOffset = 0x230;
constexpr std::uintptr_t kExitRva = 0x6B6C482;
constexpr std::uint8_t kExitBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE };
constexpr float kWideAspect = 5120.0f / 1440.0f;
constexpr float kNativeAspect = 16.0f / 9.0f;
constexpr float kManualFov = 127.3927f;

HMODULE g_module{};
HMODULE g_executable = GetModuleHandle(nullptr);
std::shared_ptr<spdlog::logger> g_logger;
SafetyHookMid g_enterHook;
SafetyHookMid g_exitHook;
std::mutex g_mutex;
std::uintptr_t g_target{};
float g_baselineFov{};
float g_baselineAspect{};
std::uint64_t g_sequence{};
std::atomic<bool> g_active{};

template <typename T>
bool Read(std::uintptr_t address, T& value) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (address >= end || sizeof(T) > end - address) return false;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

template <typename T>
bool Write(std::uintptr_t address, const T& value) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if (address >= end || sizeof(T) > end - address || !(info.Protect & writable)) return false;
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
    return true;
}

void LogState(const char* stage, std::uintptr_t target, float requestedFov, float requestedAspect, bool fovWritten, bool aspectWritten) {
    if (!g_logger) return;
    float fov{}, aspect{};
    std::uint8_t flags{};
    Read(target + kFovOffset, fov);
    Read(target + kAspectOffset, aspect);
    Read(target + kFlagsOffset, flags);
    g_logger->info("TRACE seq={} stage={} target=0x{:X} before/after-request fov={} aspect={} flags=0x{:02X} requestedFov={} requestedAspect={} fovWritten={} aspectWritten={} thread={}",
        ++g_sequence, stage, target, fov, aspect, flags, requestedFov, requestedAspect, fovWritten, aspectWritten, GetCurrentThreadId());
}

void Enter(SafetyHookContext& context) {
    const auto target = static_cast<std::uintptr_t>(context.rax);
    if (!target) return;
    float fov{}, aspect{};
    std::uint8_t flags{};
    if (!Read(target + kFovOffset, fov) || !Read(target + kAspectOffset, aspect) || !Read(target + kFlagsOffset, flags)) return;
    std::lock_guard lock(g_mutex);
    if (!g_active.load(std::memory_order_relaxed)) {
        g_target = target;
        g_baselineFov = fov;
        g_baselineAspect = aspect;
        g_active.store(true, std::memory_order_release);
        if (g_logger) g_logger->info("TRACE stage=enter-baseline target=0x{:X} fov={} aspect={} flags=0x{:02X} thread={}",
            target, fov, aspect, flags, GetCurrentThreadId());
    }
    context.rip += sizeof(kSetterBytes);
}

bool CurrentTarget(std::uintptr_t& target, float& baselineFov, float& baselineAspect) {
    std::lock_guard lock(g_mutex);
    if (!g_active.load(std::memory_order_acquire) || !g_target) return false;
    target = g_target;
    baselineFov = g_baselineFov;
    baselineAspect = g_baselineAspect;
    return true;
}

void Apply(const char* action, bool writeFov, float fov, bool writeAspect, float aspect) {
    std::uintptr_t target{};
    float baselineFov{}, baselineAspect{};
    if (!CurrentTarget(target, baselineFov, baselineAspect)) {
        if (g_logger) g_logger->warn("TRACE stage={} rejected=no-active-cinematic-target", action);
        return;
    }
    const bool fovWritten = !writeFov || Write(target + kFovOffset, fov);
    const bool aspectWritten = !writeAspect || Write(target + kAspectOffset, aspect);
    LogState(action, target, writeFov ? fov : baselineFov, writeAspect ? aspect : baselineAspect, fovWritten, aspectWritten);
}

void Exit(SafetyHookContext&) {
    std::lock_guard lock(g_mutex);
    if (!g_active.load(std::memory_order_acquire) || !g_target) return;
    const auto target = g_target;
    const bool restoredFov = Write(target + kFovOffset, g_baselineFov);
    const bool restoredAspect = Write(target + kAspectOffset, g_baselineAspect);
    LogState("exit-restore", target, g_baselineFov, g_baselineAspect, restoredFov, restoredAspect);
    g_target = 0;
    g_active.store(false, std::memory_order_release);
}

void HotkeyLoop() {
    constexpr int kFirstId = 1;
    for (int id = kFirstId; id < kFirstId + 8; ++id)
        RegisterHotKey(nullptr, id, MOD_NOREPEAT, VK_F1 + (id - kFirstId));
    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        if (message.message != WM_HOTKEY) continue;
        switch (static_cast<int>(message.wParam)) {
        case 1: Apply("manual-fov", true, kManualFov, false, 0.0f); break;
        case 2: Apply("manual-aspect", false, 0.0f, true, kWideAspect); break;
        case 3: Apply("manual-fov-then-aspect", true, kManualFov, true, kWideAspect); break;
        case 4: Apply("manual-aspect-then-fov", false, 0.0f, true, kWideAspect); Apply("manual-aspect-then-fov-step2", true, kManualFov, false, 0.0f); break;
        case 5: Apply("manual-native-aspect", false, 0.0f, true, kNativeAspect); break;
        case 6: { float f{}, a{}; std::uintptr_t t{}; if (CurrentTarget(t, f, a)) Apply("manual-restore", true, f, true, a); break; }
        case 7: { std::uintptr_t t{}; float f{}, a{}; if (CurrentTarget(t, f, a)) LogState("manual-snapshot", t, f, a, false, false); break; }
        case 8: { float f{}, a{}; std::uintptr_t t{}; if (CurrentTarget(t, f, a)) Apply("manual-baseline-fov-wide-aspect", true, f, true, kWideAspect); break; }
        default: break;
        }
    }
}

DWORD WINAPI Initialize(void*) {
    WCHAR modulePath[MAX_PATH]{};
    GetModuleFileNameW(g_module, modulePath, MAX_PATH);
    const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2CinematicManualFovAspectControlProbe204.log";
    std::ofstream(path, std::ios::trunc).close();
    try {
        g_logger = spdlog::basic_logger_mt("STALKER2CinematicManualFovAspectControlProbe204", path.string(), true);
        g_logger->flush_on(spdlog::level::info);
        const auto matches = Memory::PatternScanAll(g_executable,
            "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
        if (matches.size() != 1) throw std::runtime_error("ENTER signature is ambiguous or missing");
        auto* setter = matches.front() + kSetterOffset;
        if (std::memcmp(setter, kSetterBytes, sizeof(kSetterBytes)) != 0) throw std::runtime_error("ENTER bytes mismatch");
        auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitRva;
        if (std::memcmp(exit, kExitBytes, sizeof(kExitBytes)) != 0) throw std::runtime_error("EXIT bytes mismatch");
        g_enterHook = safetyhook::create_mid(setter, Enter);
        g_exitHook = safetyhook::create_mid(exit, Exit);
        if (!g_enterHook || !g_exitHook) throw std::runtime_error("hook creation failed");
        g_logger->info("TRACE installed: 2.0.4 manual FOV/aspect control probe; F1-F8 registered, flags untouched.");
        std::thread(HotkeyLoop).detach();
    } catch (const std::exception& exception) {
        if (g_logger) g_logger->error("TRACE setup refused: {}", exception.what());
        g_exitHook.reset();
        g_enterHook.reset();
    }
    return 0;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
