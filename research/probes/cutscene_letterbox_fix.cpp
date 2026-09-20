#include "stdafx.h"
#include "helper.hpp"

#include <safetyhook.hpp>
#include <Zydis.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
    constexpr std::uint8_t kLetterboxSignatureA[] = {
        0x48, 0x8B, 0x41, 0x18, 0x48, 0x8B, 0x88, 0xF8, 0x00, 0x00, 0x00,
        0x80, 0x89, 0x59, 0x02, 0x00, 0x00, 0x01, 0x48, 0x8B, 0x80, 0xF8,
        0x00, 0x00, 0x00, 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E,
        0xE3, 0x3F, 0xC3,
    };
    constexpr std::uint8_t kLetterboxSignatureB[] = {
        0x48, 0x8B, 0x41, 0x18, 0x48, 0x8B, 0x88, 0xF8, 0x00, 0x00, 0x00,
        0x80, 0x89, 0x59, 0x02, 0x00, 0x00, 0x01, 0x48, 0x8B, 0x80, 0xF8,
        0x00, 0x00, 0x00, 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E,
        0xE3, 0x3F, 0xB0, 0x01, 0xC3,
    };
    constexpr std::size_t kSetterOffset = 0x19;
    constexpr std::size_t kSetterLength = 10;
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kGlobalFovRva = 0x9EDE50C;
    constexpr float kNativeCinematicAspect = 16.0f / 9.0f;
    constexpr float kMinCinematicFov = 1.0f;
    constexpr float kMaxCinematicFov = 179.0f;
    constexpr float kMinDisplayAspect = 0.5f;
    constexpr float kMaxDisplayAspect = 20.0f;

    constexpr char kCinematicFovSignature[] =
        "F3 0F 10 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 89 F1 E8 ?? ?? ?? ?? 48 89 C1 31 D2 E8";

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_hookA;
    SafetyHookMid g_hookB;
    SafetyHookMid g_cinematicFovHook;
#ifdef CUTSCENE_TRANSITION_TRACE
    SafetyHookMid g_cinematicGlobalWriterHook;
    SafetyHookMid g_cinematicStateWriterHook;
    SafetyHookMid g_cinematicStatePostWriteHook;
#endif
    std::atomic<float> g_displayAspect{0.0f};
    std::atomic_bool g_stopAspectUpdates{false};
#ifdef CINEMATIC_ASPECT_PROBE
    std::atomic<std::uintptr_t> g_lastCinematicObject{};
#endif
#ifdef CUTSCENE_TRANSITION_TRACE
    constexpr std::uint64_t kMaxTraceRecords = 20000;
    std::atomic<std::uint64_t> g_traceSequence{0};
    std::atomic<std::uint64_t> g_traceWindowUntil{0};
    std::atomic_bool g_traceWindowEnabled{false};
    std::atomic<std::uint64_t> g_setterAHits{0};
    std::atomic<std::uint64_t> g_setterBHits{0};
    std::atomic<std::uint64_t> g_lastSetterASequence{0};
    std::atomic<std::uint64_t> g_lastSetterBSequence{0};
#endif

#ifdef CUTSCENE_FOV_SPLIT_STATE
    struct PendingCinematicSplit
    {
        bool valid{};
        float sourceFov{};
        float convertedFov{};
    };

    thread_local PendingCinematicSplit g_pendingCinematicSplit;
