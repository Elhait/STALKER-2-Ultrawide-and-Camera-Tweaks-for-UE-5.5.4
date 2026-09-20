#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace
{
    constexpr std::uintptr_t kDispatcherRva = 0x67CF5C4;
    constexpr std::uintptr_t kResolvePostRva = 0x67CF607;
    constexpr std::uint8_t kDispatcherEntryBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x89, 0xCE,
    };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_entryHook;
    SafetyHookMid g_resolvePostHook;
    std::uint64_t g_sequence{};

    struct Invocation
    {
        std::uint64_t sequence = 0;
        std::uintptr_t param = 0;
        std::uintptr_t paramContext = 0;
        std::uintptr_t callerRva = 0;
        std::uintptr_t stack1Rva = 0;
        std::uintptr_t entryRcx = 0;
        std::uintptr_t entryRdx = 0;
        std::uintptr_t entryR8 = 0;
        std::uintptr_t entryR9 = 0;
    };

    thread_local Invocation g_invocation;

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(value) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
        return true;
    }

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        return (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    }

    std::uintptr_t ToRva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    std::uintptr_t ReadStackRva(std::uintptr_t stack, int index)
    {
        std::uintptr_t address = 0;
        if (!SafeRead(stack + static_cast<std::uintptr_t>(index) * sizeof(std::uintptr_t), address) ||
            !IsExecutable(address)) return 0;
        return ToRva(address);
    }

    void ReadClientSize(LONG& width, LONG& height)
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
        if (window && GetClientRect(window, &rect)) {
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
        }
    }

    void TraceFunctionEntry(SafetyHookContext& context)
    {
        g_invocation = {};
        g_invocation.sequence = ++g_sequence;
        g_invocation.entryRcx = static_cast<std::uintptr_t>(context.rcx);
        g_invocation.entryRdx = static_cast<std::uintptr_t>(context.rdx);
        g_invocation.entryR8 = static_cast<std::uintptr_t>(context.r8);
        g_invocation.entryR9 = static_cast<std::uintptr_t>(context.r9);
        g_invocation.callerRva = ReadStackRva(context.rsp, 0);
        g_invocation.stack1Rva = ReadStackRva(context.rsp, 1);
        g_invocation.param = static_cast<std::uintptr_t>(context.rcx);
        SafeRead(g_invocation.param + 0x18, g_invocation.paramContext);
        g_invocation.callerRva = ReadStackRva(context.rsp, 0);
        g_invocation.stack1Rva = ReadStackRva(context.rsp, 1);

        LONG width = 0;
        LONG height = 0;
        ReadClientSize(width, height);
        if (g_logger) {
            g_logger->info(
                "TRACE seq={} stage=function-entry thread={} entryRcx=0x{:X} entryRdx=0x{:X} entryR8=0x{:X} entryR9=0x{:X} param=0x{:X} paramPlus18=0x{:X} callerRva=0x{:X} stack1Rva=0x{:X} client={}x{}",
                g_invocation.sequence, GetCurrentThreadId(), g_invocation.entryRcx,
                g_invocation.entryRdx, g_invocation.entryR8, g_invocation.entryR9,
                g_invocation.param, g_invocation.paramContext, g_invocation.callerRva,
                g_invocation.stack1Rva, width, height);
        }
    }

    void TraceResolvePost(SafetyHookContext& context)
    {
        if (!g_invocation.sequence) return;
        const auto resolveResult = static_cast<std::uintptr_t>(context.rax);
        LONG width = 0;
        LONG height = 0;
        ReadClientSize(width, height);
        if (g_logger) {
            g_logger->info(
                "TRACE seq={} stage=resolve-post thread={} param=0x{:X} paramPlus18=0x{:X} resolveResult=0x{:X} callerRva=0x{:X} stack1Rva=0x{:X} client={}x{}",
                g_invocation.sequence, GetCurrentThreadId(), g_invocation.param,
                g_invocation.paramContext, resolveResult, g_invocation.callerRva,
                g_invocation.stack1Rva, width, height);
        }
        g_invocation = {};
    }

    bool ResolveTargets(std::uint8_t*& dispatcher, std::uint8_t*& resolvePost)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        dispatcher = reinterpret_cast<std::uint8_t*>(base + kDispatcherRva);
        resolvePost = reinterpret_cast<std::uint8_t*>(base + kResolvePostRva);
        if (!IsExecutable(reinterpret_cast<std::uintptr_t>(dispatcher)) ||
            !IsExecutable(reinterpret_cast<std::uintptr_t>(resolvePost))) return false;
        return std::memcmp(dispatcher, kDispatcherEntryBytes, sizeof(kDispatcherEntryBytes)) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicDispatcherEntryTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicDispatcherEntryTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        std::uint8_t* dispatcher = nullptr;
        std::uint8_t* resolvePost = nullptr;
        if (!ResolveTargets(dispatcher, resolvePost)) {
            g_logger->error("TRACE setup refused: dispatcher 2.0.3 entry validation failed.");
            return 0;
        }
        try {
            g_entryHook = safetyhook::create_mid(dispatcher, TraceFunctionEntry);
            g_resolvePostHook = safetyhook::create_mid(resolvePost, TraceResolvePost);
            if (!g_entryHook || !g_resolvePostHook) throw std::runtime_error("dispatcher trace hook creation failed");
            g_logger->info("TRACE installed: dispatcher function-entry and post-resolve capture; read-only, no record cap.");
        } catch (...) {
            g_resolvePostHook.reset();
            g_entryHook.reset();
            g_logger->error("TRACE setup refused safely; hook rollback completed.");
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
