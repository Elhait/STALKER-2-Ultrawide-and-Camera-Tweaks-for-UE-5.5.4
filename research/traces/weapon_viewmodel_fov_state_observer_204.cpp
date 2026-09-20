#include "helper.hpp"

#include <safetyhook.hpp>
#include <Zydis.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <sstream>

namespace
{
    // The complete validated 2.0.4 gameplay camera-writer context.  The
    // observer hooks only at the existing FOV write boundary and never writes
    // to the game object.
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
    constexpr std::size_t kFovWriteOffsetInSignature = 25;
    constexpr std::uintptr_t kWorldFovOffset = 0x230;
    constexpr std::uintptr_t kFirstPersonFovOffset = 0x234;
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kFirstPersonFovStateOffset = 0x262;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_hook;

    struct Snapshot
    {
        std::uintptr_t camera{};
        float worldFov{};
        float firstPersonFov{};
        float aspect{};
        std::uint8_t firstPersonState{};
        bool valid{};
    };
    thread_local Snapshot g_lastSnapshot{};

    template <typename... Args>
    void Log(Args&&... args)
    {
        if (!g_logger) return;
        std::ostringstream message;
        (message << ... << args);
        g_logger->info("{}", message.str());
    }

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

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT) return false;
        const auto protect = info.Protect & 0xff;
        return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
            protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
    }

    std::uintptr_t CallerFromContext(SafetyHookContext& context)
    {
        std::uintptr_t caller{};
        SafeRead(static_cast<std::uintptr_t>(context.rsp), caller);
        return caller;
    }

    void ObserveCameraWriter(SafetyHookContext& context)
    {
        const auto camera = static_cast<std::uintptr_t>(context.rsi);
        Snapshot snapshot{ camera };
        const bool reads = camera &&
            SafeRead(camera + kWorldFovOffset, snapshot.worldFov) &&
            SafeRead(camera + kFirstPersonFovOffset, snapshot.firstPersonFov) &&
            SafeRead(camera + kAspectOffset, snapshot.aspect) &&
            SafeRead(camera + kFirstPersonFovStateOffset, snapshot.firstPersonState);
        snapshot.valid = reads;
        if (!reads || !g_logger) return;

        const bool changed = !g_lastSnapshot.valid ||
            snapshot.camera != g_lastSnapshot.camera ||
            snapshot.worldFov != g_lastSnapshot.worldFov ||
            snapshot.firstPersonFov != g_lastSnapshot.firstPersonFov ||
            snapshot.aspect != g_lastSnapshot.aspect ||
            snapshot.firstPersonState != g_lastSnapshot.firstPersonState;
        if (!changed) return;

        Log("VIEWMODEL snapshot: camera=0x", std::hex, snapshot.camera, std::dec,
            " worldFOV=", snapshot.worldFov,
            " firstPersonFOV=", snapshot.firstPersonFov,
            " aspect=", snapshot.aspect,
            " firstPersonState=0x", std::hex,
            static_cast<unsigned int>(snapshot.firstPersonState), std::dec,
            " caller=0x", std::hex, CallerFromContext(context), std::dec,
            " source=camera-writer reads=1.");
        g_lastSnapshot = snapshot;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2WeaponViewmodelFovStateObserver204.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2WeaponViewmodelFovStateObserver204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        try {
            const auto matches = Memory::PatternScanAll(g_executable,
                "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30");
            if (matches.size() != 1) {
                Log("Observer setup refused: camera-writer signature matches=", matches.size(), ".");
                return 0;
            }
            const auto* signatureMatch = matches.front();
            const auto target = const_cast<std::uint8_t*>(signatureMatch + kFovWriteOffsetInSignature);
            if (!IsExecutable(reinterpret_cast<std::uintptr_t>(target))) {
                Log("Observer setup refused: resolved camera-writer boundary is not executable.");
                return 0;
            }
            Log("Camera-writer signature validated at RVA=0x", std::hex,
                reinterpret_cast<std::uintptr_t>(signatureMatch) - reinterpret_cast<std::uintptr_t>(g_executable),
                std::dec, ".");
            g_hook = safetyhook::create_mid(target, ObserveCameraWriter);
            if (!g_hook) throw std::runtime_error("observer hook was not created");
            Log("Read-only viewmodel FOV state observer installed; no game-state writes.");
        } catch (const std::exception& error) {
            g_hook.reset();
            Log("Observer setup refused safely: ", error.what(), ".");
        } catch (...) {
            g_hook.reset();
            Log("Observer setup refused safely: unknown error.");
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
