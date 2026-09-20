#include "helper.hpp"

#include <spdlog/sinks/basic_file_sink.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

namespace
{
    constexpr std::uintptr_t kStoreRva = 0x6B7CB05;
    constexpr std::size_t kImmediateOffset = 6;
    constexpr std::uint8_t kStorePrefix[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00 };
    constexpr std::uint8_t kOriginalImmediate[] = { 0x39, 0x8E, 0xE3, 0x3F };
    constexpr float kMinAspect = 1.0f;
    constexpr float kMaxAspect = 8.0f;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    std::uint8_t* g_store{};
    bool g_patched{};
    std::uint8_t g_originalBytes[sizeof(kOriginalImmediate)]{};

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

    bool PatchImmediate(float aspect)
    {
        if (!g_store || !IsExecutable(reinterpret_cast<std::uintptr_t>(g_store)) ||
            std::memcmp(g_store, kStorePrefix, sizeof(kStorePrefix)) != 0) return false;
        if (std::memcmp(g_store + kImmediateOffset, kOriginalImmediate, sizeof(kOriginalImmediate)) != 0) return false;
        std::memcpy(g_originalBytes, g_store + kImmediateOffset, sizeof(g_originalBytes));
        std::uint32_t bits{};
        std::memcpy(&bits, &aspect, sizeof(bits));
        DWORD oldProtection{};
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(g_store, &info, sizeof(info))) return false;
        if (!VirtualProtect(info.BaseAddress, info.RegionSize, PAGE_EXECUTE_READWRITE, &oldProtection)) return false;
        std::memcpy(g_store + kImmediateOffset, &bits, sizeof(bits));
        FlushInstructionCache(GetCurrentProcess(), g_store, kImmediateOffset + sizeof(bits));
        DWORD ignored{};
        VirtualProtect(info.BaseAddress, info.RegionSize, oldProtection, &ignored);
        g_patched = true;
        return true;
    }

    void RestoreImmediate()
    {
        if (!g_patched || !g_store) return;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(g_store, &info, sizeof(info))) return;
        DWORD oldProtection{};
        if (!VirtualProtect(info.BaseAddress, info.RegionSize, PAGE_EXECUTE_READWRITE, &oldProtection)) return;
        std::memcpy(g_store + kImmediateOffset, g_originalBytes, sizeof(g_originalBytes));
        FlushInstructionCache(GetCurrentProcess(), g_store, kImmediateOffset + sizeof(g_originalBytes));
        DWORD ignored{};
        VirtualProtect(info.BaseAddress, info.RegionSize, oldProtection, &ignored);
        g_patched = false;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicAspectImmediatePatchFeasibility204.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicAspectImmediatePatchFeasibility204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        g_store = reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(g_executable) + kStoreRva);
        LONG width{}, height{};
        const char* sourceKind = "unknown";
        if (!ReadDisplaySize(width, height, sourceKind)) {
            g_logger->error("TRACE setup refused: display dimensions unavailable; no code patch.");
            return 0;
        }
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        if (!std::isfinite(aspect) || aspect < kMinAspect || aspect > kMaxAspect) {
            g_logger->error("TRACE setup refused: client aspect out of bounds; no code patch.");
            return 0;
        }
        if (!PatchImmediate(aspect)) {
            g_logger->error("TRACE setup refused: exact ENTER store bytes mismatch or patch failed.");
            return 0;
        }
        g_logger->info("TRACE installed: 2.0.4 aspect immediate feasibility; storeRva=0x{:X} aspect={} sourceKind={} size={}x{} patchedBytes=4 cameraWrites=0", kStoreRva, aspect, sourceKind, width, height);
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
        RestoreImmediate();
    }
    return TRUE;
}
