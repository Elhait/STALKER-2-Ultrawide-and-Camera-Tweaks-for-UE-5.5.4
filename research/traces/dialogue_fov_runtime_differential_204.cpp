#include "stdafx.h"
#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <array>
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
    constexpr char kWriterSignature[] =
        "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 "
        "0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30";
    constexpr std::size_t kFovStoreOffset = 25;
    constexpr std::size_t kMaxCandidates = 128;
    constexpr float kBaselineFov = 110.0f;
    constexpr float kChangeEpsilon = 0.02f;

    struct Candidate { std::uintptr_t address{}; float last{}; bool active{}; };
    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_writerHook;
    std::array<Candidate, kMaxCandidates> g_candidates{};
    std::size_t g_candidateCount{};
    std::atomic<bool> g_running{true};
    std::atomic<int> g_phase{0};
    std::atomic<std::uint64_t> g_sequence{};
    float g_lastCameraFov = NAN;

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

    Candidate* Find(std::uintptr_t address)
    {
        for (std::size_t i = 0; i < g_candidateCount; ++i)
            if (g_candidates[i].address == address) return &g_candidates[i];
        return nullptr;
    }

    void MarkPhase(int phase, const char* name)
    {
        g_phase.store(phase, std::memory_order_relaxed);
        if (g_logger) g_logger->info("PHASE marker={} value={} timeMs={}", name, phase, GetTickCount64());
    }

    void ScanWritableModuleSections()
    {
        const auto moduleBase = reinterpret_cast<std::uintptr_t>(g_executable);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(moduleBase + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return;
        const auto* section = IMAGE_FIRST_SECTION(nt);
        for (WORD sectionIndex = 0; sectionIndex < nt->FileHeader.NumberOfSections; ++sectionIndex, ++section) {
            if (!(section->Characteristics & IMAGE_SCN_MEM_WRITE)) continue;
            const auto sectionBase = moduleBase + section->VirtualAddress;
            const auto sectionSize = static_cast<std::size_t>(section->Misc.VirtualSize);
            for (std::size_t offset = 0; offset + sizeof(float) <= sectionSize; offset += sizeof(float)) {
                float value{};
                const auto address = sectionBase + offset;
                if (!Read(address, value) || !std::isfinite(value)) continue;
                auto* candidate = Find(address);
                if (!candidate && g_candidateCount < kMaxCandidates && std::fabs(value - kBaselineFov) <= 0.05f) {
                    candidate = &g_candidates[g_candidateCount++];
                    candidate->address = address;
                    candidate->last = value;
                    candidate->active = true;
                    if (g_logger) g_logger->info("CANDIDATE discovered address=0x{:X} value={} phase={}",
                        candidate->address, value, g_phase.load(std::memory_order_relaxed));
                }
                if (!candidate || !candidate->active || std::fabs(value - candidate->last) <= kChangeEpsilon) continue;
                const auto old = candidate->last;
                candidate->last = value;
                if (g_logger) g_logger->info("CANDIDATE change address=0x{:X} old={} new={} phase={} timeMs={}",
                    candidate->address, old, value, g_phase.load(std::memory_order_relaxed), GetTickCount64());
            }
        }
    }

    DWORD WINAPI Monitor(void*)
    {
        bool f8WasDown{}, f9WasDown{}, f10WasDown{};
        while (g_running.load(std::memory_order_relaxed)) {
            const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            const bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            const bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
            if (f8 && !f8WasDown) MarkPhase(1, "A_STABLE_GAMEPLAY");
            if (f9 && !f9WasDown) MarkPhase(2, "B_DIALOGUE_STEADY");
            if (f10 && !f10WasDown) MarkPhase(3, "C_STABLE_AFTER_EXIT");
            f8WasDown = f8; f9WasDown = f9; f10WasDown = f10;
            ScanWritableModuleSections();
            Sleep(250);
        }
        return 0;
    }

    void TraceCameraWriter(SafetyHookContext& context)
    {
        const auto source = static_cast<std::uintptr_t>(context.rsi);
        float cameraFov{};
        float firstPersonFov{};
        float aspect{};
        if (!Read(source + 0x230, cameraFov) || !Read(source + 0x234, firstPersonFov) ||
            !Read(source + 0x254, aspect) || !std::isfinite(cameraFov)) return;
        if (std::isfinite(g_lastCameraFov) && std::fabs(cameraFov - g_lastCameraFov) <= kChangeEpsilon) return;
        const auto previous = g_lastCameraFov;
        g_lastCameraFov = cameraFov;
        if (g_logger) g_logger->info(
            "CAMERA change seq={} phase={} source=0x{:X} worldFOV={} previousWorldFOV={} firstPersonFOV={} aspect={} timeMs={}",
            ++g_sequence, g_phase.load(std::memory_order_relaxed), source, cameraFov, previous,
            firstPersonFov, aspect, GetTickCount64());
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2DialogueFovRuntimeDifferential204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2DialogueFovRuntimeDifferential204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            g_logger->info("TRACE installed: read-only dialogue FOV differential; F8=A F9=B F10=C; baseline=110 target=70.");
            const auto matches = Memory::PatternScanAll(g_executable, kWriterSignature);
            if (matches.size() != 1) throw std::runtime_error("unique current 2.0.4 camera writer not found");
            auto* target = matches.front() + kFovStoreOffset;
            if (target[0] != 0xF3 || target[1] != 0x0F || target[2] != 0x11 || target[3] != 0x43 || target[4] != 0x30)
                throw std::runtime_error("camera writer instruction contract mismatch");
            g_writerHook = safetyhook::create_mid(target, TraceCameraWriter);
            if (!g_writerHook) throw std::runtime_error("camera writer hook creation failed");
            if (const auto thread = CreateThread(nullptr, 0, Monitor, nullptr, 0, nullptr)) CloseHandle(thread);
        } catch (const std::exception& error) {
            g_writerHook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: {}", error.what());
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