#endif

    bool IsWritable(std::uintptr_t address, std::size_t size)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return address < regionEnd && size <= regionEnd - address && (info.Protect & writable);
    }

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT) return false;
        const auto protect = info.Protect & 0xff;
        return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
            protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
    }

    bool IsReadable(std::uintptr_t address, std::size_t size)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        return address < regionEnd && size <= regionEnd - address;
    }

    bool BelongsToExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        return address && VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) &&
            info.State == MEM_COMMIT && info.AllocationBase == g_executable;
    }

    template <typename T>
    bool ReadValue(std::uintptr_t address, T& value)
    {
        if (!IsReadable(address, sizeof(T))) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    void Log(const char* message)
    {
        if (g_logger) g_logger->info("{}", message);
    }

#ifdef CUTSCENE_TRANSITION_TRACE
    bool TraceWindowActive()
    {
        if (!g_traceWindowEnabled.load(std::memory_order_relaxed)) return false;
        const auto until = g_traceWindowUntil.load(std::memory_order_relaxed);
        return until == 0 || GetTickCount64() <= until;
    }

    std::uint64_t TraceSequence()
    {
        return g_traceSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    }
#endif

    bool ReadDisplayAspect(float& aspect)
    {
        DEVMODE display{ .dmSize = sizeof(DEVMODE) };
        if (!EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &display) || display.dmPelsHeight == 0)
            return false;
        aspect = static_cast<float>(display.dmPelsWidth) / static_cast<float>(display.dmPelsHeight);
        return aspect > 0.0f;
    }

    DWORD WINAPI DisplayAspectOwner(void*)
    {
        while (!g_stopAspectUpdates.load(std::memory_order_relaxed)) {
            float aspect = 0.0f;
            if (ReadDisplayAspect(aspect)) g_displayAspect.store(aspect, std::memory_order_relaxed);
            Sleep(500);
        }
        return 0;
    }

    void ApplyDisplayAspect(SafetyHookContext& context, bool setterB)
    {
        // The hook replaces the validated setter instruction. RAX is the same
        // target object used by that instruction; no new dereference is made.
        const auto rawObject = static_cast<std::uintptr_t>(context.rax);
        if (rawObject > (std::numeric_limits<std::uintptr_t>::max)() - kAspectOffset) return;
        const auto target = rawObject + kAspectOffset;
        const float aspect = g_displayAspect.load(std::memory_order_relaxed);
        if (aspect <= 0.0f || !IsWritable(target, sizeof(aspect))) return;
        std::memcpy(reinterpret_cast<void*>(target), &aspect, sizeof(aspect));
#ifdef CINEMATIC_ASPECT_PROBE
        if (setterB) g_lastCinematicObject.store(rawObject, std::memory_order_release);
#endif

#ifdef CUTSCENE_TRANSITION_TRACE
        if (TraceWindowActive()) {
            const auto sequence = TraceSequence();
            if (sequence > kMaxTraceRecords) return;
            auto& hits = setterB ? g_setterBHits : g_setterAHits;
            auto& lastSequence = setterB ? g_lastSetterBSequence : g_lastSetterASequence;
            hits.fetch_add(1, std::memory_order_relaxed);
            lastSequence.store(sequence, std::memory_order_relaxed);
            if (g_logger) g_logger->info("TRACE seq={} stage=letterbox-setter-{} object=0x{:X} aspect={} A_hits={} A_last={} B_hits={} B_last={}",
                sequence, setterB ? 'B' : 'A', rawObject, aspect,
                g_setterAHits.load(std::memory_order_relaxed),
                g_lastSetterASequence.load(std::memory_order_relaxed),
                g_setterBHits.load(std::memory_order_relaxed),
                g_lastSetterBSequence.load(std::memory_order_relaxed));
        }
#endif

        // SafetyHook's context RIP points at a trampoline containing exactly
        // the replaced setter. Skip that original immediate write and resume
        // at the untouched A/B suffix (RET or MOV AL,1; RET).
        context.rip += kSetterLength;
    }

    void ApplyDisplayAspectA(SafetyHookContext& context) { ApplyDisplayAspect(context, false); }
    void ApplyDisplayAspectB(SafetyHookContext& context) { ApplyDisplayAspect(context, true); }

#ifdef CINEMATIC_ASPECT_PROBE
    bool ApplyManualCinematicAspect(float requestedAspect, const char* label)
    {
        const auto object = g_lastCinematicObject.load(std::memory_order_acquire);
        if (!object || object > (std::numeric_limits<std::uintptr_t>::max)() - kAspectOffset)
            return false;

        const auto target = object + kAspectOffset;
        float previousAspect = 0.0f;
        if (!ReadValue(target, previousAspect) || !IsWritable(target, sizeof(requestedAspect)))
            return false;

        std::memcpy(reinterpret_cast<void*>(target), &requestedAspect, sizeof(requestedAspect));
        if (g_logger) g_logger->info("TRACE marker=manual-cinematic-aspect label={} object=0x{:X} previous={} requested={} displayAspect={} applied=true",
            label, object, previousAspect, requestedAspect, g_displayAspect.load(std::memory_order_relaxed));
        return true;
    }

