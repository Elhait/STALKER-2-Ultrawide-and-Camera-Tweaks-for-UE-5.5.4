#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace
{
    constexpr std::uint8_t kLetterboxSignatureB[] = {
        0x48, 0x8B, 0x41, 0x18, 0x48, 0x8B, 0x88, 0xF8, 0x00, 0x00, 0x00,
        0x80, 0x89, 0x59, 0x02, 0x00, 0x00, 0x01, 0x48, 0x8B, 0x80, 0xF8,
        0x00, 0x00, 0x00, 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E,
        0xE3, 0x3F, 0xB0, 0x01, 0xC3,
    };
    constexpr std::size_t kSetterOffset = 0x19;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_setterBHook;
    PVOID g_vectoredHandler{};
    std::mutex g_watchMutex;
    std::uintptr_t g_watchedObject{};
    std::uintptr_t g_watchedPage{};
    std::size_t g_pageSize{};
    DWORD g_originalProtect{};
    std::uint64_t g_sequence{};

    struct PendingStep
    {
        bool active = false;
        bool oldTrapFlag = false;
        bool targetAccess = false;
        std::uintptr_t object = 0;
        std::uintptr_t faultRip = 0;
        float oldAspect = 0.0f;
        std::uint8_t oldFlags = 0;
    };

    thread_local PendingStep g_pendingStep;

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

    std::uintptr_t ToRva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
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

    bool ReadWatchedState(std::uintptr_t object, float& aspect, std::uint8_t& flags)
    {
        return SafeRead(object + 0x254, aspect) && SafeRead(object + 0x259, flags);
    }

    void RestorePageProtection()
    {
        if (!g_watchedPage || !g_originalProtect || !g_pageSize) return;
        DWORD ignored = 0;
        VirtualProtect(reinterpret_cast<void*>(g_watchedPage), g_pageSize, g_originalProtect, &ignored);
    }

    bool ArmPageForObject(std::uintptr_t object)
    {
        std::lock_guard lock(g_watchMutex);
        MEMORY_BASIC_INFORMATION objectInfo{};
        MEMORY_BASIC_INFORMATION aspectInfo{};
        MEMORY_BASIC_INFORMATION flagsInfo{};
        if (!VirtualQuery(reinterpret_cast<const void*>(object + 0x254), &aspectInfo, sizeof(aspectInfo)) ||
            !VirtualQuery(reinterpret_cast<const void*>(object + 0x259), &flagsInfo, sizeof(flagsInfo)) ||
            aspectInfo.State != MEM_COMMIT || flagsInfo.State != MEM_COMMIT ||
            aspectInfo.BaseAddress != flagsInfo.BaseAddress) return false;

        SYSTEM_INFO systemInfo{};
        GetSystemInfo(&systemInfo);
        g_pageSize = systemInfo.dwPageSize;
        const auto page = reinterpret_cast<std::uintptr_t>(aspectInfo.BaseAddress);
        if (g_watchedPage == page && g_watchedObject == object) return true;
        RestorePageProtection();

        g_watchedObject = object;
        g_watchedPage = page;
        g_originalProtect = aspectInfo.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
        if (!g_originalProtect) return false;
        DWORD ignored = 0;
        return VirtualProtect(reinterpret_cast<void*>(g_watchedPage), g_pageSize,
            g_originalProtect | PAGE_GUARD, &ignored) != FALSE;
    }

    void LogWrite(std::uintptr_t object, std::uintptr_t rip, float oldAspect, std::uint8_t oldFlags,
        float newAspect, std::uint8_t newFlags, bool targetAccess)
    {
        if (!g_logger) return;
        LONG width = 0;
        LONG height = 0;
        ReadClientSize(width, height);
        g_logger->info(
            "TRACE seq={} stage=exit-write-origin object=0x{:X} rip=0x{:X} rva=0x{:X} thread={} targetAccess={} aspect={} -> {} flags=0x{:02X} -> 0x{:02X} client={}x{}",
            ++g_sequence, object, rip, ToRva(rip), GetCurrentThreadId(), targetAccess,
            oldAspect, newAspect, oldFlags, newFlags, width, height);
    }

    LONG CALLBACK VectoredHandler(PEXCEPTION_POINTERS exceptionInfo)
    {
        if (!exceptionInfo || !exceptionInfo->ExceptionRecord || !exceptionInfo->ContextRecord) return EXCEPTION_CONTINUE_SEARCH;
        const auto code = exceptionInfo->ExceptionRecord->ExceptionCode;
        auto* context = exceptionInfo->ContextRecord;

        if (code == STATUS_GUARD_PAGE_VIOLATION) {
            const auto watchedPage = g_watchedPage;
            const auto object = g_watchedObject;
            const auto accessed = exceptionInfo->ExceptionRecord->NumberParameters >= 2 ?
                static_cast<std::uintptr_t>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]) : 0;
            const bool isWrite = exceptionInfo->ExceptionRecord->NumberParameters >= 1 &&
                exceptionInfo->ExceptionRecord->ExceptionInformation[0] == 1;
            const auto targetAspect = object + 0x254;
            const auto targetFlags = object + 0x259;
            const bool targetAccess = isWrite &&
                (accessed == targetAspect || accessed == targetFlags);

            g_pendingStep = {};
            g_pendingStep.active = true;
            g_pendingStep.oldTrapFlag = (context->EFlags & 0x100) != 0;
            g_pendingStep.targetAccess = targetAccess;
            g_pendingStep.object = object;
            g_pendingStep.faultRip = static_cast<std::uintptr_t>(context->Rip);
            ReadWatchedState(object, g_pendingStep.oldAspect, g_pendingStep.oldFlags);
            context->EFlags |= 0x100;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (code == STATUS_SINGLE_STEP && g_pendingStep.active) {
            float newAspect = 0.0f;
            std::uint8_t newFlags = 0;
            if (ReadWatchedState(g_pendingStep.object, newAspect, newFlags) &&
                (newAspect != g_pendingStep.oldAspect || newFlags != g_pendingStep.oldFlags)) {
                LogWrite(g_pendingStep.object, g_pendingStep.faultRip, g_pendingStep.oldAspect,
                    g_pendingStep.oldFlags, newAspect, newFlags, g_pendingStep.targetAccess);
            }
            const bool oldTrapFlag = g_pendingStep.oldTrapFlag;
            g_pendingStep = {};
            if (!oldTrapFlag) context->EFlags &= ~0x100u;
            DWORD ignored = 0;
            if (g_watchedPage && g_originalProtect && g_pageSize) {
                VirtualProtect(reinterpret_cast<void*>(g_watchedPage), g_pageSize,
                    g_originalProtect | PAGE_GUARD, &ignored);
            }
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }

    void TraceSetterB(SafetyHookContext& context)
    {
        const auto object = static_cast<std::uintptr_t>(context.rax);
        if (object) ArmPageForObject(object);
    }

    bool ResolveSetter(std::uint8_t*& setter)
    {
        const auto matches = Memory::PatternScanAll(g_executable,
            "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
        if (matches.size() != 1) return false;
        setter = matches.front() + kSetterOffset;
        constexpr std::uint8_t expected[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F };
        return std::memcmp(setter, expected, sizeof(expected)) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicExitWriteOriginTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicExitWriteOriginTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        std::uint8_t* setter = nullptr;
        if (!ResolveSetter(setter)) {
            g_logger->error("TRACE setup refused: setter-B signature not unique/valid.");
            return 0;
        }
        g_vectoredHandler = AddVectoredExceptionHandler(1, VectoredHandler);
        if (!g_vectoredHandler) {
            g_logger->error("TRACE setup refused: vectored exception handler installation failed.");
            return 0;
        }
        try {
            g_setterBHook = safetyhook::create_mid(setter, TraceSetterB);
            if (!g_setterBHook) throw std::runtime_error("setter hook creation failed");
            g_logger->info("TRACE installed: read-only exit write-origin watch for object+0x254/+0x259; no record cap.");
        } catch (...) {
            g_setterBHook.reset();
            RemoveVectoredExceptionHandler(g_vectoredHandler);
            g_vectoredHandler = nullptr;
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
