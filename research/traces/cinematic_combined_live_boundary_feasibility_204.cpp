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
    constexpr std::uintptr_t kAspectStoreRva = 0x6B7CB05;
    constexpr std::uintptr_t kFovConsumerRva = 0x2EE6936;
    constexpr std::size_t kImmediateOffset = 6;
    constexpr std::uint8_t kStorePrefix[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00 };
    constexpr std::uint8_t kOriginalImmediate[] = { 0x39, 0x8E, 0xE3, 0x3F };
    constexpr std::uint8_t kFovConsumerBytes[] = { 0xE8, 0x71, 0x71, 0xC8, 0x03 };
    constexpr float kNativeAspect = 16.0f / 9.0f;
    constexpr float kMinAspect = 1.0f;
    constexpr float kMaxAspect = 8.0f;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_fovHook;
    std::uint8_t* g_store{};
    std::uint8_t g_originalImmediate[sizeof(kOriginalImmediate)]{};
    std::atomic<bool> g_fovApplied{};
    bool g_aspectPatched{};

    bool ReadDisplaySize(LONG& width, LONG& height, const char*& sourceKind)
    {
        HWND window = nullptr;
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
        if (window && GetClientRect(window, &rect)) {
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
            if (width > 0 && height > 0) {
                sourceKind = "client";
                return true;
            }
        }
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
        sourceKind = "desktop";
        return width > 0 && height > 0;
    }

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT) return false;
        const auto protection = info.Protect & 0xFF;
        return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    }

    std::uintptr_t Base(std::uintptr_t rva)
    {
        return reinterpret_cast<std::uintptr_t>(g_executable) + rva;
    }

    float HorPlus(float fov, float aspect)
    {
        constexpr float pi = 3.14159265358979323846f;
        const float half = fov * (pi / 360.0f);
        return 2.0f * std::atan(std::tan(half) * (aspect / kNativeAspect)) * (180.0f / pi);
    }

    bool PatchAspect(float aspect)
    {
        if (!g_store || !IsExecutable(reinterpret_cast<std::uintptr_t>(g_store)) ||
            std::memcmp(g_store, kStorePrefix, sizeof(kStorePrefix)) != 0 ||
            std::memcmp(g_store + kImmediateOffset, kOriginalImmediate, sizeof(kOriginalImmediate)) != 0)
            return false;
        std::memcpy(g_originalImmediate, g_store + kImmediateOffset, sizeof(g_originalImmediate));
        std::uint32_t bits{};
        std::memcpy(&bits, &aspect, sizeof(bits));
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(g_store, &info, sizeof(info))) return false;
        DWORD oldProtection{};
        if (!VirtualProtect(info.BaseAddress, info.RegionSize, PAGE_EXECUTE_READWRITE, &oldProtection)) return false;
        std::memcpy(g_store + kImmediateOffset, &bits, sizeof(bits));
        FlushInstructionCache(GetCurrentProcess(), g_store, kImmediateOffset + sizeof(bits));
        DWORD ignored{};
        VirtualProtect(info.BaseAddress, info.RegionSize, oldProtection, &ignored);
        g_aspectPatched = true;
        return true;
    }

    void RestoreAspect()
    {
        if (!g_aspectPatched || !g_store) return;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(g_store, &info, sizeof(info))) return;
        DWORD oldProtection{};
        if (!VirtualProtect(info.BaseAddress, info.RegionSize, PAGE_EXECUTE_READWRITE, &oldProtection)) return;
        std::memcpy(g_store + kImmediateOffset, g_originalImmediate, sizeof(g_originalImmediate));
        FlushInstructionCache(GetCurrentProcess(), g_store, kImmediateOffset + sizeof(g_originalImmediate));
        DWORD ignored{};
        VirtualProtect(info.BaseAddress, info.RegionSize, oldProtection, &ignored);
        g_aspectPatched = false;
    }

    void TransformFov(SafetyHookContext& context)
    {
        bool expected = false;
        if (!g_fovApplied.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
        const float before = context.xmm0.f32[0];
        LONG width{}, height{};
        const char* sourceKind = "unknown";
        ReadDisplaySize(width, height, sourceKind);
        const float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 0.0f;
        const float after = std::isfinite(before) && std::isfinite(aspect) &&
            before > 1.0f && before < 179.0f && aspect >= kMinAspect && aspect <= kMaxAspect
            ? HorPlus(before, aspect) : before;
        if (after != before && std::isfinite(after)) context.xmm0.f32[0] = after;
        if (g_logger) g_logger->info(
            "TRACE stage=enter-live-fov-transform callRva=0x{:X} beforeXmm0={} afterXmm0={} aspect={} sourceKind={} size={}x{} transformed={} thread={}",
            kFovConsumerRva, before, context.xmm0.f32[0], aspect, sourceKind, width, height,
            after != before, GetCurrentThreadId());
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicCombinedLiveBoundaryFeasibility204.log";
        std::ofstream(path, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicCombinedLiveBoundaryFeasibility204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        g_store = reinterpret_cast<std::uint8_t*>(Base(kAspectStoreRva));
        auto* fovConsumer = reinterpret_cast<std::uint8_t*>(Base(kFovConsumerRva));
        LONG width{}, height{};
        const char* sourceKind = "unknown";
        if (!ReadDisplaySize(width, height, sourceKind)) {
            g_logger->error("TRACE setup refused: display dimensions unavailable; no code patch.");
            return 0;
        }
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        if (!std::isfinite(aspect) || aspect < kMinAspect || aspect > kMaxAspect ||
            std::memcmp(fovConsumer, kFovConsumerBytes, sizeof(kFovConsumerBytes)) != 0) {
            g_logger->error("TRACE setup refused: aspect/callsite validation failed; no code patch.");
            return 0;
        }
        if (!PatchAspect(aspect)) {
            g_logger->error("TRACE setup refused: exact aspect-store validation or patch failed.");
            return 0;
        }
        try {
            g_fovHook = safetyhook::create_mid(fovConsumer, TransformFov);
            if (!g_fovHook) throw std::runtime_error("live FOV hook creation failed");
            g_logger->info("TRACE installed: combined live boundary feasibility; aspectRva=0x{:X} fovRva=0x{:X} aspect={} sourceKind={} size={}x{} aspectImmediateBytes=4 cameraWrites=0", kAspectStoreRva, kFovConsumerRva, aspect, sourceKind, width, height);
        } catch (const std::exception& error) {
            g_fovHook.reset();
            RestoreAspect();
            g_logger->error("TRACE setup refused safely: {}; aspect restored.", error.what());
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        RestoreAspect();
    }
    return TRUE;
}