#endif

    void ApplyCinematicHorPlus(SafetyHookContext& context)
    {
        const float sourceFov = context.xmm0.f32[0];
        const float targetAspect = g_displayAspect.load(std::memory_order_relaxed);
        if (!std::isfinite(sourceFov) || sourceFov <= kMinCinematicFov ||
            sourceFov >= kMaxCinematicFov || !std::isfinite(targetAspect) ||
            targetAspect < kMinDisplayAspect || targetAspect > kMaxDisplayAspect)
            return;

        constexpr float pi = 3.14159265358979323846f;
        const float halfRadians = sourceFov * (pi / 360.0f);
        const float tangent = std::tan(halfRadians);
        const float convertedRadians = 2.0f * std::atan(
            tangent * (targetAspect / kNativeCinematicAspect));
        const float convertedFov = convertedRadians * (180.0f / pi);
        if (!std::isfinite(convertedFov) || convertedFov <= kMinCinematicFov ||
            convertedFov >= kMaxCinematicFov)
            return;

        // Change only the scalar FOV lane. The original CALL executes once
        // through the SafetyHook trampoline; no game-owned global is written.
        context.xmm0.f32[0] = convertedFov;
#ifdef CUTSCENE_FOV_SPLIT_STATE
        // The validated cinematic call and the state writer execute on the
        // same thread through the established call chain. Keep the split
        // event thread-local and consume it exactly once at the matching
        // durable write; no timer or indefinite global pending state is used.
        g_pendingCinematicSplit = { true, sourceFov, convertedFov };
#endif
#ifdef CUTSCENE_TRANSITION_TRACE
        if (TraceWindowActive()) {
            const auto sequence = TraceSequence();
            if (sequence > kMaxTraceRecords) return;
            if (g_logger) g_logger->info("TRACE seq={} stage=cinematic-fov sourceFov={} convertedFov={} aspect={} A_hits={} A_last={} B_hits={} B_last={}",
                sequence, sourceFov, convertedFov, targetAspect,
                g_setterAHits.load(std::memory_order_relaxed),
                g_lastSetterASequence.load(std::memory_order_relaxed),
                g_setterBHits.load(std::memory_order_relaxed),
                g_lastSetterBSequence.load(std::memory_order_relaxed));
        }
#endif
    }

