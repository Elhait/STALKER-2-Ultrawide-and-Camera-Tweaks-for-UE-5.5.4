#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace
{
    constexpr std::uintptr_t kEnterSetterRva = 0x6B7CB05;
    constexpr std::uintptr_t kExitCallbackRva = 0x6B6C482;
    constexpr std::uintptr_t kCandidate187898Rva = 0x18826D;
    constexpr std::uintptr_t kCandidate38E7D0Rva = 0x392D9D;

    constexpr std::uint8_t kEnterSetterBytes[] = {
        0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F,
    };
    constexpr std::uint8_t kExitCallbackBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE,
    };
    constexpr std::uint8_t kCandidate187898Bytes[] = {
        0xF3, 0x0F, 0x11, 0xA3, 0x30, 0x02, 0x00, 0x00,
    };
    constexpr std::uint8_t kCandidate38E7D0Bytes[] = {
        0xF3, 0x0F, 0x11, 0x83, 0x30, 0x02, 0x00, 0x00,
    };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_enterHook;
    SafetyHookMid g_exitHook;
    SafetyHookMid g_candidate187898Hook;
    SafetyHookMid g_candidate38E7D0Hook;
    std::atomic<std::uintptr_t> g_inner{};
    std::atomic<DWORD> g_enterThread{};
    std::atomic<bool> g_windowActive{};
    std::atomic<std::uint64_t> g_sequence{};

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(T) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    std::uintptr_t Base(std::uintptr_t rva)
    {
        return reinterpret_cast<std::uintptr_t>(g_executable) + rva;
    }

    std::uintptr_t ToRva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base ? address - base : 0;
    }

    bool Validate(std::uintptr_t rva, const std::uint8_t* bytes, std::size_t size)
    {
        return std::memcmp(reinterpret_cast<const void*>(Base(rva)), bytes, size) == 0;
    }

    bool ReadState(std::uintptr_t inner, float& fov, float& aspect, std::uint8_t& flags)
    {
        return SafeRead(inner + 0x230, fov) &&
            SafeRead(inner + 0x254, aspect) &&
            SafeRead(inner + 0x259, flags);
    }

    void LogCandidate(const char* candidate, std::uintptr_t siteRva,
        std::uintptr_t object, float incoming, SafetyHookContext& context)
    {
        const auto inner = g_inner.load(std::memory_order_acquire);
        const auto thread = GetCurrentThreadId();
        float currentFov{};
        float aspect{};
        std::uint8_t flags{};
        const bool readable = ReadState(object, currentFov, aspect, flags);
        const bool sameInner = g_windowActive.load(std::memory_order_acquire) &&
            thread == g_enterThread.load(std::memory_order_acquire) && object == inner;
        if (!g_logger || !readable) return;

        const char* direction = incoming < currentFov ? "decreasing" :
            incoming > currentFov ? "increasing" : "unchanged";
        g_logger->info(
            "TRACE seq={} stage=candidate-hit candidate={} siteRva=0x{:X} object=0x{:X} inner=0x{:X} sameInner={} direction={} currentFov={} incomingFov={} aspect={} flags=0x{:02X} thread={} enterThread={} contextRva=0x{:X}",
            ++g_sequence, candidate, siteRva, object, inner, sameInner, direction,
            currentFov, incoming, aspect, flags, thread,
            g_enterThread.load(std::memory_order_relaxed), ToRva(context.rip));
    }

    void Enter(SafetyHookContext& context)
    {
        const auto inner = static_cast<std::uintptr_t>(context.rax);
        float fov{};
        float aspect{};
        std::uint8_t flags{};
        if (!inner || !ReadState(inner, fov, aspect, flags)) return;
        g_inner.store(inner, std::memory_order_release);
        g_enterThread.store(GetCurrentThreadId(), std::memory_order_release);
        g_windowActive.store(true, std::memory_order_release);
        g_logger->info(
            "TRACE seq={} stage=enter inner=0x{:X} fov={} aspect={} flags=0x{:02X} thread={}",
            ++g_sequence, inner, fov, aspect, flags, GetCurrentThreadId());
    }

    void Exit(SafetyHookContext&)
    {
        const auto inner = g_inner.load(std::memory_order_acquire);
        if (g_logger && inner) {
            g_logger->info("TRACE seq={} stage=exit inner=0x{:X} thread={}",
                ++g_sequence, inner, GetCurrentThreadId());
        }
        g_windowActive.store(false, std::memory_order_release);
        g_inner.store(0, std::memory_order_release);
        g_enterThread.store(0, std::memory_order_release);
    }

    void Candidate187898(SafetyHookContext& context)
    {
        LogCandidate("FUN_140187898", kCandidate187898Rva,
            static_cast<std::uintptr_t>(context.rbx), context.xmm4.f32[0], context);
    }

    void Candidate38E7D0(SafetyHookContext& context)
    {
        LogCandidate("FUN_14038E7D0", kCandidate38E7D0Rva,
            static_cast<std::uintptr_t>(context.rbx), context.xmm0.f32[0], context);
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicFovTransitionCandidateCorrelation204.log";
        std::ofstream(logPath, std::ios::trunc).close();

        try {
            g_logger = spdlog::basic_logger_mt(
                "STALKER2CinematicFovTransitionCandidateCorrelation204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);

            if (!Validate(kEnterSetterRva, kEnterSetterBytes, sizeof(kEnterSetterBytes)) ||
                !Validate(kExitCallbackRva, kExitCallbackBytes, sizeof(kExitCallbackBytes)) ||
                !Validate(kCandidate187898Rva, kCandidate187898Bytes, sizeof(kCandidate187898Bytes)) ||
                !Validate(kCandidate38E7D0Rva, kCandidate38E7D0Bytes, sizeof(kCandidate38E7D0Bytes))) {
                throw std::runtime_error("current 2.0.4 candidate bytes mismatch");
            }

            g_enterHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kEnterSetterRva)), Enter);
            g_exitHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kExitCallbackRva)), Exit);
            g_candidate187898Hook = safetyhook::create_mid(
                reinterpret_cast<void*>(Base(kCandidate187898Rva)), Candidate187898);
            g_candidate38E7D0Hook = safetyhook::create_mid(
                reinterpret_cast<void*>(Base(kCandidate38E7D0Rva)), Candidate38E7D0);
            if (!g_enterHook || !g_exitHook || !g_candidate187898Hook || !g_candidate38E7D0Hook)
                throw std::runtime_error("hook creation failed");

            g_logger->info(
                "TRACE installed: 2.0.4 two-candidate FOV transition correlation; read-only, no writes.");
        } catch (const std::exception& error) {
            g_candidate38E7D0Hook.reset();
            g_candidate187898Hook.reset();
            g_exitHook.reset();
            g_enterHook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: {}", error.what());
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
