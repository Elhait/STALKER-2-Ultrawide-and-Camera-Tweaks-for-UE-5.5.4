#include "helper.hpp"
#include <safetyhook.hpp>
#include <Zydis.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace {
constexpr std::uintptr_t kCallbackRva=0x6B6C482;
constexpr std::uint8_t kCallbackBytes[]={0x56,0x57,0x53,0x48,0x83,0xEC,0x40,0x48,0x89,0xCE};
constexpr std::uint8_t kWriterSig[]={
0xF6,0x86,0x62,0x02,0,0,0x10,0xF3,0x0F,0x10,0x86,0x30,0x02,0,0,
0x0F,0x85,0,0,0,0,0x48,0x8D,0x4B,0x30,0xF3,0x0F,0x11,0x43,0x30,
0xF3,0x0F,0x10,0x86,0x54,0x02,0,0,0xF3,0x0F,0x11,0x43,0x5C,
0x0F,0xB6,0x96,0x59,0x02,0,0,0x8B,0x43,0x68,0x83,0xE2,1,0x83,0xE0,0xFE,
9,0xD0,0x89,0x43,0x68,0x0F,0xB6,0x96,0x59,0x02,0,0,0x83,0xE2,4,0x83,0xE0,0xFB,
9,0xD0,0x89,0x43,0x68,0x8A,0x96,0x63,0x02,0,0,0x88,0x53,0x6C};
constexpr std::size_t kWriterObserveOffset=88;
HMODULE g_module{}; HMODULE g_executable=GetModuleHandle(nullptr);
std::shared_ptr<spdlog::logger> g_logger; SafetyHookMid g_callbackHook,g_writerHook;
std::atomic<std::uint64_t> g_sequence{};
thread_local std::uintptr_t g_target{}; thread_local std::uint64_t g_seq{};

template<class T> bool Read(std::uintptr_t a,T& v){MEMORY_BASIC_INFORMATION i{};if(!a||!VirtualQuery((void*)a,&i,sizeof(i))||i.State!=MEM_COMMIT||(i.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;auto e=(std::uintptr_t)i.BaseAddress+i.RegionSize;if(a>=e||sizeof(T)>e-a)return false;std::memcpy(&v,(void*)a,sizeof(T));return true;}
bool Exec(std::uintptr_t a){MEMORY_BASIC_INFORMATION i{};if(!a||!VirtualQuery((void*)a,&i,sizeof(i))||i.State!=MEM_COMMIT)return false;return (i.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;}
std::uintptr_t Rva(std::uintptr_t a){auto b=(std::uintptr_t)g_executable;return a>=b&&a-b<0x20000000?a-b:0;}
std::uintptr_t StackRva(std::uintptr_t rsp){std::uintptr_t value{};return Read(rsp,value)&&Exec(value)?Rva(value):0;}
void Client(LONG& w,LONG& h){HWND x=nullptr;EnumWindows([](HWND c,LPARAM p){DWORD id{};GetWindowThreadProcessId(c,&id);if(id==GetCurrentProcessId()&&IsWindowVisible(c)&&!GetWindow(c,GW_OWNER)){*(HWND*)p=c;return FALSE;}return TRUE;},(LPARAM)&x);RECT r{};if(x&&GetClientRect(x,&r)){w=r.right-r.left;h=r.bottom-r.top;}}
void Callback(SafetyHookContext& c){auto ctx=(std::uintptr_t)c.rcx;std::uintptr_t t{};if(!Read(ctx+0xF8,t)||!t){g_target=0;return;}g_target=t;g_seq=++g_sequence;float a{},f{};std::uint8_t m{};Read(t+0x254,a);Read(t+0x259,m);Read(t+0x30,f);LONG w=0,h=0;Client(w,h);if(g_logger)g_logger->info("TRACE seq={} stage=exit-callback-entry thread={} context=0x{:X} target=0x{:X} aspect={} flags=0x{:02X} fov={} returnRva=0x{:X} client={}x{}",g_seq,GetCurrentThreadId(),ctx,t,a,m,f,StackRva((std::uintptr_t)c.rsp),w,h);}
void Writer(SafetyHookContext& c){auto t=g_target;if(!t||(std::uintptr_t)c.rsi!=t)return;auto s=g_seq;g_target=0;g_seq=0;auto o=(std::uintptr_t)c.rbx;float a{},oa{},of{};std::uint8_t m{};std::uint32_t om{};bool ok=Read(t+0x254,a)&&Read(t+0x259,m)&&Read(o+0x5C,oa)&&Read(o+0x30,of)&&Read(o+0x68,om);LONG w=0,h=0;Client(w,h);if(g_logger)g_logger->info("TRACE seq={} stage=first-writer-after-callback source=0x{:X} output=0x{:X} sourceAspect={} sourceFlags=0x{:02X} outputAspect={} outputFlags=0x{:X} outputFov={} reads={} writerRva=0x{:X} client={}x{}",s,(std::uintptr_t)c.rsi,o,a,m,oa,om,of,ok,Rva((std::uintptr_t)c.rip),w,h);}
bool FindWriter(std::uint8_t*& out){std::vector<std::uint8_t*> v;Memory::ForEachExecutableSection(g_executable,[&](std::uint8_t* s,std::size_t n){for(std::size_t p=0;p+sizeof(kWriterSig)<=n;++p){bool ok=true;for(std::size_t i=0;i<sizeof(kWriterSig);++i)if(i<17||i>=21)if(s[p+i]!=kWriterSig[i]){ok=false;break;}if(ok)v.push_back(s+p);}});if(v.size()!=1)return false;out=v[0]+kWriterObserveOffset;return out[0]==0x88&&out[1]==0x53&&out[2]==0x6C;}
DWORD WINAPI Init(void*){WCHAR p[MAX_PATH]{};GetModuleFileNameW(g_module,p,MAX_PATH);auto lp=std::filesystem::path(p).remove_filename()/"STALKER2CinematicExitCausalTrace.log";std::ofstream(lp,std::ios::trunc).close();try{g_logger=spdlog::basic_logger_mt("STALKER2CinematicExitCausalTrace",lp.string(),true);g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");g_logger->flush_on(spdlog::level::info);}catch(...){return 0;}auto* cb=(std::uint8_t*)((std::uintptr_t)g_executable+kCallbackRva);std::uint8_t* wr{};if(!Exec((std::uintptr_t)cb)||std::memcmp(cb,kCallbackBytes,sizeof(kCallbackBytes))||!FindWriter(wr)){g_logger->error("TRACE setup refused: current 2.0.4 callback/writer validation failed.");return 0;}try{g_callbackHook=safetyhook::create_mid(cb,Callback);g_writerHook=safetyhook::create_mid(wr,Writer);if(!g_callbackHook||!g_writerHook)throw std::runtime_error("hook creation failed");if(!g_callbackHook.enable()||!g_writerHook.enable())throw std::runtime_error("hook enable failed");g_logger->info("TRACE installed: 2.0.4 exit callback to first matching writer; read-only.");}catch(...){g_writerHook.reset();g_callbackHook.reset();g_logger->error("TRACE setup refused safely; partial hooks rolled back.");}return 0;}
}
BOOL APIENTRY DllMain(HMODULE m,DWORD r,LPVOID){if(r!=DLL_PROCESS_ATTACH)return TRUE;g_module=m;DisableThreadLibraryCalls(m);if(auto t=CreateThread(nullptr,0,Init,nullptr,0,nullptr))CloseHandle(t);return TRUE;}