#ifdef CUTSCENE_TRANSITION_TRACE
    void TraceCinematicState(SafetyHookContext& context, const char* stage)
    {
        if (!TraceWindowActive() || !g_logger) return;
        const auto sequence = TraceSequence();
        if (sequence > kMaxTraceRecords) return;

        const auto state = static_cast<std::uintptr_t>(context.rcx);
        const auto subobjectAddress = state + 0x28;
        std::uintptr_t vtable = 0;
        std::uintptr_t virtualTarget = 0;
        float state50 = std::numeric_limits<float>::quiet_NaN();
        float state54 = std::numeric_limits<float>::quiet_NaN();
        float state58 = std::numeric_limits<float>::quiet_NaN();
        const bool stateReadable = IsReadable(state, 0x5c);
        const bool subobjectReadable = stateReadable && ReadValue(subobjectAddress, vtable) &&
            IsReadable(vtable, sizeof(std::uintptr_t));
        const bool vtableReadable = subobjectReadable &&
            IsReadable(vtable + 0x10, sizeof(std::uintptr_t));
        const bool targetReadable = vtableReadable && ReadValue(vtable + 0x10, virtualTarget);
        const bool fieldsReadable = stateReadable && ReadValue(state + 0x50, state50) &&
            ReadValue(state + 0x54, state54) && ReadValue(state + 0x58, state58);
        const auto moduleBase = reinterpret_cast<std::uintptr_t>(g_executable);
        const bool targetInExecutable = targetReadable && BelongsToExecutable(virtualTarget);
        const auto targetRva = targetReadable && virtualTarget >= moduleBase ?
            virtualTarget - moduleBase : 0;

        g_logger->info("TRACE seq={} stage={} state=0x{:X} stateReadable={} subobjectAddress=0x{:X} vtable=0x{:X} virtualTarget=0x{:X} moduleBase=0x{:X} targetRva=0x{:X} targetInExecutable={} targetReadable={} targetExecutable={} state50={} state54={} state58={} fieldsReadable={} inputFov={} aspect={}",
            sequence, stage, state, stateReadable, subobjectAddress, vtable, virtualTarget,
            moduleBase, targetRva, targetInExecutable, targetReadable, targetReadable && IsExecutable(virtualTarget),
            state50, state54, state58, fieldsReadable, context.xmm1.f32[0],
            g_displayAspect.load(std::memory_order_relaxed));
    }

    void TraceCinematicStateWriter(SafetyHookContext& context)
    {
#ifdef CUTSCENE_FOV_SPLIT_STATE
        const float incomingFov = context.xmm1.f32[0];
        bool splitApplied = false;
        float splitSourceFov = std::numeric_limits<float>::quiet_NaN();
        float splitConvertedFov = std::numeric_limits<float>::quiet_NaN();
        if (g_pendingCinematicSplit.valid) {
            splitSourceFov = g_pendingCinematicSplit.sourceFov;
            splitConvertedFov = g_pendingCinematicSplit.convertedFov;
            if (incomingFov == splitConvertedFov) {
                context.xmm1.f32[0] = splitSourceFov;
                splitApplied = true;
            }
            // Consume on the first state-writer encounter. A mismatch is a
            // safe no-op for that write and prevents an event from lingering.
            g_pendingCinematicSplit = {};
        }
        if (g_logger && (splitApplied || std::isfinite(splitConvertedFov)))
            g_logger->info("TRACE stage=cinematic-fov-split incoming={} source={} converted={} applied={}",
                incomingFov, splitSourceFov, splitConvertedFov, splitApplied);
#endif
        TraceCinematicState(context, "cinematic-state-before-write");
    }

    void TraceCinematicStatePostWrite(SafetyHookContext& context)
    {
        TraceCinematicState(context, "cinematic-state-after-write");
    }

    void TraceCinematicGlobalWriter(SafetyHookContext& context)
    {
        if (!TraceWindowActive() || !g_logger) return;
        const auto sequence = TraceSequence();
        if (sequence > kMaxTraceRecords) return;
        float previous = std::numeric_limits<float>::quiet_NaN();
        const auto global = reinterpret_cast<std::uintptr_t>(g_executable) + kGlobalFovRva;
        if (IsWritable(global, sizeof(previous)))
            std::memcpy(&previous, reinterpret_cast<const void*>(global), sizeof(previous));
        g_logger->info("TRACE seq={} stage=cinematic-global-writer previous={} value={} A_hits={} A_last={} B_hits={} B_last={}",
            sequence, previous, context.xmm0.f32[0],
            g_setterAHits.load(std::memory_order_relaxed),
            g_lastSetterASequence.load(std::memory_order_relaxed),
            g_setterBHits.load(std::memory_order_relaxed),
            g_lastSetterBSequence.load(std::memory_order_relaxed));
    }
#endif

