#include "stdafx.h"
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
    constexpr std::uintptr_t kExitCallbackRva = 0x6B6C482;
    constexpr std::uintptr_t kDispatchCallRva = 0x26F7A23;
    constexpr std::uint8_t kDispatchCallBytes[] = { 0xFF, 0xD0 };
    constexpr std::uint8_t kExitCallbackBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE,
    };
    constexpr char kEnterSignature[] =
        "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 "
        "48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3";
    constexpr std::uint64_t kMaxRecords = 256;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_enterHook;
    SafetyHookMid g_dispatchCallHook;
    SafetyHookMid g_exitHook;
    std::atomic<std::uint64_t> g_sequence{0};
    std::atomic<std::uintptr_t> g_enterAddress{};
    std::atomic<DWORD> g_traceThread{};
    std::atomic<std::uintptr_t> g_traceContext{};
    std::atomic<std::uint32_t> g_postEnterCalls{};
    std::atomic<bool> g_traceArmed{false};

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT) return false;
        const auto protection = info.Protect & 0xff;
        return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    }

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

    std::uintptr_t Rva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    std::uintptr_t ReturnAddress(const SafetyHookContext& context)
    {
        std::uintptr_t value{};
        Read(static_cast<std::uintptr_t>(context.rsp), value);
        return value;
    }

    void LogInnerState(const char* stage, std::uintptr_t inner, const SafetyHookContext& context,
        std::uintptr_t item = 0, std::uintptr_t vtable = 0, std::uintptr_t target = 0, std::intptr_t slot = -1)
    {
        if (!g_logger) return;
        float aspect{};
        std::uint8_t flags{};
        float fov230{};
        const bool stateReadable = inner &&
            Read(inner + 0x254, aspect) && Read(inner + 0x259, flags) && Read(inner + 0x230, fov230);
        const auto caller = ReturnAddress(context);
        g_logger->info(
            "TRACE seq={} stage={} thread={} callerRva=0x{:X} callerExec={} rcx=0x{:X} rdx=0x{:X} r8=0x{:X} r9=0x{:X} "
            "xmm0={} xmm1={} xmm2={} xmm3={} object=0x{:X} inner=0x{:X} readable={} aspect={} flags=0x{:02X} fov230={} ",
            g_sequence.fetch_add(1, std::memory_order_relaxed) + 1, stage, GetCurrentThreadId(), Rva(caller),
            IsExecutable(caller), static_cast<std::uintptr_t>(context.rcx), static_cast<std::uintptr_t>(context.rdx),
            static_cast<std::uintptr_t>(context.r8), static_cast<std::uintptr_t>(context.r9),
            context.xmm0.f32[0], context.xmm1.f32[0], context.xmm2.f32[0], context.xmm3.f32[0],
            item, inner, stateReadable, aspect, flags, fov230);
        g_logger->info("TRACE detail stage={} vtable=0x{:X} target=0x{:X} vtableSlot=0x{:X} xmm4={} xmm5={} xmm6={} xmm7={}",
            stage, vtable, target, slot < 0 ? 0 : static_cast<std::uintptr_t>(slot),
            context.xmm4.f32[0], context.xmm5.f32[0], context.xmm6.f32[0], context.xmm7.f32[0]);
    }

    void LogState(const char* stage, std::uintptr_t object, const SafetyHookContext& context)
    {
        std::uintptr_t inner{};
        Read(object + 0xF8, inner);
        LogInnerState(stage, inner, context, object);
    }

    void TraceDispatchCall(SafetyHookContext& context)
    {
        if (g_sequence.load(std::memory_order_relaxed) >= kMaxRecords) return;
        const auto item = static_cast<std::uintptr_t>(context.rcx);
        const auto target = static_cast<std::uintptr_t>(context.rax);
        const bool isEnterTarget = target == g_enterAddress.load(std::memory_order_acquire);
        const bool sameThread = GetCurrentThreadId() == g_traceThread.load(std::memory_order_acquire);
        const bool inPostEnterWindow = g_traceArmed.load(std::memory_order_acquire) && sameThread &&
            g_postEnterCalls.load(std::memory_order_relaxed) < 8;
        if (!isEnterTarget && !inPostEnterWindow) return;
        std::uintptr_t callbackContext{};
        if (!Read(item + 0x18, callbackContext)) return;
        if (inPostEnterWindow && !isEnterTarget &&
            callbackContext != g_traceContext.load(std::memory_order_acquire)) return;
        if (inPostEnterWindow && !isEnterTarget)
            g_postEnterCalls.fetch_add(1, std::memory_order_relaxed);
        std::uintptr_t vtable{};
        Read(item, vtable);
        std::intptr_t slot = -1;
        if (vtable) {
            for (std::intptr_t offset = 0; offset <= 0x100; offset += 8) {
                std::uintptr_t entry{};
                if (Read(vtable + static_cast<std::uintptr_t>(offset), entry) && entry == target) {
                    slot = offset;
                    break;
                }
            }
        }
        std::uintptr_t inner{};
        Read(callbackContext + 0xF8, inner);
        LogInnerState(isEnterTarget ? "dispatch-enter-call" : "dispatch-post-enter-call",
            inner, context, item, vtable, target, slot);
    }

    void TraceEnter(SafetyHookContext& context)
    {
        if (g_sequence.load(std::memory_order_relaxed) >= kMaxRecords) return;
        const auto item = static_cast<std::uintptr_t>(context.rcx);
        std::uintptr_t callbackContext{};
        Read(item + 0x18, callbackContext);
        g_traceThread.store(GetCurrentThreadId(), std::memory_order_release);
        g_traceContext.store(callbackContext, std::memory_order_release);
        g_postEnterCalls.store(0, std::memory_order_release);
        g_traceArmed.store(true, std::memory_order_release);
        LogState("cinematic-enter", callbackContext, context);
    }

    void TraceExit(SafetyHookContext& context)
    {
        if (g_sequence.load(std::memory_order_relaxed) >= kMaxRecords) return;
        const auto callbackContext = static_cast<std::uintptr_t>(context.rcx);
        g_traceArmed.store(false, std::memory_order_release);
        std::uintptr_t target{};
        Read(callbackContext + 0xF8, target);
        LogInnerState("cinematic-exit", target, context, callbackContext);
    }

    bool ValidateBytes(std::uintptr_t address, const std::uint8_t* bytes, std::size_t size)
    {
        return IsExecutable(address) && std::memcmp(reinterpret_cast<const void*>(address), bytes, size) == 0;
    }

    std::uint8_t* ResolveUniqueEnter()
    {
        const auto matches = Memory::PatternScanAll(g_executable, kEnterSignature);
        return matches.size() == 1 ? matches.front() : nullptr;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicRuntimeFovDiscovery204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicRuntimeFovDiscovery204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        auto* enter = ResolveUniqueEnter();
        auto* dispatchCall = reinterpret_cast<std::uint8_t*>(g_executable) + kDispatchCallRva;
        auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitCallbackRva;
        if (!enter || !ValidateBytes(reinterpret_cast<std::uintptr_t>(dispatchCall), kDispatchCallBytes, sizeof(kDispatchCallBytes)) ||
            !ValidateBytes(reinterpret_cast<std::uintptr_t>(exit), kExitCallbackBytes, sizeof(kExitCallbackBytes))) {
            g_logger->error("TRACE setup refused: current 2.0.4 ENTER/EXIT validation failed.");
            return 0;
        }
        try {
            g_enterAddress.store(reinterpret_cast<std::uintptr_t>(enter), std::memory_order_release);
            g_enterHook = safetyhook::create_mid(enter, TraceEnter);
            g_dispatchCallHook = safetyhook::create_mid(dispatchCall, TraceDispatchCall);
            g_exitHook = safetyhook::create_mid(exit, TraceExit);
            if (!g_enterHook || !g_dispatchCallHook || !g_exitHook) throw std::runtime_error("hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 dispatch-call + ENTER/EXIT FOV discovery; read-only, no state writes.");
        } catch (...) {
            g_exitHook.reset();
            g_dispatchCallHook.reset();
            g_enterHook.reset();
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
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
