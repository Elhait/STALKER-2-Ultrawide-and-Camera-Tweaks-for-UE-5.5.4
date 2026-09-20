#include "stdafx.h"

#include <safetyhook.hpp>
#include <Zydis.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

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
    constexpr std::size_t kWriterOffset = 25;
    constexpr std::size_t kPreTraceOffset = kWriterOffset + 18;
    constexpr std::size_t kPostTraceOffset = kWriterOffset + 0x10B;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_preHook;
    SafetyHookMid g_postHook;
#ifdef FOV_RECALC_TRACE
    SafetyHookMid g_recalcPreHook;
    SafetyHookMid g_recalcPostHook;
    thread_local std::uintptr_t g_currentSource{};
    thread_local std::uintptr_t g_currentOutput{};
    struct RecalcTraceContext
    {
        std::uintptr_t output{};
        std::uintptr_t source{};
        std::uint64_t sequence{};
        float param2{};
        std::uint32_t param2Bits{};
        std::uint64_t param3{};
        std::uint64_t param4{};
        bool active{};
    };
    thread_local RecalcTraceContext g_recalcContext{};
#endif
    std::atomic<std::uint64_t> g_nextSequence{0};
    thread_local std::uint64_t g_currentSequence{};
    thread_local bool g_logCurrentSequence{};

    struct TraceSnapshot
    {
        std::uintptr_t source{};
        std::uintptr_t output{};
        std::uint32_t aspectBits{};
        std::uint8_t flags{};
        std::uint32_t outputFovBits{};
        std::uint32_t outputAspectBits{};
        std::uint64_t outputField40{};
        std::uint64_t outputField58{};
        std::uint64_t outputField68{};
        bool valid{};
    };
    thread_local TraceSnapshot g_lastPreSnapshot{};

#if defined(PASSIVE_SETTINGS_TRACE) || defined(FOV_RECALC_TRACE)
    struct ResolutionSnapshot
    {
        DWORD displayWidth{};
        DWORD displayHeight{};
        LONG windowWidth{};
        LONG windowHeight{};
        LONG clientWidth{};
        LONG clientHeight{};
        bool valid{};
    };
    ResolutionSnapshot g_lastResolutionSnapshot{};