#ifdef CUTSCENE_TRANSITION_TRACE
#ifdef CINEMATIC_ASPECT_PROBE
    void LogCameraGraphSnapshot(const char* label)
    {
        if (!g_logger) return;
        const auto object = g_lastCinematicObject.load(std::memory_order_acquire);
        if (!object || !IsReadable(object + 0x200, 0x80)) {
            g_logger->info("TRACE marker=camera-graph-snapshot label={} object=0x{:X} readable=false", label, object);
            return;
        }

        g_logger->info("TRACE marker=camera-graph-snapshot label={} object=0x{:X} range=0x200-0x280 renderResolution=unresolved source=not-established",
            label, object);

        for (std::uintptr_t offset = 0x200; offset < 0x280; offset += sizeof(std::uintptr_t)) {
            std::uintptr_t value = 0;
            if (!ReadValue(object + offset, value)) continue;
            const bool pointerLike = value > 0x10000 && IsReadable(value, 0x20);
            g_logger->info("TRACE marker=camera-graph-field label={} object=0x{:X} offset=0x{:X} value=0x{:X} pointerLike={}",
                label, object, offset, value, pointerLike);
            if (!pointerLike) continue;

            std::uintptr_t child0 = 0;
            std::uintptr_t child8 = 0;
            std::uintptr_t child10 = 0;
            std::uintptr_t child18 = 0;
            const bool childReadable = ReadValue(value + 0x00, child0) &&
                ReadValue(value + 0x08, child8) && ReadValue(value + 0x10, child10) &&
                ReadValue(value + 0x18, child18);
            g_logger->info("TRACE marker=camera-graph-child label={} parent=0x{:X} offset=0x{:X} child=0x{:X} readable={} qword0=0x{:X} qword8=0x{:X} qword10=0x{:X} qword18=0x{:X}",
                label, object, offset, value, childReadable, child0, child8, child10, child18);
        }
    }
#endif

    DWORD WINAPI TransitionMarkerLoop(void*)
    {
#ifdef CINEMATIC_ASPECT_PROBE
        bool previousF6 = false;
        bool previousF7 = false;
        bool previousF8 = false;
        bool previousF9 = false;
#else
        bool previousF7 = false;
        bool previousF8 = false;
#endif
        while (!g_stopAspectUpdates.load(std::memory_order_relaxed)) {
#ifdef CINEMATIC_ASPECT_PROBE
            const bool f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
            const bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
            const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            const bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            if (f6 && !previousF6) {
                g_traceWindowEnabled.store(true, std::memory_order_relaxed);
                g_traceWindowUntil.store(0, std::memory_order_relaxed);
                if (g_logger) g_logger->info("TRACE seq={} marker=cutscene-active", TraceSequence());
                LogCameraGraphSnapshot("native-wrong");
            }
            if (f7 && !previousF7) {
                Log("TRACE marker=cinematic-trigger-arm requested=true; gameplay diagnostic ASI owns the one-shot boundary write");
            }
            if (f8 && !previousF8) {
                LogCameraGraphSnapshot("constrained-correct");
                float displayAspect = 0.0f;
                if (!ReadDisplayAspect(displayAspect) || !ApplyManualCinematicAspect(displayAspect, "post-two-pass-native-aspect")) {
                    Log("TRACE marker=manual-cinematic-aspect label=post-two-pass-native-aspect applied=false");
                } else {
                    LogCameraGraphSnapshot("native-wrong-restored");
                }
            }
            if (f9 && !previousF9) {
                g_traceWindowEnabled.store(true, std::memory_order_relaxed);
                g_traceWindowUntil.store(GetTickCount64() + 10000, std::memory_order_relaxed);
                if (g_logger) g_logger->info("TRACE seq={} marker=observation-window-start durationMs=10000", TraceSequence());
            }
#else
            const bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
            const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            if (f7 && !previousF7) {
                g_traceWindowEnabled.store(true, std::memory_order_relaxed);
                g_traceWindowUntil.store(0, std::memory_order_relaxed);
                if (g_logger) g_logger->info("TRACE seq={} marker=cutscene-active", TraceSequence());
            }
            if (f8 && !previousF8) {
                g_traceWindowEnabled.store(true, std::memory_order_relaxed);
                g_traceWindowUntil.store(GetTickCount64() + 10000, std::memory_order_relaxed);
                if (g_logger) g_logger->info("TRACE seq={} marker=observation-window-start durationMs=10000", TraceSequence());
            }
#endif
#ifdef CINEMATIC_ASPECT_PROBE
            previousF6 = f6;
            previousF7 = f7;
            previousF8 = f8;
            previousF9 = f9;
#else
            previousF7 = f7;
            previousF8 = f8;
#endif
            Sleep(10);
        }
        return 0;
    }
