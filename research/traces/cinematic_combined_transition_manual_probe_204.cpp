#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <array>
#include <atomic>
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
constexpr std::uintptr_t kFovOffset = 0x230;
constexpr std::uintptr_t kAspectOffset = 0x254;
constexpr std::uintptr_t kFlagsOffset = 0x259;
constexpr std::uintptr_t kStateBegin = 0x200;
constexpr std::uintptr_t kStateEnd = 0x2D0;
constexpr std::uintptr_t kExitRva = 0x6B6C482;
constexpr std::uint8_t kExitBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE };
constexpr float kWideAspect = 5120.0f / 1440.0f;
constexpr float kNativeAspect = 16.0f / 9.0f;
constexpr float kManualFov = 127.3927f;

struct Snapshot {
    std::array<std::uint8_t, kStateEnd - kStateBegin> bytes{};
    float fov{};
    float aspect{};
    std::uint8_t flags{};
    bool readable{};
};

HMODULE g_module{};
HMODULE g_executable = GetModuleHandle(nullptr);
std::shared_ptr<spdlog::logger> g_logger;
SafetyHookMid g_enterHook;
#if !defined(STALKER2_TIMING_ONLY)
SafetyHookMid g_exitHook;
#endif
std::mutex g_mutex;
std::uintptr_t g_inner{};
Snapshot g_enterSnapshot{};
float g_baselineFov{};
float g_baselineAspect{};
bool g_hasBaseline{};
std::uint64_t g_sequence{};
HANDLE g_commandEvent{};
std::atomic<int> g_pendingCommand{};

template <typename T> bool Read(std::uintptr_t address, T& value) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (address >= end || sizeof(T) > end - address) return false;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

template <typename T> bool Write(std::uintptr_t address, const T& value) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if (address >= end || sizeof(T) > end - address || !(info.Protect & writable)) return false;
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
    return true;
}

Snapshot Capture(std::uintptr_t inner) {
    Snapshot value{};
    if (!inner) return value;
    value.readable = Read(inner + kStateBegin, value.bytes) &&
        Read(inner + kFovOffset, value.fov) && Read(inner + kAspectOffset, value.aspect) &&
        Read(inner + kFlagsOffset, value.flags);
    return value;
}

void LogSnapshot(const char* stage, std::uintptr_t inner, const Snapshot& before,
                 const Snapshot& current, bool compare) {
    if (!g_logger) return;
    std::size_t changed{};
    if (compare && before.readable && current.readable)
        for (std::size_t i = 0; i < current.bytes.size(); ++i) changed += before.bytes[i] != current.bytes[i] ? 1u : 0u;
    g_logger->info("TRACE seq={} stage={} inner=0x{:X} readable={} fov={} aspect={} flags=0x{:02X} changedBytes={} thread={}",
        ++g_sequence, stage, inner, current.readable, current.fov, current.aspect, current.flags, changed, GetCurrentThreadId());
    if (!compare || !before.readable || !current.readable) return;
    for (std::size_t i = 0; i < current.bytes.size(); ++i)
        if (before.bytes[i] != current.bytes[i])
            g_logger->info("TRACE change stage={} innerOffset=0x{:X} old=0x{:02X} new=0x{:02X}",
                stage, kStateBegin + i, before.bytes[i], current.bytes[i]);
}

bool Current(std::uintptr_t& inner, float& fov, float& aspect) {
    std::lock_guard lock(g_mutex);
    inner = g_inner; fov = g_baselineFov; aspect = g_baselineAspect;
    return inner && g_hasBaseline;
}

