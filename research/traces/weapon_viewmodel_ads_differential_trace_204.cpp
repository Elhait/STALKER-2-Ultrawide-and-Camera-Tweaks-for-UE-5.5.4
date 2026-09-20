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
    constexpr char kSetter[] = "38 91 65 02 00 00 75 01 C3 88 91 65 02 00 00";
    constexpr char kAdsIn[] = "F3 0F 10 40 4C F3 0F 10 8E F8 00 00 00 0F 2E C8";
    constexpr char kAdsOut[] = "F3 0F 10 40 50 F3 0F 10 8E FC 00 00 00 0F 2E C8";
    constexpr char kMesh[] = "48 89 BE 28 05 00 00 48 85 FF 74 65 84 C0";
    constexpr char kCamera[] = "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30";
    constexpr std::size_t kCameraOffset = 25;
    constexpr std::uintptr_t kWorldFov = 0x230, kFirstPersonFov = 0x234;
    constexpr std::uintptr_t kAspect = 0x254, kFirstPersonState = 0x262;
    constexpr std::uintptr_t kPrimitiveType = 0x265;
    constexpr std::size_t kMaxEvents = 4096;

    HMODULE g_module{}, g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_setter, g_adsIn, g_adsOut, g_mesh, g_camera;
    std::uintptr_t g_cameraObject{}, g_lastMeshObject{};
    std::atomic<bool> g_window{false};
    std::atomic<int> g_phase{0};
    std::atomic<std::size_t> g_events{0};
    std::uint64_t g_windowId{};

    template <typename... Args> void Log(Args&&... args)
    {
        if (!g_logger) return;
        std::ostringstream s; (s << ... << args); g_logger->info("{}", s.str());
    }

    template <typename T> bool Read(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address > end || sizeof(value) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value)); return true;
    }

    bool Executable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT) return false;
        const auto p = info.Protect & 0xff;
        return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    }

    std::uintptr_t Caller(SafetyHookContext& c) { std::uintptr_t v{}; Read(static_cast<std::uintptr_t>(c.rsp), v); return v; }

    std::uint8_t Phase() { return static_cast<std::uint8_t>(g_phase.load(std::memory_order_relaxed)); }

    void SnapshotCamera(const char* reason)
    {
        if (!g_cameraObject) return;
        float world{}, first{}, aspect{}; std::uint8_t state{};
        if (!Read(g_cameraObject + kWorldFov, world) || !Read(g_cameraObject + kFirstPersonFov, first) ||
            !Read(g_cameraObject + kAspect, aspect) || !Read(g_cameraObject + kFirstPersonState, state)) return;
        Log("Camera snapshot: window=", g_windowId, " reason=", reason, " camera=0x", std::hex, g_cameraObject,
            std::dec, " worldFOV=", world, " firstPersonFOV=", first, " aspect=", aspect,
            " firstPersonState=0x", std::hex, static_cast<unsigned int>(state), std::dec, ".");
    }

    void ObserveCamera(SafetyHookContext& c)
    {
        const auto camera = static_cast<std::uintptr_t>(c.rsi);
        if (!camera) return;
        if (camera != g_cameraObject) { g_cameraObject = camera; SnapshotCamera("camera-change"); }
        else if (g_window.load(std::memory_order_relaxed)) SnapshotCamera("camera-writer");
    }

    void OpenWindow()
    {
        if (g_window.exchange(true, std::memory_order_acq_rel)) return;
        ++g_windowId; g_phase.store(1, std::memory_order_relaxed);
        Log("ADS differential window OPEN: id=", g_windowId, "."); SnapshotCamera("baseline");
    }

    void CloseWindow()
    {
        if (!g_window.exchange(false, std::memory_order_acq_rel)) return;
        SnapshotCamera("window-close");
        Log("ADS differential window CLOSE: id=", g_windowId, ".");
        g_phase.store(0, std::memory_order_relaxed);
    }

    void ObserveAdsIn(SafetyHookContext&) { OpenWindow(); }
    void ObserveAdsOut(SafetyHookContext&) { CloseWindow(); }

    void ObserveMesh(SafetyHookContext& c)
    {
        if (!g_window.load(std::memory_order_acquire) || g_events.fetch_add(1, std::memory_order_relaxed) >= kMaxEvents) return;
        const auto object = static_cast<std::uintptr_t>(c.rsi); if (!object || object == g_lastMeshObject) return;
        g_lastMeshObject = object; std::uintptr_t parent{}; std::uint8_t type{}, parentType{};
        const bool parentOk = Read(object + 0x20, parent);
        const bool typeOk = Read(object + kPrimitiveType, type);
        const bool parentTypeOk = parentOk && parent && Read(parent + kPrimitiveType, parentType);
        Log("ADS differential mesh object: window=", g_windowId, " object=0x", std::hex, object,
            " parent=0x", parent, " type=0x", static_cast<unsigned int>(type),
            " parentType=0x", static_cast<unsigned int>(parentType), std::dec,
            " reads=", typeOk, "/", parentTypeOk, ".");
    }

    void ObserveSetter(SafetyHookContext& c)
    {
        if (!g_window.load(std::memory_order_acquire) || g_events.fetch_add(1, std::memory_order_relaxed) >= kMaxEvents) return;
        const auto object = static_cast<std::uintptr_t>(c.rcx); const auto incoming = static_cast<std::uint8_t>(c.rdx & 0xff);
        std::uint8_t old{}; const bool oldOk = Read(object + kPrimitiveType, old);
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable); const auto caller = Caller(c);
        Log("ADS differential setter: window=", g_windowId, " object=0x", std::hex, object,
            " old=0x", static_cast<unsigned int>(old), " incoming=0x", static_cast<unsigned int>(incoming),
            " callerRVA=0x", caller >= base ? caller - base : 0, std::dec, " oldRead=", oldOk,
            " changed=", oldOk && old != incoming, " phase=", static_cast<unsigned int>(Phase()), ".");
    }

    std::uint8_t* Resolve(const char* sig, const char* name)
    {
        const auto matches = Memory::PatternScanAll(g_executable, sig);
        if (matches.size() != 1 || !Executable(reinterpret_cast<std::uintptr_t>(matches.front()))) {
            Log("Resolver rejected: name=", name, " matches=", matches.size(), "."); return nullptr;
        }
        Log("Resolver validated: name=", name, " RVA=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(matches.front()) - reinterpret_cast<std::uintptr_t>(g_executable), std::dec, ".");
        return matches.front();
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR path[MAX_PATH]{}; GetModuleFileNameW(g_module, path, MAX_PATH);
        const auto logPath = std::filesystem::path(path).remove_filename() / "STALKER2WeaponViewmodelAdsDifferentialTrace204.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2WeaponViewmodelAdsDifferentialTrace204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v"); g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }
        try {
            auto* setter = Resolve(kSetter, "primitive-setter"), *in = Resolve(kAdsIn, "ads-in"), *out = Resolve(kAdsOut, "ads-out");
            auto* mesh = Resolve(kMesh, "mesh-assignment"), *camera = Resolve(kCamera, "camera-writer");
            if (!setter || !in || !out || !mesh || !camera) throw std::runtime_error("required signature resolution failed");
            g_setter = safetyhook::create_mid(setter, ObserveSetter); g_adsIn = safetyhook::create_mid(in, ObserveAdsIn);
            g_adsOut = safetyhook::create_mid(out, ObserveAdsOut); g_mesh = safetyhook::create_mid(mesh, ObserveMesh);
            g_camera = safetyhook::create_mid(camera + kCameraOffset, ObserveCamera);
            if (!g_setter || !g_adsIn || !g_adsOut || !g_mesh || !g_camera) throw std::runtime_error("trace hook creation failed");
            Log("Read-only ADS differential trace installed; no camera, primitive or render-state writes.");
        } catch (const std::exception& e) {
            g_setter.reset(); g_adsIn.reset(); g_adsOut.reset(); g_mesh.reset(); g_camera.reset(); Log("Trace setup refused safely: ", e.what(), ".");
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE; g_module = module; DisableThreadLibraryCalls(module);
    const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr); if (thread) CloseHandle(thread); return TRUE;
}
