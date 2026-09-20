#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <array>
#include <algorithm>
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
constexpr std::uint8_t kSetterBytes[] = {
    0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F
};
constexpr std::uintptr_t kFovOffset = 0x230;
constexpr std::uintptr_t kAspectOffset = 0x254;
constexpr std::uintptr_t kFlagsOffset = 0x259;
constexpr std::uintptr_t kStateBegin = 0x200;
constexpr std::uintptr_t kStateEnd = 0x2D0;
constexpr std::uintptr_t kExitRva = 0x6B6C482;
constexpr std::uint8_t kExitBytes[] = {
    0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE
};

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
SafetyHookMid g_exitHook;
std::mutex g_mutex;
std::uintptr_t g_inner{};
Snapshot g_enterSnapshot{};
bool g_hasEnterSnapshot{};
std::uint64_t g_sequence{};

template <typename T>
bool Read(std::uintptr_t address, T& value) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (address >= regionEnd || sizeof(T) > regionEnd - address) return false;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

std::uintptr_t ToRva(std::uintptr_t address) {
    const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
    return address >= base ? address - base : 0;
}

Snapshot Capture(std::uintptr_t inner) {
    Snapshot snapshot{};
    if (!inner) return snapshot;
    snapshot.readable = Read(inner + kStateBegin, snapshot.bytes);
    const bool fields = Read(inner + kFovOffset, snapshot.fov) &&
        Read(inner + kAspectOffset, snapshot.aspect) && Read(inner + kFlagsOffset, snapshot.flags);
    snapshot.readable = snapshot.readable && fields;
    return snapshot;
}

void LogSnapshot(const char* stage, std::uintptr_t inner, const Snapshot& before,
                 const Snapshot& current, bool includeChanges) {
    if (!g_logger) return;
    std::size_t changedBytes{};
    if (includeChanges && before.readable && current.readable) {
        for (std::size_t index = 0; index < current.bytes.size(); ++index)
            changedBytes += before.bytes[index] != current.bytes[index] ? 1u : 0u;
    }
    g_logger->info(
        "TRACE seq={} stage={} inner=0x{:X} readable={} fov={} aspect={} flags=0x{:02X} "
        "changedBytes={} includeChanges={} thread={}",
        ++g_sequence, stage, inner, current.readable, current.fov, current.aspect, current.flags,
        changedBytes,
        includeChanges, GetCurrentThreadId());

    if (!includeChanges || !before.readable || !current.readable) return;
    for (std::size_t index = 0; index < current.bytes.size(); ++index) {
        if (before.bytes[index] == current.bytes[index]) continue;
        g_logger->info("TRACE change stage={} innerOffset=0x{:X} old=0x{:02X} new=0x{:02X}",
            stage, kStateBegin + index, before.bytes[index], current.bytes[index]);
    }
}

bool CurrentInner(std::uintptr_t& inner) {
    std::lock_guard lock(g_mutex);
    inner = g_inner;
    return inner != 0;
}

void Enter(SafetyHookContext& context) {
    const auto inner = static_cast<std::uintptr_t>(context.rax);
    if (!inner) return;
    const auto snapshot = Capture(inner);
    std::lock_guard lock(g_mutex);
    g_inner = inner;
    g_enterSnapshot = snapshot;
    g_hasEnterSnapshot = snapshot.readable;
    if (g_logger) {
        g_logger->info("TRACE seq={} stage=enter-snapshot inner=0x{:X} readable={} fov={} aspect={} flags=0x{:02X} thread={}",
            ++g_sequence, inner, snapshot.readable, snapshot.fov, snapshot.aspect, snapshot.flags,
            GetCurrentThreadId());
    }
}

void Exit(SafetyHookContext& context) {
    std::uintptr_t callbackContext = static_cast<std::uintptr_t>(context.rcx);
    std::uintptr_t inner{};
    if (!Read(callbackContext + 0xF8, inner) || !inner) return;
    const auto snapshot = Capture(inner);
    std::lock_guard lock(g_mutex);
    if (g_logger) {
        g_logger->info("TRACE seq={} stage=exit-snapshot inner=0x{:X} readable={} fov={} aspect={} flags=0x{:02X} thread={}",
            ++g_sequence, inner, snapshot.readable, snapshot.fov, snapshot.aspect, snapshot.flags,
            GetCurrentThreadId());
    }
    g_inner = 0;
    g_hasEnterSnapshot = false;
}

void Mark(const char* stage) {
    std::uintptr_t inner{};
    if (!CurrentInner(inner)) {
        if (g_logger) g_logger->warn("TRACE stage={} rejected=no-captured-inner", stage);
        return;
    }
    const auto current = Capture(inner);
    Snapshot before{};
    bool hasBefore{};
    {
        std::lock_guard lock(g_mutex);
        before = g_enterSnapshot;
        hasBefore = g_hasEnterSnapshot;
    }
    LogSnapshot(stage, inner, before, current, hasBefore);
}

void HotkeyLoop() {
    RegisterHotKey(nullptr, 1, MOD_NOREPEAT, VK_F9);
    RegisterHotKey(nullptr, 2, MOD_NOREPEAT, VK_F10);
    RegisterHotKey(nullptr, 3, MOD_NOREPEAT, VK_F11);
    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        if (message.message != WM_HOTKEY) continue;
        switch (static_cast<int>(message.wParam)) {
        case 1: Mark("manual-mark-ready"); break;
        case 2: Mark("manual-before-working-write"); break;
        case 3: Mark("manual-after-working-write"); break;
        default: break;
        }
    }
}

DWORD WINAPI Initialize(void*) {
    WCHAR modulePath[MAX_PATH]{};
    GetModuleFileNameW(g_module, modulePath, MAX_PATH);
    const auto path = std::filesystem::path(modulePath).remove_filename() /
        "STALKER2CinematicStateTransitionProbe204.log";
    std::ofstream(path, std::ios::trunc).close();
    try {
        g_logger = spdlog::basic_logger_mt("STALKER2CinematicStateTransitionProbe204", path.string(), true);
        g_logger->flush_on(spdlog::level::info);
        const auto matches = Memory::PatternScanAll(g_executable,
            "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
        if (matches.size() != 1) throw std::runtime_error("ENTER signature is ambiguous or missing");
        auto* setter = matches.front() + kSetterOffset;
        if (std::memcmp(setter, kSetterBytes, sizeof(kSetterBytes)) != 0)
            throw std::runtime_error("ENTER bytes mismatch");
        auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitRva;
        if (std::memcmp(exit, kExitBytes, sizeof(kExitBytes)) != 0)
            throw std::runtime_error("EXIT bytes mismatch");
        g_enterHook = safetyhook::create_mid(setter, Enter);
        g_exitHook = safetyhook::create_mid(exit, Exit);
        if (!g_enterHook || !g_exitHook) throw std::runtime_error("hook creation failed");
        g_logger->info("TRACE installed: 2.0.4 read-only state transition probe; F9=MARK, F10=BEFORE, F11=AFTER.");
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
