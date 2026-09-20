#include "stdafx.h"
#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace {
constexpr std::uintptr_t kEntryRva = 0xA5A2CC;
constexpr std::uint64_t kMaxRecords = 20000;
HMODULE g_module{}; HMODULE g_executable = GetModuleHandle(nullptr);
std::shared_ptr<spdlog::logger> g_logger; SafetyHookMid g_hook; std::atomic<std::uint64_t> g_sequence{};
template <typename T> bool SafeRead(std::uintptr_t address, T& value) { MEMORY_BASIC_INFORMATION info{}; if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false; const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize; if (address > end || sizeof(value) > end - address) return false; std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value)); return true; }
bool IsExecutable(std::uintptr_t address) { MEMORY_BASIC_INFORMATION info{}; if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT) return false; const auto protect = info.Protect & 0xff; return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY; }
void Trace(SafetyHookContext& context) { const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1; if (sequence > kMaxRecords || !g_logger) return; const auto object = static_cast<std::uintptr_t>(context.rcx); std::uint8_t s248{}, s262{}, s263{}; const bool a=SafeRead(object+0x248,s248), b=SafeRead(object+0x262,s262), c=SafeRead(object+0x263,s263); std::uintptr_t caller{}; SafeRead(static_cast<std::uintptr_t>(context.rsp),caller); g_logger->info("TRACE seq={} object=0x{:X} caller=0x{:X} state248={} state262=0x{:X} state263=0x{:X} reads={}/{}/{}",sequence,object,caller,a?s248:0,b?s262:0,c?s263:0,a,b,c); }
DWORD WINAPI Initialize(void*) { WCHAR modulePath[MAX_PATH]{}; GetModuleFileNameW(g_module,modulePath,MAX_PATH); const auto path=std::filesystem::path(modulePath).remove_filename()/"STALKER2AimRenderStateTrace.log"; std::ofstream(path,std::ios::out|std::ios::trunc).close(); try { g_logger=spdlog::basic_logger_mt("STALKER2AimRenderStateTrace",path.string(),true); g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v"); g_logger->flush_on(spdlog::level::info); } catch (...) { return 0; } const auto target=reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(g_executable)+kEntryRva); if(!IsExecutable(reinterpret_cast<std::uintptr_t>(target))){g_logger->error("TRACE setup refused: Steam 2.0.2 RVA is not executable.");return 0;} try {g_hook=safetyhook::create_mid(target,Trace);if(!g_hook)throw std::runtime_error("hook creation failed");g_logger->info("TRACE hook installed: FUN_140A5A2CC aim/render state consumer.");}catch(...){g_hook.reset();g_logger->error("TRACE setup refused safely; rollback completed.");} return 0; }
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){if(reason!=DLL_PROCESS_ATTACH)return TRUE;g_module=module;DisableThreadLibraryCalls(module);const auto thread=CreateThread(nullptr,0,Initialize,nullptr,0,nullptr);if(thread)CloseHandle(thread);return TRUE;}
