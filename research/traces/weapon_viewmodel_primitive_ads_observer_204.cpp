#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace
{
    constexpr char kAdsInSignature[] = "F3 0F 10 40 4C F3 0F 10 8E F8 00 00 00 0F 2E C8";
    constexpr char kAdsOutSignature[] = "F3 0F 10 40 50 F3 0F 10 8E FC 00 00 00 0F 2E C8";
    constexpr char kMeshAssignmentSignature[] = "48 89 BE 28 05 00 00 48 85 FF 74 65 84 C0";
    constexpr char kCameraWriterSignature[] =
        "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30";
    constexpr std::size_t kCameraFovWriteOffset = 25;

    constexpr std::uintptr_t kWorldFovOffset = 0x230;
    constexpr std::uintptr_t kFirstPersonFovOffset = 0x234;
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kFirstPersonStateOffset = 0x262;
    constexpr std::uintptr_t kPrimitiveTypeOffset = 0x265;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_cameraHook;
    SafetyHookMid g_meshHook;
    SafetyHookMid g_adsInHook;
    SafetyHookMid g_adsOutHook;

    struct Candidate
    {
        std::uintptr_t object{};
        std::uintptr_t parent{};
        std::uint8_t objectType{};
        std::uint8_t parentType{};
        bool valid{};
    };
    std::array<Candidate, 64> g_candidates{};
    std::size_t g_nextCandidate{};
    std::size_t g_assignmentLogCount{};
    std::uintptr_t g_lastCamera{};
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

    void LogCandidate(const Candidate& candidate, const char* reason)
    {
        if (!candidate.valid) return;
        Log("PRIMITIVE candidate: reason=", reason,
            " object=0x", std::hex, candidate.object,
            " parent=0x", candidate.parent, std::dec,
            " object+0x265=0x", std::hex, static_cast<unsigned int>(candidate.objectType),
            " parent+0x265=0x", static_cast<unsigned int>(candidate.parentType), std::dec, ".");
    }

    void LogAdsObjectProbe(const char* registerName, std::uintptr_t object)
    {
        if (!object) return;
        std::uint8_t type{};
        std::uintptr_t parent{};
        const bool typeOk = SafeRead(object + kPrimitiveTypeOffset, type);
        const bool parentOk = SafeRead(object + 0x20, parent);
        std::uint8_t parentType{};
        const bool parentTypeOk = parentOk && parent && SafeRead(parent + kPrimitiveTypeOffset, parentType);
        Log("ADS object probe: register=", registerName, " object=0x", std::hex, object,
            " object+0x265=0x", static_cast<unsigned int>(type),
            " parent=0x", parent, " parent+0x265=0x", static_cast<unsigned int>(parentType),
            std::dec, " reads=", typeOk, "/", parentTypeOk, ".");
    }

    void ObserveMeshAssignment(SafetyHookContext& context)
    {
        const auto object = static_cast<std::uintptr_t>(context.rsi);
        if (!object) return;
        std::uintptr_t parent{};
        std::uint8_t objectType{}, parentType{};
        if (!SafeRead(object + 0x20, parent) || !parent ||
            !SafeRead(object + kPrimitiveTypeOffset, objectType) ||
            !SafeRead(parent + kPrimitiveTypeOffset, parentType)) return;

        for (const auto& existing : g_candidates) {
            if (existing.valid && existing.object == object && existing.parent == parent) return;
        }
        const Candidate candidate{ object, parent, objectType, parentType, true };
        g_candidates[g_nextCandidate++ % g_candidates.size()] = candidate;
        if (g_assignmentLogCount++ < 256)
            LogCandidate(candidate, "mesh-assignment");
    }

    void LogAdsEvent(const char* eventName, SafetyHookContext& context)
    {
        const auto now = GetTickCount64();
        auto& lastTick = std::strcmp(eventName, "IN") == 0 ? g_lastAdsInTick : g_lastAdsOutTick;
        if (lastTick != 0 && now - lastTick < 500)
            return;
        lastTick = now;

        Log("ADS event: event=", eventName,
            " rax=0x", std::hex, static_cast<std::uintptr_t>(context.rax),
            " rcx=0x", static_cast<std::uintptr_t>(context.rcx),
            " rdx=0x", static_cast<std::uintptr_t>(context.rdx),
            " rsi=0x", static_cast<std::uintptr_t>(context.rsi),
            " caller=0x", CallerFromContext(context), std::dec, ".");

        if (g_lastCamera) {
            float worldFov{}, firstPersonFov{}, aspect{};
            std::uint8_t state{};
            if (SafeRead(g_lastCamera + kWorldFovOffset, worldFov) &&
                SafeRead(g_lastCamera + kFirstPersonFovOffset, firstPersonFov) &&
                SafeRead(g_lastCamera + kAspectOffset, aspect) &&
                SafeRead(g_lastCamera + kFirstPersonStateOffset, state)) {
                Log("ADS control: event=", eventName, " camera=0x", std::hex, g_lastCamera,
                    std::dec, " worldFOV=", worldFov, " firstPersonFOV=", firstPersonFov,
                    " aspect=", aspect, " firstPersonState=0x", std::hex,
                    static_cast<unsigned int>(state), std::dec, ".");
            }
        }

        const auto count = g_nextCandidate < g_candidates.size() ? g_nextCandidate : g_candidates.size();
        Log("ADS candidate summary: event=", eventName, " retained=", count,
            "; detailed candidate logs remain limited to assignment discovery.");
        LogAdsObjectProbe("RAX", static_cast<std::uintptr_t>(context.rax));
        LogAdsObjectProbe("RCX", static_cast<std::uintptr_t>(context.rcx));
        LogAdsObjectProbe("RSI", static_cast<std::uintptr_t>(context.rsi));
    }

    void ObserveAdsIn(SafetyHookContext& context) { LogAdsEvent("IN", context); }
    void ObserveAdsOut(SafetyHookContext& context) { LogAdsEvent("OUT", context); }

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
        Log("CAMERA control: camera=0x", std::hex, camera, std::dec,
            " worldFOV=", worldFov, " firstPersonFOV=", firstPersonFov,
            " aspect=", aspect, " firstPersonState=0x", std::hex,
            static_cast<unsigned int>(state), std::dec, ".");
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
            "STALKER2WeaponViewmodelPrimitiveAdsObserver204.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2WeaponViewmodelPrimitiveAdsObserver204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        try {
            auto* camera = ResolveUnique(kCameraWriterSignature, "camera-writer");
            auto* mesh = ResolveUnique(kMeshAssignmentSignature, "mesh-assignment");
            auto* adsIn = ResolveUnique(kAdsInSignature, "ads-in");
            auto* adsOut = ResolveUnique(kAdsOutSignature, "ads-out");
            if (!camera || !mesh || !adsIn || !adsOut)
                throw std::runtime_error("one or more required signatures did not resolve uniquely");

            g_cameraHook = safetyhook::create_mid(camera + kCameraFovWriteOffset, ObserveCamera);
            g_meshHook = safetyhook::create_mid(mesh, ObserveMeshAssignment);
            g_adsInHook = safetyhook::create_mid(adsIn, ObserveAdsIn);
            g_adsOutHook = safetyhook::create_mid(adsOut, ObserveAdsOut);
            if (!g_cameraHook || !g_meshHook || !g_adsInHook || !g_adsOutHook)
                throw std::runtime_error("one or more observer hooks were not created");
            Log("Read-only primitive/ADS observer installed; no camera, primitive or render-state writes.");
        } catch (const std::exception& error) {
            g_cameraHook.reset(); g_meshHook.reset(); g_adsInHook.reset(); g_adsOutHook.reset();
            Log("Observer setup refused safely: ", error.what(), ".");
        } catch (...) {
            g_cameraHook.reset(); g_meshHook.reset(); g_adsInHook.reset(); g_adsOutHook.reset();
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
