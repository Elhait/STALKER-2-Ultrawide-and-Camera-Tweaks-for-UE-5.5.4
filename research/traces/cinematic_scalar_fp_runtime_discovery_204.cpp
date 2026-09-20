#include "stdafx.h"
#include "helper.hpp"

#include <safetyhook.hpp>
#include <Zydis.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace {
constexpr std::uint8_t kSetterSignature[] = {
    0x48, 0x8B, 0x41, 0x18, 0x48, 0x8B, 0x88, 0xF8, 0x00, 0x00, 0x00,
    0x80, 0x89, 0x59, 0x02, 0x00, 0x00, 0x01, 0x48, 0x8B, 0x80, 0xF8,
    0x00, 0x00, 0x00, 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E,
    0xE3, 0x3F, 0xC3,
};
constexpr std::size_t kSetterOffset = 0x19;
constexpr std::size_t kSetterLength = 10;
constexpr std::uintptr_t kExitRva = 0x6B6C482;
constexpr std::uintptr_t kDispatchRva = 0x26F7A23;
constexpr std::size_t kMaxRecords = 512;

HMODULE g_module{};
HMODULE g_executable = GetModuleHandle(nullptr);
std::shared_ptr<spdlog::logger> g_logger;
SafetyHookMid g_enterHook;
SafetyHookMid g_dispatchHook;
SafetyHookMid g_exitHook;
PVOID g_veh{};
std::mutex g_mutex;
std::uintptr_t g_page{};
std::size_t g_pageSize{};
DWORD g_originalProtection{};
std::uintptr_t g_inner{};
std::atomic<bool> g_window{};
std::atomic<std::size_t> g_records{};

struct PendingStep {
    bool active{};
    bool wasTrapEnabled{};
    std::uintptr_t faultRip{};
    std::uintptr_t accessed{};
};
thread_local PendingStep g_pending;

bool Readable(std::uintptr_t address, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return address < end && size <= end - address;
}

bool Executable(std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT) return false;
    const auto protect = info.Protect & 0xff;
    return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
        protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
bool SafeRead(std::uintptr_t address, T& value) {
    if (!Readable(address, sizeof(T))) return false;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

std::uintptr_t Rva(std::uintptr_t address) {
    const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
    return address >= base && address - base < 0x20000000 ? address - base : 0;
}

bool IsScalarFp(ZydisMnemonic mnemonic) {
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_MOVSS: case ZYDIS_MNEMONIC_MOVSD:
    case ZYDIS_MNEMONIC_ADDSS: case ZYDIS_MNEMONIC_ADDSD:
    case ZYDIS_MNEMONIC_SUBSS: case ZYDIS_MNEMONIC_SUBSD:
    case ZYDIS_MNEMONIC_MULSS: case ZYDIS_MNEMONIC_MULSD:
    case ZYDIS_MNEMONIC_DIVSS: case ZYDIS_MNEMONIC_DIVSD:
    case ZYDIS_MNEMONIC_MINSS: case ZYDIS_MNEMONIC_MINSD:
    case ZYDIS_MNEMONIC_MAXSS: case ZYDIS_MNEMONIC_MAXSD:
    case ZYDIS_MNEMONIC_SQRTSS: case ZYDIS_MNEMONIC_SQRTSD:
    case ZYDIS_MNEMONIC_COMISS: case ZYDIS_MNEMONIC_COMISD:
    case ZYDIS_MNEMONIC_UCOMISS: case ZYDIS_MNEMONIC_UCOMISD:
    case ZYDIS_MNEMONIC_CVTSS2SD: case ZYDIS_MNEMONIC_CVTSD2SS:
    case ZYDIS_MNEMONIC_CVTSI2SS: case ZYDIS_MNEMONIC_CVTSI2SD:
    case ZYDIS_MNEMONIC_CVTSS2SI: case ZYDIS_MNEMONIC_CVTSD2SI:
    case ZYDIS_MNEMONIC_CVTTSS2SI: case ZYDIS_MNEMONIC_CVTTSD2SI:
        return true;
    default: return false;
    }
}

void RestoreGuard() {
    if (!g_page || !g_originalProtection || !g_pageSize) return;
    DWORD ignored{};
    VirtualProtect(reinterpret_cast<void*>(g_page), g_pageSize, g_originalProtection, &ignored);
}

void LogScalar(std::uintptr_t rip, std::uintptr_t accessed, const CONTEXT& context,
    const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand* operands) {
    if (!g_logger || g_records.fetch_add(1, std::memory_order_relaxed) >= kMaxRecords) return;
    char text[256]{};
    ZydisFormatter formatter{};
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
    ZydisFormatterFormatInstruction(&formatter, &instruction, operands, instruction.operand_count_visible,
        text, sizeof(text), rip, nullptr);
    g_logger->info("TRACE scalar-fp thread={} ripRva=0x{:X} accessed=0x{:X} instr={} xmm0={} xmm1={} xmm2={} xmm3={} xmm4={} xmm5={} xmm6={} xmm7={}",
        GetCurrentThreadId(), Rva(rip), accessed, text,
        context.Xmm0.Low, context.Xmm1.Low, context.Xmm2.Low, context.Xmm3.Low,
        context.Xmm4.Low, context.Xmm5.Low, context.Xmm6.Low, context.Xmm7.Low);
}