#endif

    struct MarkerBinding
    {
        int key;
        const char* name;
    };

    constexpr MarkerBinding kMarkerBindings[] = {
        { VK_F8, "cutscene-exit" },
        { VK_F9, "ads-enter" },
        { VK_F10, "ads-exit" },
        { VK_F11, "pause-open" },
        { VK_F12, "pause-close" },
    };

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

    struct StackProbe
    {
        std::uintptr_t contextRsp{};
        std::uintptr_t trampolineRsp{};
        std::uintptr_t contextRspTop{};
        std::uintptr_t trampolineRspTop{};
    };

    StackProbe ProbeStack(const SafetyHookContext& context)
    {
        StackProbe probe{
            static_cast<std::uintptr_t>(context.rsp),
            static_cast<std::uintptr_t>(context.trampoline_rsp),
            0,
            0,
        };
        SafeRead(probe.contextRsp, probe.contextRspTop);
        SafeRead(probe.trampolineRsp, probe.trampolineRspTop);
        return probe;
    }

    std::uint64_t ExecutableRva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base ? static_cast<std::uint64_t>(address - base) : 0;
    }

    std::string CaptureNativeStack()
    {
        void* frames[12]{};
        const auto count = CaptureStackBackTrace(0, static_cast<DWORD>(std::size(frames)), frames, nullptr);
        std::ostringstream stream;
        stream << "stack=";
        for (USHORT i = 0; i < count; ++i) {
            const auto address = reinterpret_cast<std::uintptr_t>(frames[i]);
            if (i != 0) stream << ",";
            stream << "0x" << std::hex << address << "/rva=0x" << ExecutableRva(address);
        }
        return stream.str();
    }

    bool LogState(const char* phase, std::uint64_t sequence, SafetyHookContext& context, bool prePhase)
    {
        float inputFov = 0.0f;
        float secondaryFov = 0.0f;
        float aspect = 0.0f;
        float state248 = 0.0f;
        float scale25c = 0.0f;
        std::uint8_t flags = 0;
        std::uint8_t mode260 = 0;
        std::uint8_t mode261 = 0;
        std::uint8_t mode262 = 0;
        std::uint8_t mode263 = 0;
        float outputFov = 0.0f;
        float outputAspect = 0.0f;
        std::uint64_t outputField40 = 0;
        std::uint64_t outputField58 = 0;
        std::uint64_t outputField68 = 0;

        const auto source = static_cast<std::uintptr_t>(context.rsi);
        const auto output = static_cast<std::uintptr_t>(context.rbx);
        const auto stack = ProbeStack(context);
        const bool complete =
            SafeRead(source + 0x230, inputFov) && SafeRead(source + 0x234, secondaryFov) &&
            SafeRead(source + 0x248, state248) && SafeRead(source + 0x254, aspect) &&
            SafeRead(source + 0x259, flags) && SafeRead(source + 0x25C, scale25c) &&
            SafeRead(source + 0x260, mode260) && SafeRead(source + 0x261, mode261) &&
            SafeRead(source + 0x262, mode262) && SafeRead(source + 0x263, mode263) &&
            SafeRead(output + 0x30, outputFov) && SafeRead(output + 0x5C, outputAspect);
        const bool outputFieldsReadable =
            SafeRead(output + 0x40, outputField40) && SafeRead(output + 0x58, outputField58) &&
            SafeRead(output + 0x68, outputField68);
        if (!complete || !outputFieldsReadable) {
            if (g_logger) g_logger->warn("TRACE seq={} phase={} state-read-refused", sequence, phase);
            return false;
        }

        const TraceSnapshot snapshot{
            source, output, std::bit_cast<std::uint32_t>(aspect), flags,
            std::bit_cast<std::uint32_t>(outputFov), std::bit_cast<std::uint32_t>(outputAspect),
            outputField40, outputField58, outputField68, true };
        if (prePhase) {
            const bool changed = !g_lastPreSnapshot.valid ||
                snapshot.source != g_lastPreSnapshot.source ||
                snapshot.output != g_lastPreSnapshot.output ||
                snapshot.aspectBits != g_lastPreSnapshot.aspectBits ||
                snapshot.flags != g_lastPreSnapshot.flags ||
                snapshot.outputAspectBits != g_lastPreSnapshot.outputAspectBits ||
                snapshot.outputField58 != g_lastPreSnapshot.outputField58;
            g_lastPreSnapshot = snapshot;
            if (!changed) return false;
        }

        if (g_logger) {
            g_logger->info(
                "TRACE seq={} phase={} source=0x{:X} output=0x{:X} contextRsp=0x{:X} trampolineRsp=0x{:X} contextRspTop=0x{:X} trampolineRspTop=0x{:X} contextRspRva=0x{:X} trampolineRspTopRva=0x{:X} inputFov={} secondaryFov={} state248={} aspect={} flags=0x{:02X} 25C={} 260=0x{:02X} 261=0x{:02X} 262=0x{:02X} 263=0x{:02X} outputFov={} outputAspect={} outputField40=0x{:X} outputField58=0x{:X} outputField68=0x{:X} renderResolution=unresolved",
                sequence, phase, source, output, stack.contextRsp, stack.trampolineRsp, stack.contextRspTop,
                stack.trampolineRspTop, ExecutableRva(stack.contextRsp), ExecutableRva(stack.trampolineRspTop), inputFov,
                secondaryFov, state248, aspect, flags, scale25c,
                mode260, mode261, mode262, mode263, outputFov, outputAspect,
                outputField40, outputField58, outputField68);
            g_logger->info("TRACE seq={} phase={} {}", sequence, phase, CaptureNativeStack());
        }
        return true;
    }

    void TraceBeforeAdjustment(SafetyHookContext& context)
    {
        g_currentSequence = g_nextSequence.fetch_add(1, std::memory_order_relaxed) + 1;
#ifdef FOV_RECALC_TRACE
        g_currentSource = static_cast<std::uintptr_t>(context.rsi);
        g_currentOutput = static_cast<std::uintptr_t>(context.rbx);
#endif
        g_logCurrentSequence = LogState("pre-481A", g_currentSequence, context, true);
    }

    void TraceAfterAdjustment(SafetyHookContext& context)
    {
        if (g_logCurrentSequence) LogState("post-481A", g_currentSequence, context, false);
    }

