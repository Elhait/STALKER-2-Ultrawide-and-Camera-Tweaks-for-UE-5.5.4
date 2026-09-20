#include "stdafx.h"
#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    constexpr char kEntrySignature[] =
        "F3 0F 10 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 89 F1 E8 ?? ?? ?? ?? 48 89 C1 31 D2 E8";
    constexpr std::uintptr_t kGlobalFovRva = 0x9EDE50C;
    constexpr std::size_t kLoadedFovCallOffset = 8;
    constexpr std::uint64_t kMaxLogRecords = 20000;
    constexpr std::size_t kSetterOffset = 0x19;
    constexpr std::size_t kSetterLength = 10;
    constexpr std::uint8_t kLetterboxSignatureA[] = {
        0x48, 0x8B, 0x41, 0x18, 0x48, 0x8B, 0x88, 0xF8, 0x00, 0x00, 0x00,
        0x80, 0x89, 0x59, 0x02, 0x00, 0x00, 0x01, 0x48, 0x8B, 0x80, 0xF8,
        0x00, 0x00, 0x00, 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E,
        0xE3, 0x3F, 0xC3,
    };
    constexpr std::uint8_t kLetterboxSignatureB[] = {
        0x48, 0x8B, 0x41, 0x18, 0x48, 0x8B, 0x88, 0xF8, 0x00, 0x00, 0x00,
        0x80, 0x89, 0x59, 0x02, 0x00, 0x00, 0x01, 0x48, 0x8B, 0x80, 0xF8,
        0x00, 0x00, 0x00, 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E,
        0xE3, 0x3F, 0xB0, 0x01, 0xC3,
    };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_entryHook;
    SafetyHookMid g_writerHook;
    SafetyHookMid g_setterAHook;
    SafetyHookMid g_setterBHook;
    std::atomic<std::uint64_t> g_sequence{0};
    std::atomic<bool> g_markerRunning{false};
    std::atomic<std::uint64_t> g_setterAHits{0};
    std::atomic<std::uint64_t> g_setterBHits{0};
    std::atomic<std::uint64_t> g_lastSetterASequence{0};
    std::atomic<std::uint64_t> g_lastSetterBSequence{0};

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT) return false;
        const auto protect = info.Protect & 0xff;
        return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
            protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
    }

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

    std::uintptr_t CallerFromContext(SafetyHookContext& context)
    {
        std::uintptr_t caller{};
        SafeRead(static_cast<std::uintptr_t>(context.rsp), caller);
        return caller;
    }

    void MarkerLoop()
    {
        static constexpr std::array<std::pair<int, const char*>, 5> markers{{
            {VK_F7, "cutscene-active"}, {VK_F8, "cutscene-ended"},
            {VK_F9, "weapon-fov-before-ads"}, {VK_F10, "weapon-fov-after-ads"},
            {VK_F11, "pause-refresh"}
        }};
        static std::array<bool, markers.size()> previous{};
        while (g_markerRunning.load(std::memory_order_relaxed)) {
            const auto sequence = g_sequence.load(std::memory_order_relaxed);
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const bool down = (GetAsyncKeyState(markers[i].first) & 0x8000) != 0;
                if (down && !previous[i] && g_logger)
                    g_logger->info("MARKER seq={} event={}", sequence, markers[i].second);
                previous[i] = down;
            }
            Sleep(10);
        }
    }

    void TraceLoadedFov(SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        if (!g_logger || sequence > kMaxLogRecords) return;
        const auto global = reinterpret_cast<std::uintptr_t>(g_executable) + kGlobalFovRva;
        float globalValue{};
        if (!SafeRead(global, globalValue)) globalValue = std::numeric_limits<float>::quiet_NaN();
        g_logger->info("TRACE seq={} stage=cinematic-entry-call globalFov={} xmm0={} caller=0x{:X} A_hits={} A_last={} B_hits={} B_last={}",
            sequence, globalValue, context.xmm0.f32[0], CallerFromContext(context),
            g_setterAHits.load(std::memory_order_relaxed),
            g_lastSetterASequence.load(std::memory_order_relaxed),
            g_setterBHits.load(std::memory_order_relaxed),
            g_lastSetterBSequence.load(std::memory_order_relaxed));
    }

    void TraceGlobalWriter(SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        if (!g_logger || sequence > kMaxLogRecords) return;
        const auto global = reinterpret_cast<std::uintptr_t>(g_executable) + kGlobalFovRva;
        float before{};
        if (!SafeRead(global, before)) before = std::numeric_limits<float>::quiet_NaN();
        g_logger->info("TRACE seq={} stage=global-writer previous={} xmm0={} caller=0x{:X} A_hits={} A_last={} B_hits={} B_last={}",
            sequence, before, context.xmm0.f32[0], CallerFromContext(context),
            g_setterAHits.load(std::memory_order_relaxed),
            g_lastSetterASequence.load(std::memory_order_relaxed),
            g_setterBHits.load(std::memory_order_relaxed),
            g_lastSetterBSequence.load(std::memory_order_relaxed));
    }

    void TraceSetterA(SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        g_setterAHits.fetch_add(1, std::memory_order_relaxed);
        g_lastSetterASequence.store(sequence, std::memory_order_relaxed);
        if (g_logger && sequence <= kMaxLogRecords)
            g_logger->info("TRACE seq={} stage=letterbox-setter-A object=0x{:X} A_hits={} B_hits={}",
                sequence, static_cast<std::uintptr_t>(context.rax),
                g_setterAHits.load(std::memory_order_relaxed),
                g_setterBHits.load(std::memory_order_relaxed));
    }

    void TraceSetterB(SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        g_setterBHits.fetch_add(1, std::memory_order_relaxed);
        g_lastSetterBSequence.store(sequence, std::memory_order_relaxed);
        if (g_logger && sequence <= kMaxLogRecords)
            g_logger->info("TRACE seq={} stage=letterbox-setter-B object=0x{:X} A_hits={} B_hits={}",
                sequence, static_cast<std::uintptr_t>(context.rax),
                g_setterAHits.load(std::memory_order_relaxed),
                g_setterBHits.load(std::memory_order_relaxed));
    }

    bool FindUniqueExact(const std::uint8_t* sectionStart, std::size_t sectionSize,
        const std::uint8_t* signature, std::size_t signatureSize, std::uint8_t*& match)
    {
        std::size_t matches = 0;
        if (sectionSize < signatureSize) return false;
        for (std::size_t offset = 0; offset <= sectionSize - signatureSize; ++offset) {
            if (std::memcmp(sectionStart + offset, signature, signatureSize) == 0) {
                match = const_cast<std::uint8_t*>(sectionStart + offset);
                ++matches;
            }
        }
        return matches == 1;
    }

    std::uint8_t* ResolveSetter(const std::uint8_t* signature, std::size_t signatureSize)
    {
        std::uint8_t* setter = nullptr;
        Memory::ForEachExecutableSection(g_executable, [&](std::uint8_t* start, std::size_t size) {
            if (setter) return;
            std::uint8_t* match = nullptr;
            if (!FindUniqueExact(start, size, signature, signatureSize, match)) return;
            auto* candidate = match + kSetterOffset;
            if (candidate + kSetterLength > match + signatureSize || candidate[0] != 0xC7 ||
                candidate[1] != 0x80 || candidate[6] != 0x39 || candidate[7] != 0x8E ||
                candidate[8] != 0xE3 || candidate[9] != 0x3F)
                return;
            setter = candidate;
        });
        return setter;
    }

    std::vector<std::uint8_t*> ResolveEntry()
    {
        const auto pattern = Memory::PatternScanAll(g_executable, kEntrySignature);
        if (pattern.size() != 1) return {};
        return pattern;
    }

    std::uint8_t* ResolveGlobalWriter()
    {
        std::vector<std::uint8_t*> matches;
        Memory::ForEachExecutableSection(g_executable, [&](std::uint8_t* start, std::size_t size) {
            for (std::size_t i = 0; i + 8 <= size; ++i) {
                if (start[i] != 0xF3 || start[i + 1] != 0x0F || start[i + 2] != 0x11 || start[i + 3] != 0x05)
                    continue;
                std::int32_t displacement{};
                std::memcpy(&displacement, start + i + 4, sizeof(displacement));
                const auto target = reinterpret_cast<std::uintptr_t>(start + i + 8) + displacement;
                const auto expected = reinterpret_cast<std::uintptr_t>(g_executable) + kGlobalFovRva;
                if (target == expected) matches.push_back(start + i);
            }
        });
        return matches.size() == 1 ? matches.front() : nullptr;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CutsceneFovSemanticsTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CutsceneFovSemanticsTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            const auto entries = ResolveEntry();
            auto* entry = entries.empty() ? nullptr : entries.front();
            auto* writer = ResolveGlobalWriter();
            auto* setterA = ResolveSetter(kLetterboxSignatureA, sizeof(kLetterboxSignatureA));
            auto* setterB = ResolveSetter(kLetterboxSignatureB, sizeof(kLetterboxSignatureB));
            if (!entry || !writer || !setterA || !setterB ||
                !IsExecutable(reinterpret_cast<std::uintptr_t>(entry + kLoadedFovCallOffset)) ||
                !IsExecutable(reinterpret_cast<std::uintptr_t>(setterA)) ||
                !IsExecutable(reinterpret_cast<std::uintptr_t>(setterB)) ||
                !IsExecutable(reinterpret_cast<std::uintptr_t>(writer)))
                throw std::runtime_error("unique cinematic FOV entry/writer/A/B resolution failed");

            g_entryHook = safetyhook::create_mid(entry + kLoadedFovCallOffset, TraceLoadedFov);
            if (!g_entryHook) throw std::runtime_error("cinematic entry hook creation failed");
            g_writerHook = safetyhook::create_mid(writer, TraceGlobalWriter);
            if (!g_writerHook) throw std::runtime_error("global FOV writer hook creation failed");
            g_setterAHook = safetyhook::create_mid(setterA, TraceSetterA);
            if (!g_setterAHook) throw std::runtime_error("letterbox A hook creation failed");
            g_setterBHook = safetyhook::create_mid(setterB, TraceSetterB);
            if (!g_setterBHook) throw std::runtime_error("letterbox B hook creation failed");
            g_markerRunning.store(true, std::memory_order_relaxed);
            const auto markerThread = CreateThread(nullptr, 0,
                [](LPVOID) -> DWORD { MarkerLoop(); return 0; }, nullptr, 0, nullptr);
            if (!markerThread) throw std::runtime_error("marker polling thread creation failed");
            CloseHandle(markerThread);
            g_logger->info("TRACE installed: cinematic entry, global writer and A/B setters; markers F7=cutscene-active F8=cutscene-ended F9=weapon-fov-before-ads F10=weapon-fov-after-ads F11=pause-refresh.");
        } catch (const std::exception& error) {
            g_markerRunning.store(false, std::memory_order_relaxed);
            g_setterBHook.reset();
            g_setterAHook.reset();
            g_entryHook.reset();
            g_writerHook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: {}", error.what());
        } catch (...) {
            g_markerRunning.store(false, std::memory_order_relaxed);
            g_setterBHook.reset();
            g_setterAHook.reset();
            g_entryHook.reset();
            g_writerHook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: unknown error.");
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH) {
        g_markerRunning.store(false, std::memory_order_relaxed);
        return TRUE;
    }
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
    return TRUE;
}
