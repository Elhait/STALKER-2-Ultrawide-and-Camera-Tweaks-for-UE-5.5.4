#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cmath>
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
    constexpr std::size_t kSetterLength = 10;

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
    constexpr std::size_t kWriterOffset = 25;
    constexpr std::size_t kWriterPreOffset = kWriterOffset + 18;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_setterBHook;
    SafetyHookMid g_writerHook;
    std::atomic<std::uintptr_t> g_lastCinematicObject{};
    std::atomic<std::uint64_t> g_sequence{};
    std::mutex g_writerLogMutex;

    struct WriterSnapshot
    {
        bool valid = false;
        std::uintptr_t object = 0;
        std::uintptr_t source = 0;
        std::uintptr_t output = 0;
        float sourceAspect = 0.0f;
        std::uint8_t sourceFlags = 0;
        float outputAspect = 0.0f;
        float outputFov = 0.0f;
        LONG clientWidth = 0;
        LONG clientHeight = 0;
    };

    WriterSnapshot g_lastWriterSnapshot;

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

    bool NearlyEqual(float left, float right, float epsilon)
    {
        return std::abs(left - right) <= epsilon;
    }

    bool ReadWriterState(std::uintptr_t source, std::uintptr_t output, float& sourceAspect,
        std::uint8_t& sourceFlags, float& outputAspect, float& outputFov, LONG& clientWidth, LONG& clientHeight)
    {
        if (!SafeRead(source + 0x254, sourceAspect) || !SafeRead(source + 0x259, sourceFlags)) return false;
        SafeRead(output + 0x5C, outputAspect);
        SafeRead(output + 0x30, outputFov);
        ReadClientSize(clientWidth, clientHeight);
        return true;
    }

    void LogStack(const char* stage, std::uint64_t sequence, SafetyHookContext& context,
        std::uintptr_t object, std::uintptr_t source, std::uintptr_t output,
        float sourceAspect, std::uint8_t sourceFlags, float outputAspect, float outputFov,
        LONG clientWidth, LONG clientHeight, const char* reason)
    {
        if (!g_logger) return;
        std::uintptr_t ret0 = 0;
        std::uintptr_t ret1 = 0;
        std::uintptr_t ret2 = 0;
        SafeRead(static_cast<std::uintptr_t>(context.rsp), ret0);
        SafeRead(static_cast<std::uintptr_t>(context.rsp) + 8, ret1);
        SafeRead(static_cast<std::uintptr_t>(context.rsp) + 16, ret2);

        g_logger->info(
            "TRACE seq={} stage={} reason={} object=0x{:X} source=0x{:X} output=0x{:X} sourceRva=0x{:X} outputRva=0x{:X} sourceAspect={} sourceFlags=0x{:02X} outputAspect={} outputFov={} returnRva=0x{:X} stack1Rva=0x{:X} stack2Rva=0x{:X} client={}x{}",
            sequence, stage, reason, object, source, output, ToRva(source), ToRva(output), sourceAspect,
            sourceFlags, outputAspect, outputFov, ToRva(ret0), ToRva(ret1), ToRva(ret2), clientWidth, clientHeight);
    }

    void TraceSetterB(SafetyHookContext& context)
    {
        const auto object = static_cast<std::uintptr_t>(context.rax);
        if (!object) return;
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        g_lastCinematicObject.store(object, std::memory_order_release);
        float sourceAspect = 0.0f;
        std::uint8_t sourceFlags = 0;
        float outputAspect = 0.0f;
        float outputFov = 0.0f;
        LONG clientWidth = 0;
        LONG clientHeight = 0;
        ReadWriterState(object, 0, sourceAspect, sourceFlags, outputAspect, outputFov, clientWidth, clientHeight);
        LogStack("letterbox-setter-B", sequence, context, object, object, 0,
            sourceAspect, sourceFlags, outputAspect, outputFov, clientWidth, clientHeight, "setter-hit");
        // Keep the original setter instruction intact; this hook is observational only.
    }

    void TraceWriter(SafetyHookContext& context)
    {
        const auto source = static_cast<std::uintptr_t>(context.rsi);
        const auto captured = g_lastCinematicObject.load(std::memory_order_acquire);
        if (!source || source != captured) return;
        const auto output = static_cast<std::uintptr_t>(context.rbx);
        float sourceAspect = 0.0f;
        std::uint8_t sourceFlags = 0;
        float outputAspect = 0.0f;
        float outputFov = 0.0f;
        LONG clientWidth = 0;
        LONG clientHeight = 0;
        if (!ReadWriterState(source, output, sourceAspect, sourceFlags, outputAspect, outputFov,
                clientWidth, clientHeight)) return;

        std::lock_guard lock(g_writerLogMutex);
        const auto changed = !g_lastWriterSnapshot.valid ||
            g_lastWriterSnapshot.object != captured || g_lastWriterSnapshot.source != source ||
            g_lastWriterSnapshot.output != output ||
            !NearlyEqual(g_lastWriterSnapshot.sourceAspect, sourceAspect, 0.00001f) ||
            g_lastWriterSnapshot.sourceFlags != sourceFlags ||
            !NearlyEqual(g_lastWriterSnapshot.outputAspect, outputAspect, 0.00001f) ||
            !NearlyEqual(g_lastWriterSnapshot.outputFov, outputFov, 0.05f) ||
            g_lastWriterSnapshot.clientWidth != clientWidth ||
            g_lastWriterSnapshot.clientHeight != clientHeight;
        if (!changed) return;

        const char* reason = !g_lastWriterSnapshot.valid ||
            g_lastWriterSnapshot.object != captured || g_lastWriterSnapshot.source != source ||
            g_lastWriterSnapshot.output != output ? "identity" :
            g_lastWriterSnapshot.sourceAspect != sourceAspect ||
            g_lastWriterSnapshot.sourceFlags != sourceFlags ||
            g_lastWriterSnapshot.outputAspect != outputAspect ? "aspect-or-flags" :
            !NearlyEqual(g_lastWriterSnapshot.outputFov, outputFov, 0.05f) ? "fov-epsilon" : "client-size";

        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        LogStack("camera-writer-same-object", sequence, context, captured, source, output,
            sourceAspect, sourceFlags, outputAspect, outputFov, clientWidth, clientHeight, reason);
        g_lastWriterSnapshot = { true, captured, source, output, sourceAspect, sourceFlags,
            outputAspect, outputFov, clientWidth, clientHeight };
    }

    bool ResolveTargets(std::uint8_t*& setterB, std::uint8_t*& writerPre)
    {
        const auto matches = Memory::PatternScanAll(g_executable,
            "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
        if (matches.size() != 1) return false;
        setterB = matches.front() + kSetterOffset;
        constexpr std::uint8_t setterBytes[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F };
        if (std::memcmp(setterB, setterBytes, sizeof(setterBytes)) != 0) return false;

        std::uint8_t* writerMatch = nullptr;
        std::size_t writerCount = 0;
        Memory::ForEachExecutableSection(g_executable, [&](std::uint8_t* start, std::size_t size) {
            if (size < sizeof(kCameraWriterSignature)) return;
            for (std::size_t i = 0; i <= size - sizeof(kCameraWriterSignature); ++i) {
                bool equal = true;
                for (std::size_t j = 0; j < sizeof(kCameraWriterSignature); ++j) {
                    if ((j < 17 || j >= 21) && start[i + j] != kCameraWriterSignature[j]) { equal = false; break; }
                }
                if (equal) { writerMatch = start + i; ++writerCount; }
            }
        });
        if (writerCount != 1) return false;
        writerPre = writerMatch + kWriterPreOffset;
        constexpr std::uint8_t preBytes[] = { 0x0F, 0xB6, 0x96, 0x59, 0x02, 0x00, 0x00 };
        return std::memcmp(writerPre, preBytes, sizeof(preBytes)) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicRuntimeCallChainTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicRuntimeCallChainTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        std::uint8_t* setterB = nullptr;
        std::uint8_t* writerPre = nullptr;
        if (!ResolveTargets(setterB, writerPre)) {
            g_logger->error("TRACE setup refused: setter-B or validated camera writer signature not unique/valid.");
            return 0;
        }
        try {
            g_setterBHook = safetyhook::create_mid(setterB, TraceSetterB);
            g_writerHook = safetyhook::create_mid(writerPre, TraceWriter);
            if (!g_setterBHook || !g_writerHook) throw std::runtime_error("trace hook creation failed");
            g_logger->info("TRACE installed: setter-B and validated camera-writer same-object intersection; read-only.");
        } catch (...) {
            g_writerHook.reset();
            g_setterBHook.reset();
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
