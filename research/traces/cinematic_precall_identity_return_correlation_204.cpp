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

namespace {
constexpr std::uintptr_t kDispatchRva=0x26F7A23,kReturnRva=0x26F7A25,kExitRva=0x6B6C482;
constexpr std::uintptr_t kFov=0x230,kAspect=0x254,kFlags=0x259;
constexpr float kWide=5120.0f/1440.0f,kNative=16.0f/9.0f,kCorrected=127.3927f;
constexpr std::uint8_t kDispatchBytes[]={0xFF,0xD0},kReturnBytes[]={0x84,0xC0,0x75,0xEB},kExitBytes[]={0x56,0x57,0x53,0x48,0x83,0xEC,0x40,0x48,0x89,0xCE};
HMODULE g_module{}; HMODULE g_executable=GetModuleHandle(nullptr); std::shared_ptr<spdlog::logger> g_logger; SafetyHookMid g_dispatchHook,g_returnHook,g_exitHook; std::mutex g_mutex; std::uintptr_t g_setter{}; std::uintptr_t g_target{}; float g_baseline{}; bool g_active{}; std::uint64_t g_seq{};
struct Pending { bool active{}; DWORD thread{}; std::uintptr_t inner{}; }; thread_local Pending g_pending;
template<typename T> bool Read(std::uintptr_t a,T&v){MEMORY_BASIC_INFORMATION i{};if(!a||!VirtualQuery((const void*)a,&i,sizeof(i))||i.State!=MEM_COMMIT||(i.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;auto e=(std::uintptr_t)i.BaseAddress+i.RegionSize;if(a>=e||sizeof(T)>e-a)return false;std::memcpy(&v,(const void*)a,sizeof(T));return true;}
template<typename T> bool Write(std::uintptr_t a,const T&v){MEMORY_BASIC_INFORMATION i{};if(!a||!VirtualQuery((const void*)a,&i,sizeof(i))||i.State!=MEM_COMMIT||(i.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;auto e=(std::uintptr_t)i.BaseAddress+i.RegionSize;DWORD w=PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;if(a>=e||sizeof(T)>e-a||!(i.Protect&w))return false;std::memcpy((void*)a,&v,sizeof(T));return true;}
bool State(std::uintptr_t t,float&f,float&a,std::uint8_t&m){return Read(t+kFov,f)&&Read(t+kAspect,a)&&Read(t+kFlags,m)&&std::isfinite(f)&&f>1.0f&&f<179.0f;}
void Log(const char*s,std::uintptr_t t,float bf,float ba,std::uint8_t bm,bool fw,bool aw){float f{},a{};std::uint8_t m{};Read(t+kFov,f);Read(t+kAspect,a);Read(t+kFlags,m);if(g_logger)g_logger->info("TRACE seq={} stage={} target=0x{:X} beforeFov={} beforeAspect={} beforeFlags=0x{:02X} afterFov={} afterAspect={} afterFlags=0x{:02X} fovWritten={} aspectWritten={} thread={}",++g_seq,s,t,bf,ba,bm,f,a,m,fw,aw,GetCurrentThreadId());}
void PreCall(SafetyHookContext&c){g_pending={};if((std::uintptr_t)c.rax!=g_setter)return;std::uintptr_t inner{};if(!Read((std::uintptr_t)c.rcx+0x18,inner)||!inner)return;g_pending={true,GetCurrentThreadId(),inner};if(g_logger)g_logger->info("TRACE stage=pre-call-capture item=0x{:X} inner=0x{:X} setter=0x{:X} thread={}",(std::uintptr_t)c.rcx,inner,g_setter,GetCurrentThreadId());}
void PostCall(SafetyHookContext&){if(!g_pending.active||g_pending.thread!=GetCurrentThreadId())return;const auto t=g_pending.inner;float f{},a{};std::uint8_t m{};const bool valid=State(t,f,a,m)&&std::fabs(a-kNative)<0.001f&&m==0x05;if(!valid){if(g_logger)g_logger->warn("TRACE stage=post-return-rejected inner=0x{:X} aspect={} flags=0x{:02X} fov={} thread={}",t,a,m,f,GetCurrentThreadId());g_pending={};return;}std::lock_guard lock(g_mutex);if(!g_active){const bool fw=Write(t+kFov,kCorrected),aw=Write(t+kAspect,kWide);if(fw&&aw){g_target=t;g_baseline=f;g_active=true;}Log("post-return",t,f,a,m,fw,aw);}g_pending={};}
void Exit(SafetyHookContext&c){std::uintptr_t ctx=(std::uintptr_t)c.rcx,t{};if(!Read(ctx+0xF8,t))return;std::lock_guard lock(g_mutex);if(!g_active||t!=g_target)return;const bool ok=Write(t+kFov,g_baseline);Log("exit-fov-restore",t,g_baseline,0,0,ok,false);g_target=0;g_active=false;}
DWORD WINAPI Initialize(void*){WCHAR p[MAX_PATH]{};GetModuleFileNameW(g_module,p,MAX_PATH);auto path=std::filesystem::path(p).remove_filename()/"STALKER2CinematicPrecallIdentityReturnCorrelation204.log";std::ofstream(path,std::ios::trunc).close();try{g_logger=spdlog::basic_logger_mt("STALKER2CinematicPrecallIdentityReturnCorrelation204",path.string(),true);g_logger->flush_on(spdlog::level::info);auto ms=Memory::PatternScanAll(g_executable,"48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3");if(ms.size()!=1)throw std::runtime_error("ENTER signature ambiguous");g_setter=(std::uintptr_t)(ms.front()+0x19);auto*b=(std::uint8_t*)g_executable+kDispatchRva,*r=(std::uint8_t*)g_executable+kReturnRva,*e=(std::uint8_t*)g_executable+kExitRva;if(std::memcmp(b,kDispatchBytes,2)||std::memcmp(r,kReturnBytes,4)||std::memcmp(e,kExitBytes,sizeof(kExitBytes)))throw std::runtime_error("correlation anchor bytes mismatch");g_dispatchHook=safetyhook::create_mid(b,PreCall);g_returnHook=safetyhook::create_mid(r,PostCall);g_exitHook=safetyhook::create_mid(e,Exit);if(!g_dispatchHook||!g_returnHook||!g_exitHook)throw std::runtime_error("hook creation failed");g_logger->info("TRACE installed: 2.0.4 pre-call identity to return correlation; one FOV/aspect pair, FOV-only EXIT restore.");}catch(const std::exception&x){if(g_logger)g_logger->error("TRACE setup refused: {}",x.what());g_exitHook.reset();g_returnHook.reset();g_dispatchHook.reset();}return 0;}
}
BOOL APIENTRY DllMain(HMODULE m,DWORD r,LPVOID){if(r!=DLL_PROCESS_ATTACH)return TRUE;g_module=m;DisableThreadLibraryCalls(m);if(auto t=CreateThread(nullptr,0,Initialize,nullptr,0,nullptr))CloseHandle(t);return TRUE;}