void Enter(SafetyHookContext& context) {
    const auto inner = static_cast<std::uintptr_t>(context.rax);
    if (!inner) return;
    const auto snapshot = Capture(inner);
    std::lock_guard lock(g_mutex);
    g_inner = inner; g_enterSnapshot = snapshot; g_baselineFov = snapshot.fov; g_baselineAspect = snapshot.aspect;
    g_hasBaseline = snapshot.readable;
    if (g_logger) g_logger->info("TRACE seq={} stage=enter-snapshot inner=0x{:X} readable={} fov={} aspect={} flags=0x{:02X} thread={}",
        ++g_sequence, inner, snapshot.readable, snapshot.fov, snapshot.aspect, snapshot.flags, GetCurrentThreadId());
}

#if !defined(STALKER2_TIMING_ONLY)
void Exit(SafetyHookContext& context) {
    std::uintptr_t callbackContext = static_cast<std::uintptr_t>(context.rcx), inner{};
    if (!Read(callbackContext + 0xF8, inner) || !inner) return;
    const auto snapshot = Capture(inner);
    std::lock_guard lock(g_mutex);
    if (g_logger) g_logger->info("TRACE seq={} stage=exit-snapshot inner=0x{:X} readable={} fov={} aspect={} flags=0x{:02X} thread={}",
        ++g_sequence, inner, snapshot.readable, snapshot.fov, snapshot.aspect, snapshot.flags, GetCurrentThreadId());
    g_inner = 0; g_hasBaseline = false;
}
#endif

void SnapshotMark(const char* stage) {
    std::uintptr_t inner{}; float unusedFov{}, unusedAspect{};
    if (!Current(inner, unusedFov, unusedAspect)) { if (g_logger) g_logger->warn("TRACE stage={} rejected=no-active-target", stage); return; }
    Snapshot baseline{};
    { std::lock_guard lock(g_mutex); baseline = g_enterSnapshot; }
    LogSnapshot(stage, inner, baseline, Capture(inner), true);
}

#if !defined(STALKER2_TIMING_ONLY)
void Apply(const char* stage, bool fovWrite, float fov, bool aspectWrite, float aspect) {
    std::uintptr_t inner{}; float baselineFov{}, baselineAspect{};
    if (!Current(inner, baselineFov, baselineAspect)) { if (g_logger) g_logger->warn("TRACE stage={} rejected=no-active-target", stage); return; }
    const auto before = Capture(inner);
    const bool fovOk = !fovWrite || Write(inner + kFovOffset, fov);
    const bool aspectOk = !aspectWrite || Write(inner + kAspectOffset, aspect);
    const auto after = Capture(inner);
    g_logger->info("TRACE seq={} stage={} inner=0x{:X} beforeFov={} beforeAspect={} afterFov={} afterAspect={} fovWritten={} aspectWritten={} thread={}",
        ++g_sequence, stage, inner, before.fov, before.aspect, after.fov, after.aspect, fovOk, aspectOk, GetCurrentThreadId());
}
#endif

void ExecuteCommand(int command) {
    switch (command) {
#if defined(STALKER2_TIMING_ONLY)
    case 7: SnapshotMark("manual-snapshot"); break;
#else
    case 1: SnapshotMark("manual-mark-ready"); break;
    case 2: SnapshotMark("manual-before-working-write"); break;
    case 3: Apply("manual-fov-then-aspect", true, kManualFov, true, kWideAspect); SnapshotMark("manual-after-working-write"); break;
    case 4: Apply("manual-aspect-then-fov", false, 0.0f, true, kWideAspect); Apply("manual-aspect-then-fov-step2", true, kManualFov, false, 0.0f); SnapshotMark("manual-after-working-write"); break;
    case 5: Apply("manual-native-aspect", false, 0.0f, true, kNativeAspect); break;
    case 6: { std::uintptr_t inner{}; float fov{}, aspect{}; if (Current(inner, fov, aspect)) Apply("manual-restore", true, fov, true, aspect); break; }
    case 7: SnapshotMark("manual-snapshot"); break;
    case 8: { std::uintptr_t inner{}; float fov{}, aspect{}; if (Current(inner, fov, aspect)) Apply("manual-baseline-fov-wide-aspect", true, fov, true, kWideAspect); break; }
#endif
    default: break;
    }
}

