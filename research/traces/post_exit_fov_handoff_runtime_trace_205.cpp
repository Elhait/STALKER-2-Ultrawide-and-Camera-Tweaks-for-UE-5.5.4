#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace
{
    constexpr std::uint8_t kCameraWriterSignature[] = {
        0xF6, 0x86, 0x62, 0x02, 0x00, 0x00, 0x10,
        0xF3, 0x0F, 0x10, 0x86, 0x30, 0x02, 0x00, 0x00,
        0x0F, 0x85, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x4B, 0x30,
        0xF3, 0x0F, 0x11, 0x43, 0x30,
        0xF3, 0x0F, 0x10, 0x86, 0x54, 0x02, 0x00, 0x00,
        0xF3, 0x0F, 0x11, 0x43, 0x5C,
        0x0F, 0xB6, 0x96, 0x59, 0x02, 0x00, 0x00,
        0x8B, 0x43, 0x68, 0x83, 0xE2, 0x01, 0x83, 0xE0, 0xFE,
        0x09, 0xD0, 0x89, 0x43, 0x68,
        0x0F, 0xB6, 0x96, 0x59, 0x02, 0x00, 0x00,
        0x83, 0xE2, 0x04, 0x83, 0xE0, 0xFB, 0x09, 0xD0, 0x89,
        0x43, 0x68, 0x8A, 0x96, 0x63, 0x02, 0x00, 0x00, 0x88,
        0x53, 0x6C,
    };
    constexpr char kExitIndexedSignature[] =
        "40 0F B6 C7 F3 0F 10 44 83 38 E8 ?? ?? ?? ?? "
        "48 89 F1 E8 ?? ?? ?? ?? 48 89 C1 31 D2 E8 ?? ?? ?? ?? "
        "48 85 C0 74 ?? 48 89 C7 48 8B 00 48 89 F9 "
        "FF 90 80 08 00 00 48 8B 07 48 89 F9 FF 90 68 08 00 00";
    constexpr std::size_t kWriterHookOffset = 25;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_writerHook;
    SafetyHookMid g_exitConsumerHook;
    std::atomic<std::uint64_t> g_sequence{0};
    std::atomic<bool> g_postExitArmed{false};
    std::atomic<std::uint32_t> g_postExitWriterCount{0};

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

    template <typename... Args>
    void Log(Args&&... args)
    {
        if (!g_logger) return;
        std::ostringstream message;
        (message << ... << args);
        g_logger->info("{}", message.str());
    }

    std::size_t CountWriterMatches()
    {
        std::size_t matches = 0;
        Memory::ForEachExecutableSection(g_executable, [&](std::uint8_t* start, std::size_t size) {
            if (size < sizeof(kCameraWriterSignature)) return;
            for (std::size_t offset = 0; offset <= size - sizeof(kCameraWriterSignature); ++offset) {
                bool equal = true;
                for (std::size_t i = 0; i < sizeof(kCameraWriterSignature); ++i) {
                    if ((i < 17 || i >= 21) && start[offset + i] != kCameraWriterSignature[i]) {
                        equal = false;
                        break;
                    }
                }
                if (equal) ++matches;
            }
        });
        return matches;
    }

    void TraceExitBoundary(SafetyHookContext& context)
    {
        const auto seq = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        g_postExitWriterCount.store(0, std::memory_order_release);
        g_postExitArmed.store(true, std::memory_order_release);
        Log("TRACE seq=", seq, " stage=EXIT_BOUNDARY_ENTRY boundary=0x366FA2D",
            " rbx=0x", std::hex, static_cast<std::uintptr_t>(context.rbx),
            " rsi=0x", static_cast<std::uintptr_t>(context.rsi),
            " rdi=0x", static_cast<std::uintptr_t>(context.rdi), std::dec,
            " dil=", static_cast<unsigned int>(context.rdi & 0xff),
            " thread=", GetCurrentThreadId());
    }

    void TraceWriter(SafetyHookContext& context)
    {
        if (!g_postExitArmed.load(std::memory_order_acquire) || !g_logger) return;
        const auto ordinal = g_postExitWriterCount.fetch_add(1, std::memory_order_acq_rel) + 1;
        const auto source = static_cast<std::uintptr_t>(context.rsi);
        float cameraFov{};
        float firstPersonFov{};
        float aspect{};
        std::uint8_t flags{};
        const bool readable = SafeRead(source + 0x230, cameraFov) &&
            SafeRead(source + 0x234, firstPersonFov) && SafeRead(source + 0x254, aspect) &&
            SafeRead(source + 0x259, flags);
        Log("TRACE seq=", g_sequence.fetch_add(1, std::memory_order_relaxed) + 1,
            " stage=POST_EXIT_WRITER ordinal=", ordinal,
            " source=0x", std::hex, source, " output=0x", static_cast<std::uintptr_t>(context.rbx),
            std::dec, " xmm0=", context.xmm0.f32[0], " cameraFov=", cameraFov,
            " firstPersonFov=", firstPersonFov, " aspect=", aspect,
            " flags=0x", std::hex, static_cast<int>(flags), std::dec,
            " readable=", readable, " thread=", GetCurrentThreadId());
        if (ordinal >= 12) g_postExitArmed.store(false, std::memory_order_release);
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2PostExitFovHandoffRuntimeTrace205.log";
        std::ofstream(path, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2PostExitFovHandoffRuntimeTrace205", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            const auto writerMatches = CountWriterMatches();
            const auto exitMatches = Memory::PatternScanAll(g_executable, kExitIndexedSignature);
            Log("TRACE init: executable=2.0.5 writerMatches=", writerMatches,
                " exitIndexedMatches=", exitMatches.size(), " readOnly=true.");
            if (writerMatches != 1 || exitMatches.size() != 1)
                throw std::runtime_error("current writer/EXIT signature was not unique");
            auto* writer = Memory::PatternScan(g_executable,
                "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30");
            if (!writer) throw std::runtime_error("writer signature resolution failed");
            g_writerHook = safetyhook::create_mid(writer + kWriterHookOffset, TraceWriter);
            // Hook the first instruction of the validated boundary. Do not
            // intercept its internal CALL or interpret RBX as a camera object.
            g_exitConsumerHook = safetyhook::create_mid(exitMatches.front(), TraceExitBoundary);
            if (!g_writerHook || !g_exitConsumerHook) throw std::runtime_error("hook creation failed");
            Log("TRACE installed: current 2.0.5 EXIT consumer + writer; first 12 post-EXIT writer hits only.");
        } catch (const std::exception& error) {
            g_exitConsumerHook.reset();
            g_writerHook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: ", error.what());
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
