#include "helper.hpp"
#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace {
constexpr std::uintptr_t kReturnRva = 0x26F7A25;
constexpr std::uintptr_t kExitRva = 0x6B6C482;
constexpr std::uintptr_t kFovOffset = 0x230;
constexpr std::uintptr_t kAspectOffset = 0x254;
constexpr float kWideAspect = 5120.0f / 1440.0f;
constexpr float kCorrectionFov = 127.3927f;
constexpr std::uint8_t kReturnBytes[] = {0x84,0xC0,0x75,0xEB};
constexpr std::uint8_t kExitBytes[] = {0x56,0x57,0x53,0x48,0x83,0xEC,0x40,0x48,0x89,0xCE};
HMODULE g_module{}; HMODULE g_executable=GetModuleHandle(nullptr); std::shared_ptr<spdlog::logger> g_logger; SafetyHookMid g_returnHook,g_exitHook; std::mutex g_mutex; std::uintptr_t g_target{}; float g_baseline{}; bool g_active{}; std::uint64_t g_seq{};
template<typename T> bool Read(std::uintptr_t a,T&v){MEMORY_BASIC_INFORMATION i{};if(!a||!VirtualQuery((const void*)a,&i,sizeof(i))||i.State!=MEM_COMMIT||(i.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;auto e=(std::uintptr_t)i.BaseAddress+i.RegionSize;if(a>=e||sizeof(T)>e-a)return false;std::memcpy(&v,(const void*)a,sizeof(T));return true;}
template<typename T> bool Write(std::uintptr_t a,const T&v){MEMORY_BASIC_INFORMATION i{};if(!a||!VirtualQuery((const void*)a,&i,sizeof(i))||i.State!=MEM_COMMIT||(i.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;auto e=(std::uintptr_t)i.BaseAddress+i.RegionSize;DWORD w=PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;if(a>=e||sizeof(T)>e-a||!(i.Protect&w))return false;std::memcpy((void*)a,&v,sizeof(T));return true;}
void Log(const char*stage,std::uintptr_t t,float beforeF,float beforeA,std::uint8_t beforeFlags,bool fw,bool aw){float f{},a{};std::uint8_t m{};Read(t+kFovOffset,f);Read(t+kAspectOffset,a);Read(t+0x259,m);if(g_logger)g_logger->info("TRACE seq={} stage={} target=0x{:X} beforeFov={} beforeAspect={} beforeFlags=0x{:02X} afterFov={} afterAspect={} afterFlags=0x{:02X} fovWritten={} aspectWritten={} thread={}",++g_seq,stage,t,beforeF,beforeA,beforeFlags,f,a,m,fw,aw,GetCurrentThreadId());}
bool ResolveInner(std::uintptr_t item,std::uintptr_t&inner){return item&&Read(item+0x18,inner)&&inner;}
void Apply(SafetyHookContext&c){std::uintptr_t t{};if(!ResolveInner((std::uintptr_t)c.rcx,t))return;std::lock_guard lock(g_mutex);if(g_active)return;float f{},a{};std::uint8_t m{};if(!Read(t+kFovOffset,f)||!Read(t+kAspectOffset,a)||!Read(t+0x259,m))return;bool fw=Write(t+kFovOffset,kCorrectionFov);bool aw=Write(t+kAspectOffset,kWideAspect);if(fw&&aw){g_target=t;g_baseline=f;g_active=true;}Log("post-enter-return",t,f,a,m,fw,aw);}
void Exit(SafetyHookContext&c){std::uintptr_t ctx=(std::uintptr_t)c.rcx,t{};if(!Read(ctx+0xF8,t))return;std::lock_guard lock(g_mutex);if(!g_active||t!=g_target)return;bool ok=Write(t+kFovOffset,g_baseline);Log("exit-fov-restore",t,g_baseline,0,0,ok,false);g_target=0;g_active=false;}
DWORD WINAPI Initialize(void*){WCHAR p[MAX_PATH]{};GetModuleFileNameW(g_module,p,MAX_PATH);auto path=std::filesystem::path(p).remove_filename()/"STALKER2CinematicPostEnterEffectiveFovBoundary204.log";std::ofstream(path,std::ios::trunc).close();try{g_logger=spdlog::basic_logger_mt("STALKER2CinematicPostEnterEffectiveFovBoundary204",path.string(),true);g_logger->flush_on(spdlog::level::info);auto*r=(std::uint8_t*)g_executable+kReturnRva;auto*e=(std::uint8_t*)g_executable+kExitRva;if(std::memcmp(r,kReturnBytes,sizeof(kReturnBytes))!=0)throw std::runtime_error("post-enter return bytes mismatch");if(std::memcmp(e,kExitBytes,sizeof(kExitBytes))!=0)throw std::runtime_error("EXIT bytes mismatch");g_returnHook=safetyhook::create_mid(r,Apply);g_exitHook=safetyhook::create_mid(e,Exit);if(!g_returnHook||!g_exitHook)throw std::runtime_error("hook creation failed");g_logger->info("TRACE installed: 2.0.4 post-enter return boundary FOV/aspect feasibility; one pair, FOV-only EXIT restore.");}catch(const std::exception&x){if(g_logger)g_logger->error("TRACE setup refused: {}",x.what());g_exitHook.reset();g_returnHook.reset();}return 0;}
}
BOOL APIENTRY DllMain(HMODULE m,DWORD r,LPVOID){if(r!=DLL_PROCESS_ATTACH)return TRUE;g_module=m;DisableThreadLibraryCalls(m);if(auto t=CreateThread(nullptr,0,Initialize,nullptr,0,nullptr))CloseHandle(t);return TRUE;}
