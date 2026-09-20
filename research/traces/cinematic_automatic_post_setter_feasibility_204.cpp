#include "helper.hpp"
#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace {
constexpr std::size_t kSetterOffset = 0x19;
constexpr std::size_t kSetterLength = 10;
constexpr std::uintptr_t kFovOffset = 0x230;
constexpr std::uintptr_t kAspectOffset = 0x254;
constexpr std::uintptr_t kFlagsOffset = 0x259;
constexpr std::uintptr_t kExitRva = 0x6B6C482;
constexpr float kWideAspect = 5120.0f / 1440.0f;
constexpr float kCorrectionFov = 127.3927f;
constexpr std::uint8_t kSetterBytes[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F };
constexpr std::uint8_t kExitBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE };

HMODULE g_module{}; HMODULE g_executable = GetModuleHandle(nullptr);
std::shared_ptr<spdlog::logger> g_logger; SafetyHookMid g_postSetterHook, g_exitHook;
std::mutex g_mutex; std::uintptr_t g_target{}; float g_baselineFov{}; std::atomic<bool> g_active{}; std::uint64_t g_sequence{};

template <typename T> bool Read(std::uintptr_t address, T& value) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (address >= end || sizeof(T) > end - address) return false;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true;
}
template <typename T> bool Write(std::uintptr_t address, const T& value) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if (address >= end || sizeof(T) > end - address || !(info.Protect & writable)) return false;
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T)); return true;
}
std::uintptr_t Rva(std::uintptr_t address) { const auto base = reinterpret_cast<std::uintptr_t>(g_executable); return address >= base ? address - base : 0; }
void LogState(const char* stage, std::uintptr_t target, float beforeFov, float beforeAspect, std::uint8_t beforeFlags, bool fovWritten, bool aspectWritten) {
    float fov{}, aspect{}; std::uint8_t flags{}; Read(target + kFovOffset, fov); Read(target + kAspectOffset, aspect); Read(target + kFlagsOffset, flags);
    if (g_logger) g_logger->info("TRACE seq={} stage={} target=0x{:X} beforeFov={} beforeAspect={} beforeFlags=0x{:02X} afterFov={} afterAspect={} afterFlags=0x{:02X} fovWritten={} aspectWritten={} thread={}", ++g_sequence, stage, target, beforeFov, beforeAspect, beforeFlags, fov, aspect, flags, fovWritten, aspectWritten, GetCurrentThreadId());
}
void PostSetter(SafetyHookContext& context) {
    const auto target = static_cast<std::uintptr_t>(context.rax); if (!target) return;
    std::lock_guard lock(g_mutex); if (g_active.load(std::memory_order_relaxed)) return;
    float beforeFov{}, beforeAspect{}; std::uint8_t beforeFlags{};
    if (!Read(target + kFovOffset, beforeFov) || !Read(target + kAspectOffset, beforeAspect) || !Read(target + kFlagsOffset, beforeFlags)) return;
    const bool fovWritten = Write(target + kFovOffset, kCorrectionFov);
    const bool aspectWritten = Write(target + kAspectOffset, kWideAspect);
    if (fovWritten && aspectWritten) { g_target = target; g_baselineFov = beforeFov; g_active.store(true, std::memory_order_release); }
    LogState("post-enter", target, beforeFov, beforeAspect, beforeFlags, fovWritten, aspectWritten);
}
void Exit(SafetyHookContext& context) {
    const auto callbackContext = static_cast<std::uintptr_t>(context.rcx); std::uintptr_t target{};
    if (!Read(callbackContext + 0xF8, target) || !target) return;
    std::lock_guard lock(g_mutex); if (!g_active.load(std::memory_order_acquire) || target != g_target) return;
    const bool restored = Write(target + kFovOffset, g_baselineFov);
    LogState("exit-fov-restore", target, g_baselineFov, 0.0f, 0, restored, false);
    g_target = 0; g_active.store(false, std::memory_order_release);
}
DWORD WINAPI Initialize(void*) {
    WCHAR modulePath[MAX_PATH]{}; GetModuleFileNameW(g_module, modulePath, MAX_PATH);
    const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2CinematicAutomaticPostSetterFeasibility204.log";
    std::ofstream(path, std::ios::trunc).close();
    try {
        g_logger = spdlog::basic_logger_mt("STALKER2CinematicAutomaticPostSetterFeasibility204", path.string(), true); g_logger->flush_on(spdlog::level::info);
        const auto matches = Memory::PatternScanAll(g_executable, "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
        if (matches.size() != 1) throw std::runtime_error("ENTER signature is ambiguous or missing");
        auto* setter = matches.front() + kSetterOffset; if (std::memcmp(setter, kSetterBytes, sizeof(kSetterBytes)) != 0 || setter[kSetterLength] != 0xB0 || setter[kSetterLength + 1] != 0x01 || setter[kSetterLength + 2] != 0xC3) throw std::runtime_error("post-setter epilogue validation failed");
        auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitRva; if (std::memcmp(exit, kExitBytes, sizeof(kExitBytes)) != 0) throw std::runtime_error("EXIT bytes mismatch");
        g_postSetterHook = safetyhook::create_mid(setter + kSetterLength, PostSetter); g_exitHook = safetyhook::create_mid(exit, Exit);
        if (!g_postSetterHook || !g_exitHook) throw std::runtime_error("hook creation failed");
        g_logger->info("TRACE installed: 2.0.4 automatic post-setter FOV/aspect feasibility; one ENTER pair, FOV-only EXIT restore.");
    } catch (const std::exception& exception) { if (g_logger) g_logger->error("TRACE setup refused: {}", exception.what()); g_exitHook.reset(); g_postSetterHook.reset(); }
    return 0;
}
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) { if (reason != DLL_PROCESS_ATTACH) return TRUE; g_module = module; DisableThreadLibraryCalls(module); if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread); return TRUE; }
