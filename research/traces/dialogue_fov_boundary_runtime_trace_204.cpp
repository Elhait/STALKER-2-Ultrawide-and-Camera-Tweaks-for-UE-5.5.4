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
    constexpr char kDialogueSignature[] =
        "48 8B 0A 48 8B 01 0F 28 CE FF 90 08 06 00 00 "
        "0F 2E 76 2C 75 02 7B 1A";
    constexpr char kCameraWriterSignature[] =
        "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 "
        "0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30";
    constexpr std::size_t kDialogueHookOffset = 9;
    constexpr std::size_t kCameraFovStoreOffset = 25;
    constexpr float kEpsilon = 0.01f;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_dialogueHook;
    SafetyHookMid g_cameraHook;
    std::atomic<bool> g_running{true};
    std::atomic<int> g_phase{0};
    std::atomic<std::uint64_t> g_hits{0};
    std::atomic<std::uintptr_t> g_camera{};
    std::atomic<float> g_cameraFov{NAN};
    float g_lastIncoming = NAN;

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

    std::uintptr_t Rva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    void LogPhase(int value, const char* name)
    {
        g_phase.store(value, std::memory_order_relaxed);
        if (g_logger) g_logger->info("PHASE marker={} value={} timeMs={}", name, value, GetTickCount64());
    }

    void TraceCamera(SafetyHookContext& context)
    {
        const auto camera = static_cast<std::uintptr_t>(context.rsi);
        float fov{};
        if (!Read(camera + 0x230, fov) || !std::isfinite(fov)) return;
        g_camera.store(camera, std::memory_order_release);
        g_cameraFov.store(fov, std::memory_order_release);
    }

    void TraceDialogueBoundary(SafetyHookContext& context)
    {
        const auto hit = g_hits.fetch_add(1, std::memory_order_relaxed) + 1;
        const float incoming = context.xmm6.f32[0];
        const auto source = static_cast<std::uintptr_t>(context.rdx);
        std::uintptr_t object{};
        std::uintptr_t vtable{};
        Read(source, object);
        Read(object, vtable);
        const auto camera = g_camera.load(std::memory_order_acquire);
        const auto cameraFov = g_cameraFov.load(std::memory_order_acquire);
        const bool changed = !std::isfinite(g_lastIncoming) || std::fabs(incoming - g_lastIncoming) > kEpsilon;
        if (g_logger && changed) {
            g_logger->info(
                "DIALOGUE_BOUNDARY hit={} phase={} incomingXMM6={} incomingXMM1={} source=0x{:X} object=0x{:X} vtable=0x{:X} camera=0x{:X} cameraWorldFOV={} cameraRva=0x{:X} timeMs={}",
                hit, g_phase.load(std::memory_order_relaxed), incoming, incoming, source, object, vtable,
                camera, cameraFov, Rva(camera), GetTickCount64());
            g_lastIncoming = incoming;
        }
    }

    DWORD WINAPI Monitor(void*)
    {
        bool f8WasDown{}, f9WasDown{}, f10WasDown{};
        while (g_running.load(std::memory_order_relaxed)) {
            const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            const bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            const bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
            if (f8 && !f8WasDown) LogPhase(1, "A_STABLE_GAMEPLAY");
            if (f9 && !f9WasDown) LogPhase(2, "B_DIALOGUE_STEADY");
            if (f10 && !f10WasDown) LogPhase(3, "C_STABLE_AFTER_EXIT");
            f8WasDown = f8;
            f9WasDown = f9;
            f10WasDown = f10;
            Sleep(25);
        }
        return 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2DialogueFovBoundaryRuntimeTrace204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2DialogueFovBoundaryRuntimeTrace204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);

            const auto dialogueMatches = Memory::PatternScanAll(g_executable, kDialogueSignature);
            const auto cameraMatches = Memory::PatternScanAll(g_executable, kCameraWriterSignature);
            g_logger->info("Runtime identity gate: dialogueMatches={} cameraWriterMatches={} dialoguePatternRva=0xD20F6E hookRva=0xD20F77", dialogueMatches.size(), cameraMatches.size());
            if (dialogueMatches.size() != 1 || cameraMatches.size() != 1)
                throw std::runtime_error("unique current 2.0.4 dialogue/camera signatures not found");

            auto* dialogueHook = dialogueMatches.front() + kDialogueHookOffset;
            auto* cameraHook = cameraMatches.front() + kCameraFovStoreOffset;
            if (dialogueHook[0] != 0xFF || dialogueHook[1] != 0x90 || dialogueHook[2] != 0x08 ||
                dialogueHook[3] != 0x06 || dialogueHook[4] != 0x00 || dialogueHook[5] != 0x00)
                throw std::runtime_error("dialogue boundary instruction contract mismatch");
            if (cameraHook[0] != 0xF3 || cameraHook[1] != 0x0F || cameraHook[2] != 0x11 ||
                cameraHook[3] != 0x43 || cameraHook[4] != 0x30)
                throw std::runtime_error("camera writer instruction contract mismatch");

            g_dialogueHook = safetyhook::create_mid(dialogueHook, TraceDialogueBoundary);
            g_cameraHook = safetyhook::create_mid(cameraHook, TraceCamera);
            if (!g_dialogueHook || !g_cameraHook || !g_dialogueHook.enable() || !g_cameraHook.enable())
                throw std::runtime_error("runtime trace hook creation/enable failed");
            g_logger->info("Read-only dialogue boundary trace installed; no register, camera or FOV writes.");
            if (const auto thread = CreateThread(nullptr, 0, Monitor, nullptr, 0, nullptr)) CloseHandle(thread);
        } catch (const std::exception& error) {
            g_cameraHook.reset();
            g_dialogueHook.reset();
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
