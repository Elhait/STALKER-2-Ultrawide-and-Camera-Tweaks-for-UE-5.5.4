#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace
{
    constexpr std::uintptr_t kSetterRva = 0x6B7CB05;
    constexpr std::uintptr_t kExitRva = 0x6B6C482;
    constexpr std::uintptr_t kWriter1Rva = 0x18826D;
    constexpr std::uintptr_t kWriter2Rva = 0x63C452;
    constexpr std::uintptr_t kWriter3Rva = 0x10FFD01;

    constexpr std::uint8_t kSetterBytes[] = {
        0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F
    };
    constexpr std::uint8_t kWriter1Bytes[] = { 0xF3, 0x0F, 0x11, 0xA3, 0x30, 0x02, 0x00, 0x00 };
    constexpr std::uint8_t kWriter2Bytes[] = { 0xF3, 0x0F, 0x11, 0x89, 0x30, 0x02, 0x00, 0x00 };
    constexpr std::uint8_t kWriter3Bytes[] = { 0xF3, 0x0F, 0x11, 0x83, 0x30, 0x02, 0x00, 0x00 };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_setterHook, g_exitHook, g_writer1Hook, g_writer2Hook, g_writer3Hook;
    std::uintptr_t g_inner{};
    std::uint64_t g_sequence{};

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

    bool SameInner(std::uintptr_t object)
    {
        return g_inner != 0 && object == g_inner;
    }

    void LogWriter(const char* name, std::uintptr_t writerRva, std::uintptr_t object, float incoming,
        const SafetyHookContext& context)
    {
        if (!SameInner(object)) return;
        float currentFov{};
        float aspect{};
        std::uint8_t flags{};
        if (!SafeRead(object + 0x230, currentFov) ||
            !SafeRead(object + 0x254, aspect) || !SafeRead(object + 0x259, flags)) return;
        g_logger->info(
            "TRACE seq={} stage=writer-hit writer={} rva=0x{:X} inner=0x{:X} currentFov={} incomingFov={} aspect={} flags=0x{:02X} thread={}",
            ++g_sequence, name, writerRva, object,
            currentFov, incoming, aspect, flags, GetCurrentThreadId());
    }

    void Enter(SafetyHookContext& context)
    {
        const auto inner = static_cast<std::uintptr_t>(context.rax);
        float fov{};
        float aspect{};
        std::uint8_t flags{};
        if (!inner || !SafeRead(inner + 0x230, fov) || !SafeRead(inner + 0x254, aspect) ||
            !SafeRead(inner + 0x259, flags)) return;
        g_inner = inner;
        g_logger->info("TRACE seq={} stage=enter inner=0x{:X} fov={} aspect={} flags=0x{:02X} thread={}",
            ++g_sequence, inner, fov, aspect, flags, GetCurrentThreadId());
    }

    void Exit(SafetyHookContext&)
    {
        if (g_inner) g_logger->info("TRACE seq={} stage=exit inner=0x{:X}", ++g_sequence, g_inner);
        g_inner = 0;
    }

    void Writer1(SafetyHookContext& context)
    { LogWriter("0x18826D_RBX_XMM4", kWriter1Rva, static_cast<std::uintptr_t>(context.rbx), context.xmm4.f32[0], context); }

    void Writer2(SafetyHookContext& context)
    { LogWriter("0x63C452_RCX_XMM1", kWriter2Rva, static_cast<std::uintptr_t>(context.rcx), context.xmm1.f32[0], context); }

    void Writer3(SafetyHookContext& context)
    { LogWriter("0x10FFD01_RBX_XMM0", kWriter3Rva, static_cast<std::uintptr_t>(context.rbx), context.xmm0.f32[0], context); }

    bool Validate(std::uintptr_t rva, const std::uint8_t* bytes, std::size_t size)
    {
        const auto address = reinterpret_cast<const std::uint8_t*>(Base(rva));
        return std::memcmp(address, bytes, size) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicFov230WriteOriginTrace204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicFov230WriteOriginTrace204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            if (!Validate(kSetterRva, kSetterBytes, sizeof(kSetterBytes)) ||
                !Validate(kWriter1Rva, kWriter1Bytes, sizeof(kWriter1Bytes)) ||
                !Validate(kWriter2Rva, kWriter2Bytes, sizeof(kWriter2Bytes)) ||
                !Validate(kWriter3Rva, kWriter3Bytes, sizeof(kWriter3Bytes)))
                throw std::runtime_error("current 2.0.4 writer bytes mismatch");

            g_setterHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kSetterRva)), Enter);
            g_exitHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kExitRva)), Exit);
            g_writer1Hook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kWriter1Rva)), Writer1);
            g_writer2Hook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kWriter2Rva)), Writer2);
            g_writer3Hook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kWriter3Rva)), Writer3);
            if (!g_setterHook || !g_exitHook || !g_writer1Hook || !g_writer2Hook || !g_writer3Hook)
                throw std::runtime_error("hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 +0x230 write-origin; read-only, same-inner correlation.");
        } catch (const std::exception& error) {
            g_writer3Hook.reset(); g_writer2Hook.reset(); g_writer1Hook.reset();
            g_exitHook.reset(); g_setterHook.reset();
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
