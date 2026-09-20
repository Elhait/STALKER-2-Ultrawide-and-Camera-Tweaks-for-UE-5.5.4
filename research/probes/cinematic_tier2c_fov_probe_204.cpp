#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

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
    constexpr std::size_t kSetterOffset = 0x19;
    constexpr std::uint8_t kSetterBytes[] = {
        0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F,
    };
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kFlagsOffset = 0x259;
    constexpr std::uintptr_t kFovOffset = 0x230;
    constexpr std::uintptr_t kExitCallbackRva = 0x6B6C482;
    constexpr std::uint8_t kExitCallbackBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE,
    };
    constexpr float kCinematicAspect = 16.0f / 9.0f;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_setterHook;
    SafetyHookMid g_exitHook;
    std::mutex g_mutex;
    std::uintptr_t g_target{};
    float g_baselineFov{};
    bool g_active{};

    template <typename T>
    bool Read(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(T) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    template <typename T>
    bool Write(std::uintptr_t address, const T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if (address >= end || sizeof(T) > end - address || !(info.Protect & writable)) return false;
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
        return true;
    }

    std::uintptr_t Rva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    void ClientAspect(float& aspect)
    {
        aspect = 0.0f;
        HWND window{};
        EnumWindows([](HWND candidate, LPARAM parameter) {
            DWORD processId{};
            GetWindowThreadProcessId(candidate, &processId);
            if (processId == GetCurrentProcessId() && IsWindowVisible(candidate) &&
                GetWindow(candidate, GW_OWNER) == nullptr) {
                *reinterpret_cast<HWND*>(parameter) = candidate;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&window));
        RECT rect{};
        if (window && GetClientRect(window, &rect) && rect.bottom > 0)
            aspect = static_cast<float>(rect.right) / static_cast<float>(rect.bottom);
        if (aspect <= 0.0f) {
            DEVMODE display{ .dmSize = sizeof(DEVMODE) };
            if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &display) && display.dmPelsHeight > 0)
                aspect = static_cast<float>(display.dmPelsWidth) / static_cast<float>(display.dmPelsHeight);
        }
        if (aspect <= 0.0f) {
            const int width = GetSystemMetrics(SM_CXSCREEN);
            const int height = GetSystemMetrics(SM_CYSCREEN);
            if (width > 0 && height > 0) aspect = static_cast<float>(width) / static_cast<float>(height);
        }
    }

    bool HorPlus(float source, float aspect, float& converted)
    {
        if (!std::isfinite(source) || source <= 1.0f || source >= 179.0f ||
            !std::isfinite(aspect) || aspect < 0.5f || aspect > 20.0f) return false;
        constexpr float pi = 3.14159265358979323846f;
        const float halfRadians = source * pi / 360.0f;
        converted = 2.0f * std::atan(std::tan(halfRadians) * (aspect / kCinematicAspect)) * 180.0f / pi;
        return std::isfinite(converted) && converted > 1.0f && converted < 179.0f;
    }

    void Log(const char* stage, std::uintptr_t target, float baseline, float value, bool applied)
    {
        if (!g_logger) return;
        float aspect{};
        std::uint8_t flags{};
        Read(target + kAspectOffset, aspect);
        Read(target + kFlagsOffset, flags);
        g_logger->info("TRACE stage={} target=0x{:X} aspect={} flags=0x{:02X} baselineFov={} valueFov={} applied={} thread={}",
            stage, target, aspect, flags, baseline, value, applied, GetCurrentThreadId());
    }

    void Enter(SafetyHookContext& context)
    {
        const auto target = static_cast<std::uintptr_t>(context.rax);
        if (!target) return;

        float nativeAspect{};
        std::uint8_t flags{};
        float baseline{};
        if (!Read(target + kAspectOffset, nativeAspect) || !Read(target + kFlagsOffset, flags) ||
            !Read(target + kFovOffset, baseline)) return;

        float displayAspect{};
        ClientAspect(displayAspect);
        float corrected{};
#ifdef CINEMATIC_ASPECT_ONLY
        const bool candidate = flags == 0x05 && displayAspect > kCinematicAspect + 0.5f;
        corrected = baseline;
#else
        const bool candidate = flags == 0x05 && displayAspect > kCinematicAspect + 0.5f &&
            HorPlus(baseline, displayAspect, corrected);
#endif
        bool applied = false;
        if (candidate) {
            std::lock_guard lock(g_mutex);
#ifdef CINEMATIC_ASPECT_ONLY
            if (!g_active && Write(target + kAspectOffset, displayAspect)) {
                g_target = target;
                g_baselineFov = baseline;
                g_active = true;
                applied = true;
            }
#else
            if (!g_active && Write(target + kFovOffset, corrected) &&
                Write(target + kAspectOffset, displayAspect)) {
                g_target = target;
                g_baselineFov = baseline;
                g_active = true;
                applied = true;
            } else if (!g_active) {
                Write(target + kFovOffset, baseline);
            }
#endif
        }
        Log("enter-correction", target, baseline, corrected, applied);
        if (g_logger) g_logger->info("TRACE stage=enter-correction-input target=0x{:X} displayAspect={} candidate={} nativeAspect={} flags=0x{:02X} baselineFov={} thread={}",
            target, displayAspect, candidate, nativeAspect, flags, baseline, GetCurrentThreadId());
        context.rip += sizeof(kSetterBytes);
    }

    void Exit(SafetyHookContext& context)
    {
        const auto callbackContext = static_cast<std::uintptr_t>(context.rcx);
        std::uintptr_t target{};
        if (!Read(callbackContext + 0xF8, target) || !target) return;
        std::lock_guard lock(g_mutex);
        if (g_active && target == g_target) {
#ifdef CINEMATIC_ASPECT_ONLY
            Log("exit-bookkeeping-clear", target, g_baselineFov, g_baselineFov, true);
#else
            const float baseline = g_baselineFov;
            const bool restored = Write(target + kFovOffset, baseline);
            Log("exit-restore", target, baseline, baseline, restored);
#endif
            g_active = false;
            g_target = 0;
        }
    }

    bool ResolveSetter(std::uint8_t*& setter)
    {
        const auto matches = Memory::PatternScanAll(g_executable,
            "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");
        if (matches.size() != 1) return false;
        setter = matches.front() + kSetterOffset;
        return std::memcmp(setter, kSetterBytes, sizeof(kSetterBytes)) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2CinematicTier2CFovProbe204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicTier2CFovProbe204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        std::uint8_t* setter{};
        if (!ResolveSetter(setter)) {
            g_logger->error("TRACE setup refused: current 2.0.4 ENTER setter validation failed.");
            return 0;
        }
        auto* exitCallback = reinterpret_cast<std::uint8_t*>(g_executable) + kExitCallbackRva;
        if (std::memcmp(exitCallback, kExitCallbackBytes, sizeof(kExitCallbackBytes)) != 0) {
            g_logger->error("TRACE setup refused: current 2.0.4 EXIT callback validation failed.");
            return 0;
        }
        try {
            g_setterHook = safetyhook::create_mid(setter, Enter);
            g_exitHook = safetyhook::create_mid(exitCallback, Exit);
            if (!g_setterHook || !g_exitHook) throw std::runtime_error("hook creation failed");
#ifdef CINEMATIC_ASPECT_ONLY
            g_logger->info("TRACE installed: Tier 2C 2.0.4 aspect-only feasibility probe; one ENTER aspect write and native EXIT restore.");
#else
            g_logger->info("TRACE installed: Tier 2C 2.0.4 combined aspect/FOV feasibility probe; one ENTER write and one EXIT restore.");
#endif
        } catch (...) {
            g_exitHook.reset();
            g_setterHook.reset();
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
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
