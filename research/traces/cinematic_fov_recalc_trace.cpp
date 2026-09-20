#include "stdafx.h"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <bit>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>

namespace
{
    constexpr std::uintptr_t kCinematicEntryRva = 0x7267672;
    constexpr std::uintptr_t kCandidateEntryRva = 0x559BB2C;
    constexpr std::uintptr_t kRecalcEntryRva = 0xAF4F4A;

    constexpr std::uint8_t kCinematicEntryBytes[] = { 0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x56, 0x57, 0x53 };
    constexpr std::uint8_t kCandidateEntryBytes[] = { 0x56, 0x48, 0x83, 0xEC, 0x20 };
    constexpr std::uint8_t kRecalcEntryBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40 };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_cinematicEntryHook;
    SafetyHookMid g_candidateHook;
    SafetyHookMid g_recalcHook;
    std::atomic<std::uint64_t> g_sequence{0};
    thread_local std::uint64_t g_activeSequence{};
    thread_local std::uintptr_t g_candidateObject{};

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address > end || sizeof(value) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
        return true;
    }

    bool ReadResolution(LONG& clientWidth, LONG& clientHeight)
    {
        HWND window = nullptr;
        EnumWindows([](HWND candidate, LPARAM parameter) {
            DWORD processId = 0;
            GetWindowThreadProcessId(candidate, &processId);
            if (processId == GetCurrentProcessId() && IsWindowVisible(candidate) && GetWindow(candidate, GW_OWNER) == nullptr) {
                *reinterpret_cast<HWND*>(parameter) = candidate;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&window));

        RECT rect{};
        if (!window || !GetClientRect(window, &rect)) return false;
        clientWidth = rect.right - rect.left;
        clientHeight = rect.bottom - rect.top;
        return true;
    }

    void LogCinematicEntry(SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto parameter = static_cast<std::uintptr_t>(context.r8);
        std::uint8_t active = 0;
        float value6C = 0.0f;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        const bool readable = parameter &&
            SafeRead(parameter + 0x70, active) &&
            SafeRead(parameter + 0x6C, value6C) &&
            SafeRead(parameter + 0x30, width) &&
            SafeRead(parameter + 0x34, height);
        if (!readable || active == 0) {
            g_activeSequence = 0;
            g_candidateObject = 0;
            if (g_logger) g_logger->info(
                "TRACE seq={} stage=FUN_147267672-entry param3=0x{:X} readable={} active=0x{:02X} branch=inactive",
                sequence, parameter, readable, active);
            return;
        }

        g_activeSequence = sequence;
        g_candidateObject = 0;
        LONG clientWidth = 0;
        LONG clientHeight = 0;
        ReadResolution(clientWidth, clientHeight);
        if (g_logger) g_logger->info(
            "TRACE seq={} stage=FUN_147267672-entry param3=0x{:X} readable=true active=0x{:02X} branch=active value6C={} width={} height={} propertySemantics=unresolved client={}x{}",
            sequence, parameter, active, value6C, width, height,
            clientWidth, clientHeight);
    }

    void LogCandidate(SafetyHookContext& context)
    {
        if (!g_activeSequence || !g_logger) return;
        const auto object = static_cast<std::uintptr_t>(context.rcx);
        float fov = 0.0f;
        float scale = 0.0f;
        float derived9d0 = 0.0f;
        float derived9d4 = 0.0f;
        float derived9d8 = 0.0f;
        if (!SafeRead(object + 0x30, fov) || !SafeRead(object + 0x40, scale) ||
            !SafeRead(object + 0x9D0, derived9d0) || !SafeRead(object + 0x9D4, derived9d4) ||
            !SafeRead(object + 0x9D8, derived9d8)) return;

        g_candidateObject = object;
        g_logger->info(
            "TRACE seq={} stage=FUN_14559BB2C-pre object=0x{:X} fov30={} scale40={} state9D0={} state9D4={} state9D8={}",
            g_activeSequence, object, fov, scale, derived9d0, derived9d4, derived9d8);
    }

    void LogRecalc(SafetyHookContext& context)
    {
        if (!g_activeSequence || !g_candidateObject || !g_logger) return;
        const auto object = static_cast<std::uintptr_t>(context.rcx);
        if (object != g_candidateObject) return;

        float fov = 0.0f;
        float scale = 0.0f;
        float derived9d0 = 0.0f;
        float derived9d4 = 0.0f;
        float derived9d8 = 0.0f;
        std::uint32_t param2Bits = context.xmm1.u32[0];
        LONG clientWidth = 0;
        LONG clientHeight = 0;
        const bool readable = SafeRead(object + 0x30, fov) && SafeRead(object + 0x40, scale) &&
            SafeRead(object + 0x9D0, derived9d0) && SafeRead(object + 0x9D4, derived9d4) &&
            SafeRead(object + 0x9D8, derived9d8);
        if (!readable) return;
        ReadResolution(clientWidth, clientHeight);

        g_logger->info(
            "TRACE seq={} stage=FUN_140AF4F4A-pre object=0x{:X} param2={} param2Bits=0x{:08X} r8b=0x{:02X} r9b=0x{:02X} fov30={} scale40={} state9D0={} state9D4={} state9D8={} client={}x{}",
            g_activeSequence, object, context.xmm1.f32[0], param2Bits,
            static_cast<std::uint64_t>(context.r8) & 0xFF,
            static_cast<std::uint64_t>(context.r9) & 0xFF,
            fov, scale, derived9d0, derived9d4, derived9d8, clientWidth, clientHeight);
    }

    bool Validate(std::uintptr_t address, const std::uint8_t* bytes, std::size_t size)
    {
        return std::memcmp(reinterpret_cast<const void*>(address), bytes, size) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicFovRecalcTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicFovRecalcTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        const auto cinematicEntry = base + kCinematicEntryRva;
        const auto candidateEntry = base + kCandidateEntryRva;
        const auto recalcEntry = base + kRecalcEntryRva;
        if (!Validate(cinematicEntry, kCinematicEntryBytes, sizeof(kCinematicEntryBytes)) ||
            !Validate(candidateEntry, kCandidateEntryBytes, sizeof(kCandidateEntryBytes)) ||
            !Validate(recalcEntry, kRecalcEntryBytes, sizeof(kRecalcEntryBytes))) {
            g_logger->error("TRACE setup refused: 2.0.3 cinematic recalculation sites did not validate.");
            return 0;
        }

        try {
            g_cinematicEntryHook = safetyhook::create_mid(reinterpret_cast<void*>(cinematicEntry), LogCinematicEntry);
            g_candidateHook = safetyhook::create_mid(reinterpret_cast<void*>(candidateEntry), LogCandidate);
            g_recalcHook = safetyhook::create_mid(reinterpret_cast<void*>(recalcEntry), LogRecalc);
            if (!g_cinematicEntryHook || !g_candidateHook || !g_recalcHook)
                throw std::runtime_error("one or more diagnostic hooks were not created");
            g_logger->info("TRACE installed: FUN_147267672 entry, FUN_14559BB2C entry, FUN_140AF4F4A entry; read-only.");
        } catch (...) {
            g_recalcHook.reset();
            g_candidateHook.reset();
            g_cinematicEntryHook.reset();
            g_logger->error("TRACE setup refused safely; partial hooks rolled back.");
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
    return TRUE;
}
