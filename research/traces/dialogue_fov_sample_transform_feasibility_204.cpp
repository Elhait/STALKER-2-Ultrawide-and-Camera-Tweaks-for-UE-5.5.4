#include "stdafx.h"
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
#include <stdexcept>

namespace
{
#ifndef DIALOGUE_FEASIBILITY_POLICY
#define DIALOGUE_FEASIBILITY_POLICY 1
#endif
    constexpr char kDialogueSignature[] =
        "48 8B 0A 48 8B 01 0F 28 CE FF 90 08 06 00 00 "
        "0F 2E 76 2C 75 02 7B 1A";
    constexpr char kCameraWriterSignature[] =
        "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 "
        "0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30";
    constexpr std::size_t kDialogueHookOffset = 9;
    constexpr std::size_t kCameraFovStoreOffset = 25;
    constexpr float kTransformEpsilon = 0.01f;
    constexpr float kRecoveryEpsilon = 1.0f;
    constexpr bool kEnableTransform = true;

    enum class State : int { Unknown, GameplayStable, DialogueEntering, DialogueActive, DialogueExiting };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_dialogueHook;
    SafetyHookMid g_cameraHook;
    std::atomic<bool> g_running{true};
    std::atomic<int> g_policy{DIALOGUE_FEASIBILITY_POLICY}; // 1 = Reduced (k=0.5), 0 = Disabled (k=0).
    std::atomic<std::uintptr_t> g_camera{};
    std::atomic<float> g_cameraFov{NAN};
    State g_state = State::Unknown;
    float g_baseline = NAN;
    float g_previous = NAN;
    std::uint64_t g_hits{};
    std::uint64_t g_lastHitMs{};