#endif

    bool FindUniqueMatch(const std::uint8_t* sectionStart, std::size_t sectionSize,
        const std::uint8_t* signature, std::size_t signatureSize, std::uint8_t*& match)
    {
        std::size_t matches = 0;
        for (std::size_t offset = 0; offset <= sectionSize - signatureSize; ++offset) {
            if (std::memcmp(sectionStart + offset, signature, signatureSize) == 0) {
                match = const_cast<std::uint8_t*>(sectionStart + offset);
                ++matches;
            }
        }
        return matches == 1;
    }

    bool DecodeSetterAndSuffix(std::uint8_t* signatureMatch, std::size_t signatureSize,
        const std::uint8_t* expectedSuffix, std::size_t expectedSuffixSize, std::uint8_t*& setter)
    {
        auto* setterAddress = signatureMatch + kSetterOffset;
        ZydisDecoder decoder{};
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) ||
            !ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, setterAddress, 15, &instruction, operands)) ||
            instruction.mnemonic != ZYDIS_MNEMONIC_MOV || instruction.length != kSetterLength ||
            instruction.operand_count_visible < 2 || operands[0].type != ZYDIS_OPERAND_TYPE_MEMORY ||
            operands[0].mem.base != ZYDIS_REGISTER_RAX || !operands[0].mem.disp.has_displacement ||
            operands[0].mem.disp.value != 0x254 || operands[1].type != ZYDIS_OPERAND_TYPE_IMMEDIATE ||
            operands[1].imm.value.u != 0x3FE38E39 ||
            setterAddress + kSetterLength + expectedSuffixSize > signatureMatch + signatureSize ||
            std::memcmp(setterAddress + kSetterLength, expectedSuffix, expectedSuffixSize) != 0)
            return false;

        setter = setterAddress;
        return true;
    }

    bool ResolveTargets(std::uint8_t*& setterA, std::uint8_t*& setterB,
        std::uint8_t*& cinematicFovCall
#ifdef CUTSCENE_TRANSITION_TRACE
        , std::uint8_t*& cinematicStateWriter
#endif
    )
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
            if (size < sizeof(kLetterboxSignatureA) || size < sizeof(kLetterboxSignatureB)) return false;

            constexpr std::uint8_t suffixA[] = { 0xC3 };
            constexpr std::uint8_t suffixB[] = { 0xB0, 0x01, 0xC3 };
            std::uint8_t* matchA = nullptr;
            std::uint8_t* matchB = nullptr;
            if (!FindUniqueMatch(start, size, kLetterboxSignatureA, sizeof(kLetterboxSignatureA), matchA) ||
                !FindUniqueMatch(start, size, kLetterboxSignatureB, sizeof(kLetterboxSignatureB), matchB)) return false;
            if (!DecodeSetterAndSuffix(matchA, sizeof(kLetterboxSignatureA), suffixA, sizeof(suffixA), setterA) ||
                !DecodeSetterAndSuffix(matchB, sizeof(kLetterboxSignatureB), suffixB, sizeof(suffixB), setterB))
                return false;

#ifndef CINEMATIC_ASPECT_PROBE
            const auto fovPattern = Memory::PatternScanAll(g_executable, kCinematicFovSignature);
            if (fovPattern.size() != 1) return false;
            auto* fovEntry = fovPattern.front();
            if (fovEntry[8] != 0xE8) return false;
            cinematicFovCall = fovEntry + 8;
            if (!IsExecutable(reinterpret_cast<std::uintptr_t>(cinematicFovCall))) return false;
#else
            cinematicFovCall = nullptr;