#ifdef FOV_RECALC_TRACE
    void LogRecalcState(const char* phase, std::uintptr_t output, std::uintptr_t source, std::uint64_t sequence,
        float param2, std::uint32_t param2Bits, std::uint64_t param3, std::uint64_t param4)
    {
        float outputFov = 0.0f;
        float outputScale = 0.0f;
        float derived9d0 = 0.0f;
        float derived9d4 = 0.0f;
        float derived9d8 = 0.0f;
        float sourceAspect = 0.0f;
        std::uint8_t sourceFlags = 0;
        const bool outputReadable =
            SafeRead(output + 0x30, outputFov) && SafeRead(output + 0x40, outputScale) &&
            SafeRead(output + 0x9D0, derived9d0) && SafeRead(output + 0x9D4, derived9d4) &&
            SafeRead(output + 0x9D8, derived9d8);
        const bool sourceReadable = source != 0 && SafeRead(source + 0x254, sourceAspect) &&
            SafeRead(source + 0x259, sourceFlags);
        if (!outputReadable || !g_logger) return;

        g_logger->info(
            "RECALC seq={} phase={} output=0x{:X} source=0x{:X} sourceAspect={} sourceFlags=0x{:02X} param2={} param2Bits=0x{:08X} param3B=0x{:02X} param4B=0x{:02X} outputFov={} output40={} output9D0={} output9D4={} output9D8={}",
            sequence, phase, output, sourceReadable ? source : 0, sourceReadable ? sourceAspect : 0.0f,
            sourceReadable ? sourceFlags : 0, param2, param2Bits, param3 & 0xFF, param4 & 0xFF,
            outputFov, outputScale, derived9d0, derived9d4, derived9d8);
    }

    void TraceRecalcBefore(SafetyHookContext& context)
    {
        g_recalcContext = {};
        const auto output = static_cast<std::uintptr_t>(context.rcx);
        if (!g_logCurrentSequence || output == 0 || output != g_currentOutput) return;

        g_recalcContext.output = output;
        g_recalcContext.source = g_currentSource;
        g_recalcContext.sequence = g_currentSequence;
        g_recalcContext.param2 = context.xmm1.f32[0];
        g_recalcContext.param2Bits = context.xmm1.u32[0];
        g_recalcContext.param3 = static_cast<std::uint64_t>(context.r8) & 0xFF;
        g_recalcContext.param4 = static_cast<std::uint64_t>(context.r9) & 0xFF;
        g_recalcContext.active = true;
        LogRecalcState("pre", g_recalcContext.output, g_recalcContext.source, g_recalcContext.sequence,
            g_recalcContext.param2, g_recalcContext.param2Bits, g_recalcContext.param3, g_recalcContext.param4);
    }

    void TraceRecalcAfter(SafetyHookContext& context)
    {
        (void)context;
        if (!g_recalcContext.active) return;

        LogRecalcState("post", g_recalcContext.output, g_recalcContext.source, g_recalcContext.sequence,
            g_recalcContext.param2, g_recalcContext.param2Bits, g_recalcContext.param3, g_recalcContext.param4);
        g_recalcContext = {};
    }
#endif

