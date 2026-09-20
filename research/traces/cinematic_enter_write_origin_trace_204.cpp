#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <cstdint>
#include <cstring>
#include <cmath>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr std::size_t kSetterOffset = 0x19;
    constexpr std::uint8_t kSetterExpected[] = {
        0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F,
    };
    constexpr std::uint8_t kWriterSignature[] = {
        0xF6, 0x86, 0x62, 0x02, 0x00, 0x00, 0x10,
        0xF3, 0x0F, 0x10, 0x86, 0x30, 0x02, 0x00, 0x00,
        0x0F, 0x85, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x4B, 0x30, 0xF3, 0x0F, 0x11, 0x43, 0x30,
        0xF3, 0x0F, 0x10, 0x86, 0x54, 0x02, 0x00, 0x00,
        0xF3, 0x0F, 0x11, 0x43, 0x5C, 0x0F, 0xB6, 0x96,
        0x59, 0x02, 0x00, 0x00, 0x8B, 0x43, 0x68, 0x83, 0xE2, 0x01,
        0x83, 0xE0, 0xFE, 0x09, 0xD0, 0x89, 0x43, 0x68,
        0x0F, 0xB6, 0x96, 0x59, 0x02, 0x00, 0x00, 0x83, 0xE2, 0x04,
        0x83, 0xE0, 0xFB, 0x09, 0xD0, 0x89, 0x43, 0x68,
        0x8A, 0x96, 0x63, 0x02, 0x00, 0x00, 0x88, 0x53, 0x6C,
    };
    constexpr std::size_t kWriterObserveOffset = 88;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_setterHook;
    SafetyHookMid g_writerHook;
    PVOID g_vectoredHandler{};
    std::mutex g_watchMutex;
    std::uintptr_t g_object{};
    std::uintptr_t g_page{};
    std::size_t g_pageSize{};
    DWORD g_originalProtection{};
    std::uint64_t g_sequence{};
    std::atomic<bool> g_writerArmed{};

    struct PendingStep
    {
        bool active{};
        bool wasTrapEnabled{};
        bool targetAccess{};
        std::uintptr_t object{};
        std::uintptr_t faultRip{};
        float oldAspect{};
        std::uint8_t oldFlags{};
    };

    thread_local PendingStep g_pending;

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

    void ReadClient(LONG& width, LONG& height)
    {
        HWND window = nullptr;
        EnumWindows([](HWND candidate, LPARAM parameter) {
            DWORD processId{};
            GetWindowThreadProcessId(candidate, &processId);
            if (processId == GetCurrentProcessId() && IsWindowVisible(candidate) &&
                GetWindow(candidate, GW_OWNER) == nullptr) {
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

    bool ReadState(std::uintptr_t object, float& aspect, std::uint8_t& flags)
    {
        return SafeRead(object + 0x254, aspect) && SafeRead(object + 0x259, flags);
    }

    void RestoreGuard()
    {
        if (!g_page || !g_originalProtection || !g_pageSize) return;
        DWORD ignored{};
        VirtualProtect(reinterpret_cast<void*>(g_page), g_pageSize, g_originalProtection, &ignored);
    }

    bool ArmObject(std::uintptr_t object)
    {
        std::lock_guard lock(g_watchMutex);
        MEMORY_BASIC_INFORMATION aspectInfo{};
        MEMORY_BASIC_INFORMATION flagsInfo{};
        if (!VirtualQuery(reinterpret_cast<const void*>(object + 0x254), &aspectInfo, sizeof(aspectInfo)) ||
            !VirtualQuery(reinterpret_cast<const void*>(object + 0x259), &flagsInfo, sizeof(flagsInfo)) ||
            aspectInfo.State != MEM_COMMIT || flagsInfo.State != MEM_COMMIT ||
            aspectInfo.BaseAddress != flagsInfo.BaseAddress) return false;

        SYSTEM_INFO systemInfo{};
        GetSystemInfo(&systemInfo);
        g_pageSize = systemInfo.dwPageSize;
        g_page = reinterpret_cast<std::uintptr_t>(aspectInfo.BaseAddress);
        g_object = object;
        g_originalProtection = aspectInfo.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
        if (!g_originalProtection) return false;
        DWORD ignored{};
        return VirtualProtect(reinterpret_cast<void*>(g_page), g_pageSize,
            g_originalProtection | PAGE_GUARD, &ignored) != FALSE;
    }

    void LogWrite(std::uintptr_t object, std::uintptr_t rip, float oldAspect, std::uint8_t oldFlags,
        float newAspect, std::uint8_t newFlags, bool targetAccess)
    {
        if (!g_logger) return;
        LONG width{}, height{};
        ReadClient(width, height);
        g_logger->info(
            "TRACE seq={} stage=enter-write-origin object=0x{:X} rip=0x{:X} rva=0x{:X} thread={} targetAccess={} aspect={} -> {} flags=0x{:02X} -> 0x{:02X} client={}x{}",
            ++g_sequence, object, rip, ToRva(rip), GetCurrentThreadId(), targetAccess,
            oldAspect, newAspect, oldFlags, newFlags, width, height);
    }

    LONG CALLBACK VectoredHandler(PEXCEPTION_POINTERS exceptionInfo)
    {
        if (!exceptionInfo || !exceptionInfo->ExceptionRecord || !exceptionInfo->ContextRecord) return EXCEPTION_CONTINUE_SEARCH;
        auto* context = exceptionInfo->ContextRecord;
        const auto code = exceptionInfo->ExceptionRecord->ExceptionCode;
        if (code == STATUS_GUARD_PAGE_VIOLATION) {
            const auto accessed = exceptionInfo->ExceptionRecord->NumberParameters >= 2 ?
                static_cast<std::uintptr_t>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]) : 0;
            const bool isWrite = exceptionInfo->ExceptionRecord->NumberParameters >= 1 &&
                exceptionInfo->ExceptionRecord->ExceptionInformation[0] == 1;
            const bool targetAccess = isWrite &&
                (accessed == g_object + 0x254 || accessed == g_object + 0x259);
            g_pending = {};
            g_pending.active = true;
            g_pending.wasTrapEnabled = (context->EFlags & 0x100) != 0;
            g_pending.targetAccess = targetAccess;
            g_pending.object = g_object;
            g_pending.faultRip = static_cast<std::uintptr_t>(context->Rip);
            ReadState(g_object, g_pending.oldAspect, g_pending.oldFlags);
            context->EFlags |= 0x100;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (code == STATUS_SINGLE_STEP && g_pending.active) {
            float newAspect{};
            std::uint8_t newFlags{};
            if (g_pending.targetAccess && ReadState(g_pending.object, newAspect, newFlags) &&
                (newAspect != g_pending.oldAspect || newFlags != g_pending.oldFlags)) {
                LogWrite(g_pending.object, g_pending.faultRip, g_pending.oldAspect, g_pending.oldFlags,
                    newAspect, newFlags, true);
                if (std::fabs(newAspect - (16.0f / 9.0f)) <= 0.001f && newFlags == 0x05) {
                    g_writerArmed.store(true, std::memory_order_release);
                }
            }
            const bool wasTrapEnabled = g_pending.wasTrapEnabled;
            g_pending = {};
            if (!wasTrapEnabled) context->EFlags &= ~0x100u;
            DWORD ignored{};
            if (g_page && g_originalProtection && g_pageSize) {
                VirtualProtect(reinterpret_cast<void*>(g_page), g_pageSize,
                    g_originalProtection | PAGE_GUARD, &ignored);
            }
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void TraceSetter(SafetyHookContext& context)
    {
        const auto object = static_cast<std::uintptr_t>(context.rax);
        if (!object) return;
        float aspect{};
        std::uint8_t flags{};
        ReadState(object, aspect, flags);
        LONG width{}, height{};
        ReadClient(width, height);
        if (g_logger) g_logger->info(
            "TRACE stage=cinematic-enter-anchor object=0x{:X} aspect={} flags=0x{:02X} returnRva=0x{:X} client={}x{}",
            object, aspect, flags, ToRva(static_cast<std::uintptr_t>(context.rsp)), width, height);
        ArmObject(object);
    }

    void TraceWriter(SafetyHookContext& context)
    {
        if (!g_writerArmed.load(std::memory_order_acquire) ||
            static_cast<std::uintptr_t>(context.rsi) != g_object) return;
        float sourceAspect{};
        std::uint8_t sourceFlags{};
        if (!ReadState(g_object, sourceAspect, sourceFlags) ||
            std::fabs(sourceAspect - (16.0f / 9.0f)) > 0.001f || sourceFlags != 0x05) return;
        g_writerArmed.store(false, std::memory_order_release);
        float outputAspect{};
        std::uint32_t outputFlags{};
        float outputFov{};
        const auto output = static_cast<std::uintptr_t>(context.rbx);
        const bool reads = SafeRead(output + 0x5C, outputAspect) &&
            SafeRead(output + 0x68, outputFlags) && SafeRead(output + 0x30, outputFov);
        LONG width{}, height{};
        ReadClient(width, height);
        if (g_logger) g_logger->info(
            "TRACE stage=first-writer-after-enter object=0x{:X} output=0x{:X} sourceAspect={} sourceFlags=0x{:02X} outputAspect={} outputFlags=0x{:X} outputFov={} reads={} writerRva=0x{:X} client={}x{}",
            g_object, output, sourceAspect, sourceFlags, outputAspect, outputFlags, outputFov, reads,
            ToRva(static_cast<std::uintptr_t>(context.rip)), width, height);
    }

    bool ResolveSetter(std::uint8_t*& setter)
    {
        const auto matches = Memory::PatternScanAll(g_executable,
            "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
        if (matches.size() != 1) return false;
        setter = matches.front() + kSetterOffset;
        return std::memcmp(setter, kSetterExpected, sizeof(kSetterExpected)) == 0;
    }

    bool ResolveWriter(std::uint8_t*& writer)
    {
        std::vector<std::uint8_t*> matches;
        Memory::ForEachExecutableSection(g_executable, [&](std::uint8_t* start, std::size_t size) {
            if (size < sizeof(kWriterSignature)) return;
            for (std::size_t offset = 0; offset <= size - sizeof(kWriterSignature); ++offset) {
                bool equal = true;
                for (std::size_t index = 0; index < sizeof(kWriterSignature); ++index) {
                    if ((index < 17 || index >= 21) && start[offset + index] != kWriterSignature[index]) {
                        equal = false;
                        break;
                    }
                }
                if (equal) matches.push_back(start + offset);
            }
        });
        if (matches.size() != 1) return false;
        writer = matches.front() + kWriterObserveOffset;
        return writer[0] == 0x88 && writer[1] == 0x53 && writer[2] == 0x6C;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicEnterWriteOriginTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicEnterWriteOriginTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        std::uint8_t* setter{};
        std::uint8_t* writer{};
        if (!ResolveSetter(setter) || !ResolveWriter(writer)) {
            g_logger->error("TRACE setup refused: current 2.0.4 cinematic-enter setter/writer validation failed.");
            return 0;
        }
        g_vectoredHandler = AddVectoredExceptionHandler(1, VectoredHandler);
        if (!g_vectoredHandler) {
            g_logger->error("TRACE setup refused: vectored exception handler installation failed.");
            return 0;
        }
        try {
            g_setterHook = safetyhook::create_mid(setter, TraceSetter);
            g_writerHook = safetyhook::create_mid(writer, TraceWriter);
            if (!g_setterHook || !g_writerHook) throw std::runtime_error("hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 cinematic-enter write-origin and first-writer correlation; read-only.");
        } catch (...) {
            g_writerHook.reset();
            g_setterHook.reset();
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
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