#endif
#ifdef CUTSCENE_TRANSITION_TRACE
            constexpr char statePattern[] =
                "56 57 48 83 EC 28 48 89 CE F3 0F 11 49 54 E8 ?? ?? ?? ?? 48 85 C0 0F 84 ?? ?? ?? ?? 48 89 C7";
            const auto stateMatches = Memory::PatternScanAll(g_executable, statePattern);
            if (stateMatches.size() != 1) return false;
            auto* stateEntry = stateMatches.front();
            ZydisDecoder decoder{};
            ZydisDecodedInstruction instruction{};
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
            if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) ||
                !ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, stateEntry + 9, 15, &instruction, operands)) ||
                instruction.mnemonic != ZYDIS_MNEMONIC_MOVSS || instruction.length != 5 ||
                instruction.operand_count_visible < 2 || operands[0].type != ZYDIS_OPERAND_TYPE_MEMORY ||
                operands[0].mem.base != ZYDIS_REGISTER_RCX || !operands[0].mem.disp.has_displacement ||
                operands[0].mem.disp.value != 0x54 || operands[1].type != ZYDIS_OPERAND_TYPE_REGISTER ||
                operands[1].reg.value != ZYDIS_REGISTER_XMM1)
                return false;
            cinematicStateWriter = stateEntry;
#endif
            return true;
        }
        return false;
    }

#ifdef CUTSCENE_TRANSITION_TRACE
    std::uint8_t* ResolveGlobalWriter()
    {
        std::vector<std::uint8_t*> matches;
        Memory::ForEachExecutableSection(g_executable, [&](std::uint8_t* start, std::size_t size) {
            for (std::size_t i = 0; i + 8 <= size; ++i) {
                if (start[i] != 0xF3 || start[i + 1] != 0x0F || start[i + 2] != 0x11 || start[i + 3] != 0x05)
                    continue;
                std::int32_t displacement{};
                std::memcpy(&displacement, start + i + 4, sizeof(displacement));
                const auto target = reinterpret_cast<std::uintptr_t>(start + i + 8) + displacement;
                const auto expected = reinterpret_cast<std::uintptr_t>(g_executable) + kGlobalFovRva;
                if (target == expected) matches.push_back(start + i);
            }
        });
        return matches.size() == 1 ? matches.front() : nullptr;
    }
#endif

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() / "STALKER2ExperimentalLetterboxFix.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2ExperimentalLetterboxFix", logPath.string(), true);
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        float initialAspect = 0.0f;
        std::uint8_t* setterA = nullptr;
        std::uint8_t* setterB = nullptr;
        std::uint8_t* cinematicFovCall = nullptr;
#ifdef CUTSCENE_TRANSITION_TRACE
        std::uint8_t* globalWriter = nullptr;
        std::uint8_t* cinematicStateWriter = nullptr;
#endif
        if (!ReadDisplayAspect(initialAspect) ||
            !ResolveTargets(setterA, setterB, cinematicFovCall
#ifdef CUTSCENE_TRANSITION_TRACE
                , cinematicStateWriter
#endif
            )) {
            Log("Letterbox setup refused: display aspect or both validated targets unavailable.");
            return 0;
        }
#ifdef CUTSCENE_TRANSITION_TRACE
        globalWriter = ResolveGlobalWriter();
        if (!globalWriter || !IsExecutable(reinterpret_cast<std::uintptr_t>(globalWriter))) {
            Log("Diagnostic setup refused: unique cinematic global writer unavailable.");
            return 0;
        }