#if defined(PASSIVE_SETTINGS_TRACE) || defined(FOV_RECALC_TRACE)
    BOOL CALLBACK FindCurrentProcessWindow(HWND window, LPARAM parameter)
    {
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId == GetCurrentProcessId() && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
            *reinterpret_cast<HWND*>(parameter) = window;
            return FALSE;
        }
        return TRUE;
    }

    void LogResolutionChange()
    {
        DEVMODE display{ .dmSize = sizeof(DEVMODE) };
        const bool displayReady = EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &display) != FALSE;
        HWND window = nullptr;
        EnumWindows(FindCurrentProcessWindow, reinterpret_cast<LPARAM>(&window));
        RECT windowRect{};
        RECT clientRect{};
        const bool windowReady = window && GetWindowRect(window, &windowRect) && GetClientRect(window, &clientRect);
        ResolutionSnapshot current{
            displayReady ? display.dmPelsWidth : 0,
            displayReady ? display.dmPelsHeight : 0,
            windowReady ? windowRect.right - windowRect.left : 0,
            windowReady ? windowRect.bottom - windowRect.top : 0,
            windowReady ? clientRect.right - clientRect.left : 0,
            windowReady ? clientRect.bottom - clientRect.top : 0,
            displayReady || windowReady };
        if (!current.valid || (g_lastResolutionSnapshot.valid &&
            current.displayWidth == g_lastResolutionSnapshot.displayWidth &&
            current.displayHeight == g_lastResolutionSnapshot.displayHeight &&
            current.windowWidth == g_lastResolutionSnapshot.windowWidth &&
            current.windowHeight == g_lastResolutionSnapshot.windowHeight &&
            current.clientWidth == g_lastResolutionSnapshot.clientWidth &&
            current.clientHeight == g_lastResolutionSnapshot.clientHeight))
            return;

        g_lastResolutionSnapshot = current;
        if (g_logger) g_logger->info(
            "RESOLUTION display={}x{} window={}x{} client={}x{} source=Win32 display/window bounds engineRenderResolution=unresolved",
            current.displayWidth, current.displayHeight, current.windowWidth, current.windowHeight,
            current.clientWidth, current.clientHeight);
    }

    DWORD WINAPI ResolutionMonitorLoop(void*)
    {
        while (true) {
            LogResolutionChange();
            Sleep(250);
        }
    }
#endif

    DWORD WINAPI PollManualMarkers(void*)
    {
        bool wasDown[std::size(kMarkerBindings)]{};
        for (;;) {
            for (std::size_t i = 0; i < std::size(kMarkerBindings); ++i) {
                const bool isDown = (GetAsyncKeyState(kMarkerBindings[i].key) & 0x8000) != 0;
                if (isDown && !wasDown[i] && g_logger) {
                    g_logger->info(
                        "MARKER name={} lastSequence={}",
                        kMarkerBindings[i].name,
                        g_nextSequence.load(std::memory_order_relaxed));
                }
                wasDown[i] = isDown;
            }
            Sleep(10);
        }
    }

    bool FindUniqueMatch(const std::uint8_t* start, std::size_t size, std::uint8_t*& match)
    {
        std::size_t matches = 0;
        for (std::size_t offset = 0; offset <= size - sizeof(kCameraWriterSignature); ++offset) {
            const auto* candidate = start + offset;
            bool equal = true;
            for (std::size_t i = 0; i < sizeof(kCameraWriterSignature); ++i) {
                if ((i < 17 || i >= 21) && candidate[i] != kCameraWriterSignature[i]) {
                    equal = false;
                    break;
                }
            }
            if (equal) {
                match = const_cast<std::uint8_t*>(candidate);
                ++matches;
            }
        }
        return matches == 1;
    }

    bool ResolveTraceSites(std::uint8_t*& preSite, std::uint8_t*& postSite)
    {
        const auto* base = reinterpret_cast<const std::uint8_t*>(g_executable);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        const auto* section = IMAGE_FIRST_SECTION(nt);
        for (std::uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
            if (std::memcmp(section->Name, ".text", 5) != 0) continue;
            const auto* start = base + section->VirtualAddress;
            const auto size = static_cast<std::size_t>(section->Misc.VirtualSize);
            if (size < sizeof(kCameraWriterSignature) || size <= kPostTraceOffset + 7) return false;

            std::uint8_t* match = nullptr;
            if (!FindUniqueMatch(start, size, match)) return false;
            preSite = match + kPreTraceOffset;
            postSite = match + kPostTraceOffset;

            constexpr std::uint8_t preBytes[] = { 0x0F, 0xB6, 0x96, 0x59, 0x02, 0x00, 0x00 };
            constexpr std::uint8_t postBytes[] = { 0x80, 0xBE, 0x40, 0x02, 0x00, 0x00, 0x00 };
            return std::memcmp(preSite, preBytes, sizeof(preBytes)) == 0 &&
                std::memcmp(postSite, postBytes, sizeof(postBytes)) == 0;
        }
        return false;
    }

