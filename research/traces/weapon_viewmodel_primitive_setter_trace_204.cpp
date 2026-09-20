#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace
{
    constexpr char kPrimitiveSetterSignature[] =
        "38 91 65 02 00 00 75 01 C3 88 91 65 02 00 00";
    constexpr char kAdsInSignature[] =
        "F3 0F 10 40 4C F3 0F 10 8E F8 00 00 00 0F 2E C8";
    constexpr char kAdsOutSignature[] =
        "F3 0F 10 40 50 F3 0F 10 8E FC 00 00 00 0F 2E C8";
    constexpr char kCameraWriterSignature[] =
        "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30";
    constexpr std::size_t kCameraFovWriteOffset = 25;
    constexpr std::uintptr_t kWorldFovOffset = 0x230;
    constexpr std::uintptr_t kFirstPersonFovOffset = 0x234;
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kFirstPersonStateOffset = 0x262;
    constexpr std::uintptr_t kPrimitiveTypeOffset = 0x265;
    constexpr std::size_t kMaxEvents = 512;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_setterHook;
    SafetyHookMid g_adsInHook;
    SafetyHookMid g_adsOutHook;
    SafetyHookMid g_cameraHook;
    std::uintptr_t g_lastCamera{};
    std::atomic<int> g_adsPhase{0};
    std::atomic<std::size_t> g_eventCount{0};
    ULONGLONG g_lastAdsInTick{};
    ULONGLONG g_lastAdsOutTick{};

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

    const char* AdsPhaseName()
    {
        switch (g_adsPhase.load(std::memory_order_relaxed)) {
        case 1: return "IN";
        case 2: return "OUT";
        default: return "none";
        }
    }

    void ObserveAds(const char* direction)
    {
        const auto now = GetTickCount64();
        auto& lastTick = std::strcmp(direction, "IN") == 0 ? g_lastAdsInTick : g_lastAdsOutTick;
        if (lastTick != 0 && now - lastTick < 500) return;
        lastTick = now;
        g_adsPhase.store(std::strcmp(direction, "IN") == 0 ? 1 : 2, std::memory_order_relaxed);
        Log("ADS marker: phase=", direction, ".");
    }

    void ObserveAdsIn(SafetyHookContext&) { ObserveAds("IN"); }
    void ObserveAdsOut(SafetyHookContext&) { ObserveAds("OUT"); }

    void ObserveCamera(SafetyHookContext& context)
    {
        const auto camera = static_cast<std::uintptr_t>(context.rsi);
        if (!camera || camera == g_lastCamera) return;
        float worldFov{}, firstPersonFov{}, aspect{};
        std::uint8_t state{};
        if (!SafeRead(camera + kWorldFovOffset, worldFov) ||
            !SafeRead(camera + kFirstPersonFovOffset, firstPersonFov) ||
            !SafeRead(camera + kAspectOffset, aspect) ||
            !SafeRead(camera + kFirstPersonStateOffset, state)) return;
        g_lastCamera = camera;
        Log("Camera control: camera=0x", std::hex, camera, std::dec,
            " worldFOV=", worldFov, " firstPersonFOV=", firstPersonFov,
            " aspect=", aspect, " firstPersonState=0x", std::hex,
            static_cast<unsigned int>(state), std::dec, ".");
    }

    void ObserveSetter(SafetyHookContext& context)
    {
        if (g_eventCount.fetch_add(1, std::memory_order_relaxed) >= kMaxEvents) return;

        const auto object = static_cast<std::uintptr_t>(context.rcx);
        const auto incoming = static_cast<std::uint8_t>(context.rdx & 0xff);
        std::uint8_t oldValue{};
        const bool oldRead = SafeRead(object + kPrimitiveTypeOffset, oldValue);
        std::uintptr_t parent{};
        const bool parentRead = SafeRead(object + 0x20, parent);
        std::uint8_t parentType{};
        const bool parentTypeRead = parentRead && parent &&
            SafeRead(parent + kPrimitiveTypeOffset, parentType);
        const auto caller = CallerFromContext(context);
        const auto moduleBase = reinterpret_cast<std::uintptr_t>(g_executable);
        const bool changed = oldRead && oldValue != incoming;

        Log("Primitive setter: object=0x", std::hex, object,
            " caller=0x", caller,
            " callerRVA=0x", caller >= moduleBase ? caller - moduleBase : 0,
            " old=0x", static_cast<unsigned int>(oldValue),
            " incoming=0x", static_cast<unsigned int>(incoming),
            " parent=0x", parent,
            " parentType=0x", static_cast<unsigned int>(parentType),
            std::dec, " oldRead=", oldRead, " parentRead=", parentTypeRead,
            " changed=", changed, " adsPhase=", AdsPhaseName(), ".");

        if (g_lastCamera) {
            float worldFov{}, firstPersonFov{}, aspect{};
            std::uint8_t state{};
            if (SafeRead(g_lastCamera + kWorldFovOffset, worldFov) &&
                SafeRead(g_lastCamera + kFirstPersonFovOffset, firstPersonFov) &&
                SafeRead(g_lastCamera + kAspectOffset, aspect) &&
                SafeRead(g_lastCamera + kFirstPersonStateOffset, state)) {
                Log("Setter camera control: camera=0x", std::hex, g_lastCamera,
                    std::dec, " worldFOV=", worldFov, " firstPersonFOV=", firstPersonFov,
                    " aspect=", aspect, " firstPersonState=0x", std::hex,
                    static_cast<unsigned int>(state), std::dec, ".");
            }
        }
    }

    std::uint8_t* ResolveUnique(const char* signature, const char* name)
    {
        const auto matches = Memory::PatternScanAll(g_executable, signature);
        if (matches.size() != 1) {
            Log("Resolver rejected: name=", name, " matches=", matches.size(), ".");
            return nullptr;
        }
        auto* target = matches.front();
        if (!IsExecutable(reinterpret_cast<std::uintptr_t>(target))) {
            Log("Resolver rejected: name=", name, " target is not executable.");
            return nullptr;
        }
        Log("Resolver validated: name=", name, " RVA=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(target) - reinterpret_cast<std::uintptr_t>(g_executable),
            std::dec, ".");
        return target;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2WeaponViewmodelPrimitiveSetterTrace204.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2WeaponViewmodelPrimitiveSetterTrace204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        try {
            auto* setter = ResolveUnique(kPrimitiveSetterSignature, "primitive-setter");
            auto* adsIn = ResolveUnique(kAdsInSignature, "ads-in");
            auto* adsOut = ResolveUnique(kAdsOutSignature, "ads-out");
            auto* camera = ResolveUnique(kCameraWriterSignature, "camera-writer");
            if (!setter || !adsIn || !adsOut || !camera)
                throw std::runtime_error("one or more required signatures did not resolve uniquely");

            g_setterHook = safetyhook::create_mid(setter, ObserveSetter);
            g_adsInHook = safetyhook::create_mid(adsIn, ObserveAdsIn);
            g_adsOutHook = safetyhook::create_mid(adsOut, ObserveAdsOut);
            g_cameraHook = safetyhook::create_mid(camera + kCameraFovWriteOffset, ObserveCamera);
            if (!g_setterHook || !g_adsInHook || !g_adsOutHook || !g_cameraHook)
                throw std::runtime_error("one or more trace hooks were not created");
            Log("Read-only primitive setter trace installed; no primitive or render-state writes.");
        } catch (const std::exception& error) {
            g_setterHook.reset(); g_adsInHook.reset(); g_adsOutHook.reset(); g_cameraHook.reset();
            Log("Trace setup refused safely: ", error.what(), ".");
        } catch (...) {
            g_setterHook.reset(); g_adsInHook.reset(); g_adsOutHook.reset(); g_cameraHook.reset();
            Log("Trace setup refused safely: unknown error.");
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