    template <typename T> bool Read(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(T) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    const char* Name(State state)
    {
        switch (state) {
        case State::GameplayStable: return "GameplayStable";
        case State::DialogueEntering: return "DialogueEntering";
        case State::DialogueActive: return "DialogueActive";
        case State::DialogueExiting: return "DialogueExiting";
        default: return "Unknown";
        }
    }

    void SetState(State next, float sample)
    {
        if (g_state == next) return;
        const auto old = Name(g_state);
        g_state = next;
        if (g_logger) g_logger->info("STATE transition={} -> {} sample={} baselineG={} policy={} timeMs={}",
            old, Name(next), sample, g_baseline,
            g_policy.load(std::memory_order_relaxed) ? "Reduced" : "Disabled", GetTickCount64());
    }

    std::uintptr_t Rva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    void TraceCamera(SafetyHookContext& context)
    {
        const auto camera = static_cast<std::uintptr_t>(context.rsi);
        float fov{};
        if (!Read(camera + 0x230, fov) || !std::isfinite(fov)) return;
        g_camera.store(camera, std::memory_order_release);
        g_cameraFov.store(fov, std::memory_order_release);
    }

    void TraceDialogue(SafetyHookContext& context)
    {
        const float incoming = context.xmm6.f32[0];
        if (!std::isfinite(incoming) || incoming < 1.0f || incoming > 179.0f) return;
        ++g_hits;
        const auto nowMs = GetTickCount64();
        const auto gapMs = g_lastHitMs ? nowMs - g_lastHitMs : 0;
        g_lastHitMs = nowMs;
        if (g_logger) {
            g_logger->info("BOUNDARY hit={} incoming={} gapMs={} state={} baselineG={} camera=0x{:X} cameraWorldFOV={} source=0x{:X} timeMs={}",
                g_hits, incoming, gapMs, Name(g_state), g_baseline,
                g_camera.load(std::memory_order_acquire),
                g_cameraFov.load(std::memory_order_acquire),
                static_cast<std::uintptr_t>(context.rdx), nowMs);
        }
        const float delta = std::isfinite(g_previous) ? incoming - g_previous : 0.0f;
        if (g_state == State::Unknown) {
            g_baseline = incoming;
            SetState(State::DialogueActive, incoming);
        }

        if (std::isfinite(g_baseline)) {
            const bool descending = delta < -kTransformEpsilon;
            const bool ascending = delta > kTransformEpsilon;
            if (g_state == State::GameplayStable && descending) {
                g_baseline = g_previous;
                SetState(State::DialogueActive, incoming);
            }
            if (g_state == State::DialogueActive && ascending)
                SetState(State::DialogueExiting, incoming);
            if (g_state == State::DialogueExiting && ascending && std::fabs(incoming - g_baseline) <= kRecoveryEpsilon) {
                SetState(State::GameplayStable, incoming);
            }
        }

        float output = incoming;
        if (kEnableTransform && (g_state == State::DialogueActive || g_state == State::DialogueExiting)) {
            const float k = g_policy.load(std::memory_order_relaxed) ? 0.5f : 0.0f;
            output = g_baseline + (incoming - g_baseline) * k;
            if (std::isfinite(output)) context.xmm1.f32[0] = output;
        }

        if (g_logger && (std::fabs(output - incoming) > kTransformEpsilon || g_hits % 25 == 0)) {
            std::uintptr_t source = static_cast<std::uintptr_t>(context.rdx), object{}, vtable{};
            Read(source, object); Read(object, vtable);
            g_logger->info("SAMPLE hit={} state={} policy={} incoming={} output={} baselineG={} delta={} camera=0x{:X} cameraWorldFOV={} source=0x{:X} object=0x{:X} vtable=0x{:X} timeMs={}",
                g_hits, Name(g_state), g_policy.load(std::memory_order_relaxed) ? "Reduced" : "Disabled", incoming,
                output, g_baseline, delta, g_camera.load(std::memory_order_acquire),
                g_cameraFov.load(std::memory_order_acquire), source, object, vtable, GetTickCount64());
        }
        g_previous = incoming;
    }

    DWORD WINAPI Monitor(void*)
    {
        bool f7WasDown{};
        while (g_running.load(std::memory_order_relaxed)) {
            const bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
            if (f7 && !f7WasDown) {
                const int next = g_policy.load(std::memory_order_relaxed) ? 0 : 1;
                g_policy.store(next, std::memory_order_relaxed);
                if (g_logger) {
                    g_logger->info("POLICY changed={} (F7 toggles Reduced/Disabled; F5/F6 unused)",
                        next ? "Reduced(k=0.5)" : "Disabled(k=0)");
                }
            }
            f7WasDown = f7;
            Sleep(25);
        }
        return 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2DialogueFovSampleTransformFeasibility204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2DialogueFovSampleTransformFeasibility204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            const auto dialogueMatches = Memory::PatternScanAll(g_executable, kDialogueSignature);
            const auto cameraMatches = Memory::PatternScanAll(g_executable, kCameraWriterSignature);
            g_logger->info("Runtime identity gate: dialogueMatches={} cameraWriterMatches={} dialoguePatternRva=0xD20F6E hookRva=0xD20F77", dialogueMatches.size(), cameraMatches.size());
            if (dialogueMatches.size() != 1 || cameraMatches.size() != 1)
                throw std::runtime_error("unique current 2.0.4 dialogue/camera signatures not found");
            auto* dialogueHook = dialogueMatches.front() + kDialogueHookOffset;
            auto* cameraHook = cameraMatches.front() + kCameraFovStoreOffset;
            if (dialogueHook[0] != 0xFF || dialogueHook[1] != 0x90 || dialogueHook[2] != 0x08 || dialogueHook[3] != 0x06 || dialogueHook[4] != 0x00 || dialogueHook[5] != 0x00)
                throw std::runtime_error("dialogue boundary instruction contract mismatch");
            if (cameraHook[0] != 0xF3 || cameraHook[1] != 0x0F || cameraHook[2] != 0x11 || cameraHook[3] != 0x43 || cameraHook[4] != 0x30)
                throw std::runtime_error("camera writer instruction contract mismatch");
            g_dialogueHook = safetyhook::create_mid(dialogueHook, TraceDialogue);
            g_cameraHook = safetyhook::create_mid(cameraHook, TraceCamera);
            if (!g_dialogueHook || !g_cameraHook || !g_dialogueHook.enable() || !g_cameraHook.enable())
                throw std::runtime_error("runtime feasibility hooks failed");
            g_logger->info("Experimental sample-transform feasibility installed; first boundary sample captures baseline G; F7 toggles Reduced/Disabled; F5/F6 unused; only XMM1 may be transformed; no camera/field/GameData writes.");
            if (const auto thread = CreateThread(nullptr, 0, Monitor, nullptr, 0, nullptr)) CloseHandle(thread);
        } catch (const std::exception& error) {
            g_cameraHook.reset(); g_dialogueHook.reset();
            if (g_logger) g_logger->error("Setup refused safely: {}", error.what());
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH) g_running.store(false, std::memory_order_relaxed);
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
