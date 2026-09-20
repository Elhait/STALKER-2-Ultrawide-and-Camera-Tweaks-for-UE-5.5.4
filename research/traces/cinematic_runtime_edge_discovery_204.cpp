#include "stdafx.h"
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
#include <stdexcept>

namespace
{
    constexpr std::uintptr_t kDispatchCallRva = 0x26F7A23;
    constexpr std::uintptr_t kExitCallbackRva = 0x6B6C482;
    constexpr std::uint8_t kDispatchCallBytes[] = { 0xFF, 0xD0 };
    constexpr std::uint8_t kExitCallbackBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE,
    };
    constexpr char kEnterSignature[] =
        "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 "
        "48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3";
    constexpr std::size_t kMaxEdges = 64;
    constexpr std::size_t kMaxHits = 512;

    struct Edge
    {
        std::uintptr_t target{};
        std::uintptr_t vtable{};
        std::intptr_t slot{-1};
        std::uint32_t hits{};
        std::uint64_t firstMs{};
        std::uint64_t lastMs{};
        float xmm0{};
        float xmm1{};
        float xmm2{};
        float xmm3{};
    };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_dispatchHook;
    SafetyHookMid g_enterHook;
    SafetyHookMid g_exitHook;
    std::atomic<std::uint64_t> g_sequence{};
    std::atomic<DWORD> g_thread{};
    std::atomic<std::uintptr_t> g_context{};
    std::atomic<bool> g_armed{};
    std::uint64_t g_enterMs{};
    std::size_t g_edgeCount{};
    std::size_t g_totalHits{};
    std::array<Edge, kMaxEdges> g_edges{};

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

    std::intptr_t FindSlot(std::uintptr_t vtable, std::uintptr_t target)
    {
        if (!vtable) return -1;
        for (std::intptr_t offset = 0; offset <= 0x100; offset += 8) {
            std::uintptr_t entry{};
            if (Read(vtable + static_cast<std::uintptr_t>(offset), entry) && entry == target) return offset;
        }
        return -1;
    }

    void LogEdge(const char* stage, const Edge& edge, std::uintptr_t item, const SafetyHookContext& context)
    {
        g_logger->info(
            "TRACE seq={} stage={} thread={} item=0x{:X} context=0x{:X} callerRva=0x{:X} targetRva=0x{:X} "
            "vtable=0x{:X} slot=0x{:X} hits={} firstMs={} lastMs={} xmm0={} xmm1={} xmm2={} xmm3={}",
            g_sequence.fetch_add(1, std::memory_order_relaxed) + 1, stage, GetCurrentThreadId(), item,
            g_context.load(std::memory_order_relaxed), Rva(ReturnAddress(context)), Rva(edge.target), edge.vtable,
            edge.slot < 0 ? 0 : static_cast<std::uintptr_t>(edge.slot), edge.hits, edge.firstMs - g_enterMs,
            edge.lastMs - g_enterMs, edge.xmm0, edge.xmm1, edge.xmm2, edge.xmm3);
    }

    void TraceDispatch(SafetyHookContext& context)
    {
        if (!g_armed.load(std::memory_order_acquire) || GetCurrentThreadId() != g_thread.load(std::memory_order_acquire)) return;
        if (g_totalHits >= kMaxHits) return;
        const auto item = static_cast<std::uintptr_t>(context.rcx);
        const auto target = static_cast<std::uintptr_t>(context.rax);
        std::uintptr_t itemContext{};
        if (!Read(item + 0x18, itemContext) || itemContext != g_context.load(std::memory_order_acquire)) return;
        std::uintptr_t vtable{};
        Read(item, vtable);
        const auto now = GetTickCount64();
        ++g_totalHits;
        for (std::size_t i = 0; i < g_edgeCount; ++i) {
            auto& edge = g_edges[i];
            if (edge.target != target || edge.vtable != vtable) continue;
            ++edge.hits;
            edge.lastMs = now;
            return;
        }
        if (g_edgeCount >= kMaxEdges) return;
        auto& edge = g_edges[g_edgeCount++];
        edge.target = target;
        edge.vtable = vtable;
        edge.slot = FindSlot(vtable, target);
        edge.hits = 1;
        edge.firstMs = edge.lastMs = now;
        edge.xmm0 = context.xmm0.f32[0];
        edge.xmm1 = context.xmm1.f32[0];
        edge.xmm2 = context.xmm2.f32[0];
        edge.xmm3 = context.xmm3.f32[0];
        LogEdge("edge-first", edge, item, context);
    }

    void TraceEnter(SafetyHookContext& context)
    {
        const auto item = static_cast<std::uintptr_t>(context.rcx);
        std::uintptr_t itemContext{};
        if (!Read(item + 0x18, itemContext)) return;
        g_thread.store(GetCurrentThreadId(), std::memory_order_release);
        g_context.store(itemContext, std::memory_order_release);
        g_enterMs = GetTickCount64();
        g_edgeCount = 0;
        g_totalHits = 0;
        g_edges = {};
        g_armed.store(true, std::memory_order_release);
        g_logger->info("TRACE seq={} stage=window-enter thread={} item=0x{:X} context=0x{:X} callerRva=0x{:X}",
            g_sequence.fetch_add(1, std::memory_order_relaxed) + 1, GetCurrentThreadId(), item, itemContext,
            Rva(ReturnAddress(context)));
    }

    void TraceExit(SafetyHookContext& context)
    {
        if (!g_armed.exchange(false, std::memory_order_acq_rel)) return;
        const auto now = GetTickCount64();
        g_logger->info("TRACE seq={} stage=window-exit thread={} context=0x{:X} durationMs={} totalHits={} uniqueEdges={}",
            g_sequence.fetch_add(1, std::memory_order_relaxed) + 1, GetCurrentThreadId(),
            g_context.load(std::memory_order_relaxed), now - g_enterMs, g_totalHits, g_edgeCount);
        for (std::size_t i = 0; i < g_edgeCount; ++i) LogEdge("edge-summary", g_edges[i], 0, context);
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
        const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2CinematicRuntimeEdgeDiscovery204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicRuntimeEdgeDiscovery204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }
        auto* enter = ResolveUniqueEnter();
        auto* dispatch = reinterpret_cast<std::uint8_t*>(g_executable) + kDispatchCallRva;
        auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitCallbackRva;
        if (!enter || !ValidateBytes(reinterpret_cast<std::uintptr_t>(dispatch), kDispatchCallBytes, sizeof(kDispatchCallBytes)) ||
            !ValidateBytes(reinterpret_cast<std::uintptr_t>(exit), kExitCallbackBytes, sizeof(kExitCallbackBytes))) {
            g_logger->error("TRACE setup refused: current 2.0.4 validation failed.");
            return 0;
        }
        try {
            g_enterHook = safetyhook::create_mid(enter, TraceEnter);
            g_dispatchHook = safetyhook::create_mid(dispatch, TraceDispatch);
            g_exitHook = safetyhook::create_mid(exit, TraceExit);
            if (!g_enterHook || !g_dispatchHook || !g_exitHook) throw std::runtime_error("hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 bounded cinematic edge discovery; read-only, context-correlated.");
        } catch (...) {
            g_exitHook.reset(); g_dispatchHook.reset(); g_enterHook.reset();
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