#ifdef FOV_RECALC_TRACE
    bool ResolveRecalcSites(std::uint8_t*& entry, std::uint8_t*& exit)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        entry = reinterpret_cast<std::uint8_t*>(base + 0xAF4F4A);
        exit = reinterpret_cast<std::uint8_t*>(base + 0xAF4F72);
        constexpr std::uint8_t entryBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40 };
        constexpr std::uint8_t exitBytes[] = { 0x0F, 0x28, 0x74, 0x24, 0x20 };
        return std::memcmp(entry, entryBytes, sizeof(entryBytes)) == 0 &&
            std::memcmp(exit, exitBytes, sizeof(exitBytes)) == 0;
    }
#endif

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
#ifdef PASSIVE_SETTINGS_TRACE
            "STALKER2PassiveSettingsTransitionTrace.log";
#elif defined(FOV_RECALC_TRACE)
            "STALKER2FovRecalculationTrace.log";
#else
            "STALKER2GameplayTransitionTrace.log";
#endif
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2GameplayTransitionTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        std::uint8_t* preSite = nullptr;
        std::uint8_t* postSite = nullptr;
        if (!ResolveTraceSites(preSite, postSite)) {
            g_logger->error("TRACE setup refused: unique gameplay writer or validated trace sites not found.");
            return 0;
        }

        try {
            g_preHook = safetyhook::create_mid(preSite, TraceBeforeAdjustment);
            if (!g_preHook) throw std::runtime_error("pre hook was not created");
            g_postHook = safetyhook::create_mid(postSite, TraceAfterAdjustment);
            if (!g_postHook) throw std::runtime_error("post hook was not created");
#ifdef FOV_RECALC_TRACE
            std::uint8_t* recalcEntry = nullptr;
            std::uint8_t* recalcExit = nullptr;
            if (!ResolveRecalcSites(recalcEntry, recalcExit))
                throw std::runtime_error("validated FOV recalculation sites not found");
            g_recalcPreHook = safetyhook::create_mid(recalcEntry, TraceRecalcBefore);
            if (!g_recalcPreHook) throw std::runtime_error("recalculation entry hook was not created");
            g_recalcPostHook = safetyhook::create_mid(recalcExit, TraceRecalcAfter);
            if (!g_recalcPostHook) throw std::runtime_error("recalculation exit hook was not created");
#endif
            g_logger->info("TRACE hooks installed: pre-481A and post-481A.");
#ifdef FOV_RECALC_TRACE
            g_logger->info("RECALC hooks installed: FUN_140AF4F4A entry/exit; diagnostic read-only.");
#endif
            const auto markerThread = CreateThread(nullptr, 0, PollManualMarkers, nullptr, 0, nullptr);
            if (!markerThread) throw std::runtime_error("marker thread was not created");
            CloseHandle(markerThread);
            g_logger->info("TRACE manual markers: F8=cutscene-exit F9=ads-enter F10=ads-exit F11=pause-open F12=pause-close.");
#if defined(PASSIVE_SETTINGS_TRACE) || defined(FOV_RECALC_TRACE)
            const auto resolutionThread = CreateThread(nullptr, 0, ResolutionMonitorLoop, nullptr, 0, nullptr);
            if (!resolutionThread) throw std::runtime_error("resolution monitor was not created");
            CloseHandle(resolutionThread);
            g_logger->info("PASSIVE settings transition trace: no camera-state writes, resolution monitoring enabled.");
#endif
        } catch (...) {
#ifdef FOV_RECALC_TRACE
            g_recalcPostHook.reset();
            g_recalcPreHook.reset();
#endif
            g_postHook.reset();
            g_preHook.reset();
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