LONG CALLBACK Handler(PEXCEPTION_POINTERS info) {
    if (!info || !info->ExceptionRecord || !info->ContextRecord) return EXCEPTION_CONTINUE_SEARCH;
    auto* context = info->ContextRecord;
    if (info->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION && g_window.load()) {
        const auto accessed = info->ExceptionRecord->NumberParameters >= 2
            ? static_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionInformation[1]) : 0;
        if (accessed >= g_page && accessed < g_page + g_pageSize) {
            g_pending = {};
            g_pending.active = true;
            g_pending.wasTrapEnabled = (context->EFlags & 0x100) != 0;
            g_pending.faultRip = static_cast<std::uintptr_t>(context->Rip);
            g_pending.accessed = accessed;
            context->EFlags |= 0x100;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    if (info->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP && g_pending.active) {
        ZydisDecoder decoder{};
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        if (Executable(g_pending.faultRip) && Readable(g_pending.faultRip, 16) &&
            ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) &&
            ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, reinterpret_cast<const void*>(g_pending.faultRip), 16,
                &instruction, operands)) && IsScalarFp(instruction.mnemonic)) {
            LogScalar(g_pending.faultRip, g_pending.accessed, *context, instruction, operands);
        }
        const bool wasTrapEnabled = g_pending.wasTrapEnabled;
        g_pending = {};
        if (!wasTrapEnabled) context->EFlags &= ~0x100u;
        if (g_window.load() && g_page && g_originalProtection) {
            DWORD ignored{};
            VirtualProtect(reinterpret_cast<void*>(g_page), g_pageSize,
                g_originalProtection | PAGE_GUARD, &ignored);
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void ArmInner(std::uintptr_t item) {
    std::uintptr_t inner{};
    if (!item || !SafeRead(item + 0x18, inner) || !inner)
        return;
    if (!inner || !Readable(inner + 0x254, 1)) {
        if (g_logger) g_logger->warn("TRACE stage=enter-rejected item=0x{:X} inner=0x{:X}", item, inner);
        return;
    }
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(reinterpret_cast<const void*>(inner + 0x254), &info, sizeof(info)) ||
        info.State != MEM_COMMIT) return;
    std::lock_guard lock(g_mutex);
    RestoreGuard();
    g_page = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    g_pageSize = systemInfo.dwPageSize;
    g_originalProtection = info.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    if (!g_originalProtection) return;
    g_inner = inner;
    g_records.store(0, std::memory_order_relaxed);
    DWORD ignored{};
    if (VirtualProtect(reinterpret_cast<void*>(g_page), g_pageSize,
        g_originalProtection | PAGE_GUARD, &ignored)) {
        g_window.store(true, std::memory_order_release);
        if (g_logger) g_logger->info("TRACE stage=window-enter item=0x{:X} inner=0x{:X} page=0x{:X}", item, inner, g_page);
    }
}

void Enter(SafetyHookContext& context) {
    ArmInner(static_cast<std::uintptr_t>(context.rcx));
}

void Dispatch(SafetyHookContext& context) {
    ArmInner(static_cast<std::uintptr_t>(context.rcx));
}

void Exit(SafetyHookContext&) {
    std::lock_guard lock(g_mutex);
    g_window.store(false, std::memory_order_release);
    RestoreGuard();
    if (g_logger) g_logger->info("TRACE stage=window-exit inner=0x{:X} scalarRecords={}", g_inner,
        g_records.load(std::memory_order_relaxed));
    g_page = g_inner = 0;
}

DWORD WINAPI Initialize(void*) {
    WCHAR modulePath[MAX_PATH]{};
    GetModuleFileNameW(g_module, modulePath, MAX_PATH);
    const auto path = std::filesystem::path(modulePath).remove_filename() /
        "STALKER2CinematicScalarFpRuntimeDiscovery204.log";
    std::ofstream(path, std::ios::out | std::ios::trunc).close();
    try {
        g_logger = spdlog::basic_logger_mt("STALKER2CinematicScalarFpRuntimeDiscovery204", path.string(), true);
        g_logger->flush_on(spdlog::level::info);
        g_veh = AddVectoredExceptionHandler(1, Handler);
        if (!g_veh) throw std::runtime_error("VEH install failed");
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        const auto matches = Memory::PatternScanAll(g_executable,
            "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F C3");
        if (matches.size() != 1) throw std::runtime_error("ambiguous ENTER signature");
        auto* setter = matches.front() + kSetterOffset;
        auto* exit = reinterpret_cast<std::uint8_t*>(base + kExitRva);
        auto* dispatch = reinterpret_cast<std::uint8_t*>(base + kDispatchRva);
        if (!setter || !Executable(reinterpret_cast<std::uintptr_t>(setter)) ||
            !Executable(reinterpret_cast<std::uintptr_t>(dispatch)) ||
            !Executable(reinterpret_cast<std::uintptr_t>(exit)) || dispatch[0] != 0xFF || dispatch[1] != 0xD0)
            throw std::runtime_error("validated lifecycle anchors unavailable");
        g_enterHook = safetyhook::create_mid(setter, Enter);
        g_dispatchHook = safetyhook::create_mid(dispatch, Dispatch);
        g_exitHook = safetyhook::create_mid(exit, Exit);
        if (!g_enterHook || !g_dispatchHook || !g_exitHook) throw std::runtime_error("hook creation failed");
        g_logger->info("TRACE installed: 2.0.4 scalar-FP instruction discovery; read-only, bounded camera-page window.");
    } catch (const std::exception& exception) {
        if (g_logger) g_logger->error("TRACE setup refused: {}", exception.what());
        RestoreGuard();
        if (g_veh) RemoveVectoredExceptionHandler(g_veh);
        g_veh = nullptr;
    }
    return 0;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
    }
    return TRUE;
}