#endif
        g_displayAspect.store(initialAspect, std::memory_order_relaxed);

        const auto ownerThread = CreateThread(nullptr, 0, DisplayAspectOwner, nullptr, 0, nullptr);
        if (!ownerThread) {
            Log("Letterbox setup refused: display-aspect owner thread could not start.");
            return 0;
        }
        CloseHandle(ownerThread);
        try {
            g_hookA = safetyhook::create_mid(setterA, ApplyDisplayAspectA, SafetyHookMid::StartDisabled);
            if (!g_hookA) throw std::runtime_error("hook A was not created");
            if (g_hookA.original_bytes().size() != kSetterLength)
                throw std::runtime_error("hook A did not use the safe near E9 patch width");
            g_hookB = safetyhook::create_mid(setterB, ApplyDisplayAspectB, SafetyHookMid::StartDisabled);
            if (!g_hookB) throw std::runtime_error("hook B was not created");
            if (g_hookB.original_bytes().size() != kSetterLength)
                throw std::runtime_error("hook B did not use the safe near E9 patch width");
#ifdef CUTSCENE_TRANSITION_TRACE
            g_cinematicGlobalWriterHook = safetyhook::create_mid(globalWriter, TraceCinematicGlobalWriter,
                SafetyHookMid::StartDisabled);
            if (!g_cinematicGlobalWriterHook)
                throw std::runtime_error("cinematic global writer hook was not created");
            g_cinematicStateWriterHook = safetyhook::create_mid(cinematicStateWriter,
                TraceCinematicStateWriter, SafetyHookMid::StartDisabled);
            if (!g_cinematicStateWriterHook)
                throw std::runtime_error("cinematic state writer hook was not created");
            g_cinematicStatePostWriteHook = safetyhook::create_mid(cinematicStateWriter + 14,
                TraceCinematicStatePostWrite, SafetyHookMid::StartDisabled);
            if (!g_cinematicStatePostWriteHook)
                throw std::runtime_error("cinematic state post-write hook was not created");
#endif
#ifndef CINEMATIC_ASPECT_PROBE
            g_cinematicFovHook = safetyhook::create_mid(cinematicFovCall, ApplyCinematicHorPlus,
                SafetyHookMid::StartDisabled);
            if (!g_cinematicFovHook)
                throw std::runtime_error("cinematic FOV hook was not created");
            if (g_cinematicFovHook.original_bytes().size() != 5)
                throw std::runtime_error("cinematic FOV hook did not use the expected CALL patch width");
#endif
            if (auto result = g_hookA.enable(); !result)
                throw std::runtime_error("hook A could not be enabled");
            if (auto result = g_hookB.enable(); !result)
                throw std::runtime_error("hook B could not be enabled");
#ifdef CUTSCENE_TRANSITION_TRACE
            if (auto result = g_cinematicGlobalWriterHook.enable(); !result)
                throw std::runtime_error("cinematic global writer hook could not be enabled");
            if (auto result = g_cinematicStateWriterHook.enable(); !result)
                throw std::runtime_error("cinematic state writer hook could not be enabled");
            if (auto result = g_cinematicStatePostWriteHook.enable(); !result)
                throw std::runtime_error("cinematic state post-write hook could not be enabled");
#endif
#ifndef CINEMATIC_ASPECT_PROBE
            if (auto result = g_cinematicFovHook.enable(); !result)
                throw std::runtime_error("cinematic FOV hook could not be enabled");
#endif
#ifdef CUTSCENE_TRANSITION_TRACE
            g_traceWindowEnabled.store(true, std::memory_order_relaxed);
            g_traceWindowUntil.store(0, std::memory_order_relaxed);
            const auto markerThread = CreateThread(nullptr, 0, TransitionMarkerLoop, nullptr, 0, nullptr);
            if (!markerThread) throw std::runtime_error("transition marker thread could not start");
            CloseHandle(markerThread);
            Log("Diagnostic mode: probe hotkeys are sequential: F6=cutscene start, F7=one-shot trigger arm marker, F8=post-two-pass native aspect restore, F9=10-second observation window.");
#endif
#ifdef CINEMATIC_ASPECT_PROBE
            Log("Diagnostic aspect-only probe installed: F6=cutscene marker, F7=one-shot trigger arm marker, F8=aspect-only native restore after two-pass, F9=10-second observation; manual cinematic FOV conversion disabled.");
#else
            Log("Experimental letterbox hooks A/B and dynamic cinematic Hor+ FOV hook installed.");
#endif
        } catch (...) {
#ifdef CUTSCENE_TRANSITION_TRACE
            g_cinematicStatePostWriteHook.reset();
            g_cinematicStateWriterHook.reset();
            g_cinematicGlobalWriterHook.reset();
#endif
            g_cinematicFovHook.reset();
            g_hookB.reset();
            g_hookA.reset();
            g_stopAspectUpdates.store(true, std::memory_order_relaxed);
            Log("Letterbox setup refused safely; any partial hook was rolled back.");
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