DWORD WINAPI CommandWorker(void*) {
    while (g_commandEvent) {
        if (WaitForSingleObject(g_commandEvent, INFINITE) != WAIT_OBJECT_0) break;
        const auto command = g_pendingCommand.exchange(0, std::memory_order_acq_rel);
        if (!command) continue;
        if (g_logger) g_logger->info("TRACE manual-command key=F{} received=true thread={}", command, GetCurrentThreadId());
        ExecuteCommand(command);
    }
    return 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (event && !(event->flags & LLKHF_UP) && event->vkCode >= VK_F1 && event->vkCode <= VK_F8) {
            g_pendingCommand.store(static_cast<int>(event->vkCode - VK_F1 + 1), std::memory_order_release);
            if (g_commandEvent) SetEvent(g_commandEvent);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

DWORD WINAPI KeyboardHookThread(void*) {
    const auto hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_module, 0);
    if (g_logger) g_logger->info("TRACE keyboard-hook-installed={} error={}", hook != nullptr, hook ? 0 : GetLastError());
    if (!hook) return 0;
    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_QUIT) break;
    }
    UnhookWindowsHookEx(hook);
    return 0;
}

DWORD WINAPI Initialize(void*) {
    WCHAR modulePath[MAX_PATH]{}; GetModuleFileNameW(g_module, modulePath, MAX_PATH);
    const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2CinematicCombinedTransitionManualProbe204.log";
    std::ofstream(path, std::ios::trunc).close();
    try {
        g_logger = spdlog::basic_logger_mt("STALKER2CinematicCombinedTransitionManualProbe204", path.string(), true);
        g_logger->flush_on(spdlog::level::info);
        const auto matches = Memory::PatternScanAll(g_executable, "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
        if (matches.size() != 1) throw std::runtime_error("ENTER signature is ambiguous or missing");
        auto* setter = matches.front() + kSetterOffset;
        if (std::memcmp(setter, kSetterBytes, sizeof(kSetterBytes)) != 0) throw std::runtime_error("ENTER bytes mismatch");
        g_enterHook = safetyhook::create_mid(setter, Enter);
#if !defined(STALKER2_TIMING_ONLY)
        auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitRva;
        if (std::memcmp(exit, kExitBytes, sizeof(kExitBytes)) != 0) throw std::runtime_error("EXIT bytes mismatch");
        g_exitHook = safetyhook::create_mid(exit, Exit);
#endif
        if (!g_enterHook
#if !defined(STALKER2_TIMING_ONLY)
            || !g_exitHook
#endif
            ) throw std::runtime_error("hook creation failed");
        g_commandEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!g_commandEvent) throw std::runtime_error("command event creation failed");
        const auto worker = CreateThread(nullptr, 0, CommandWorker, nullptr, 0, nullptr);
        if (worker) CloseHandle(worker); else throw std::runtime_error("command worker creation failed");
        const auto keyboard = CreateThread(nullptr, 0, KeyboardHookThread, nullptr, 0, nullptr);
        if (keyboard) CloseHandle(keyboard); else throw std::runtime_error("keyboard hook thread creation failed");
#if defined(STALKER2_TIMING_ONLY)
        g_logger->info("TRACE installed: 2.0.4 F7 timing probe; ENTER snapshot and F7 read-only only.");
#else
        g_logger->info("TRACE installed: 2.0.4 combined transition/manual probe; keyboard hook F1-F8; one ENTER hook; explicit writes only.");
#endif
    } catch (const std::exception& exception) {
        if (g_logger) g_logger->error("TRACE setup refused: {}", exception.what());
#if !defined(STALKER2_TIMING_ONLY)
        g_exitHook.reset();
#endif
        g_enterHook.reset();
        if (g_commandEvent) { CloseHandle(g_commandEvent); g_commandEvent = nullptr; }
    }
    return 0;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module; DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
