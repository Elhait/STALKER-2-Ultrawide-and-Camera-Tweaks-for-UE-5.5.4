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
#include <sstream>

namespace {
constexpr std::uintptr_t kAdsInRva=0x6ABE7E,kAdsOutRva=0x6AC01B,kCallRva=0xAF42A7,kPostRva=0xAF50AF,kEnterRva=0x2EE6936,kExitRva=0x2EE69A7;
constexpr std::uint8_t kAdsIn[]={0xF3,0x0F,0x10,0x40,0x4C,0xF3,0x0F,0x10,0x8E,0xF8,0,0,0,0x0F,0x2E,0xC8};
constexpr std::uint8_t kAdsOut[]={0xF3,0x0F,0x10,0x40,0x50,0xF3,0x0F,0x10,0x8E,0xFC,0,0,0,0x0F,0x2E,0xC8};
constexpr std::uint8_t kCall[]={0xE8,0xF8,0x0C,0,0};
constexpr std::uint8_t kPrologue[]={0x56,0x57,0x53,0x48,0x83,0xEC,0x40,0x0F};
constexpr std::uint8_t kEnter[]={0xE8,0x71,0x71,0xC8,0x03},kExit[]={0xE8,0,0x71,0xC8,0x03};
HMODULE gModule{},gExe=GetModuleHandle(nullptr); std::shared_ptr<spdlog::logger> gLog; SafetyHookMid gIn,gOut,gCall,gEnter,gExit; std::atomic<std::uintptr_t> gSource{},gOutput{}; std::atomic<std::uint64_t> gSeq{}; std::atomic<int> gAds{};
template<class... T> void Log(T&&...v){if(!gLog)return;std::ostringstream s;(s<<...<<v);gLog->info("{}",s.str());}
template<class T> bool Read(std::uintptr_t a,T&v){MEMORY_BASIC_INFORMATION i{};if(!a||!VirtualQuery((void*)a,&i,sizeof(i))||i.State!=MEM_COMMIT||(i.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;auto e=(std::uintptr_t)i.BaseAddress+i.RegionSize;if(a>e||sizeof(v)>e-a)return false;std::memcpy(&v,(void*)a,sizeof(v));return true;}
bool Match(std::uintptr_t a,const std::uint8_t*b,std::size_t n){for(std::size_t i=0;i<n;++i){std::uint8_t v{};if(!Read(a+i,v)||v!=b[i])return false;}return true;}
const char* AdsName(){return gAds.load()==1?"IN":gAds.load()==2?"OUT":"NONE";}
void Snapshot(const char*stage,std::uintptr_t s,std::uintptr_t o){float s230{},s234{},s25c{},o30{},o38{},d0{},d4{},d8{};std::uint8_t s260{},s261{},s262{};bool ok=Read(s+0x230,s230)&&Read(s+0x234,s234)&&Read(s+0x25c,s25c)&&Read(s+0x260,s260)&&Read(s+0x261,s261)&&Read(s+0x262,s262)&&Read(o+0x30,o30)&&Read(o+0x38,o38)&&Read(o+0x9D0,d0)&&Read(o+0x9D4,d4)&&Read(o+0x9D8,d8);const float abs25c=std::fabs(s25c);const bool projection=!std::isfinite(s25c)||abs25c>1.0e-8f;Log("Consumer ",stage," seq=",gSeq.load()," adsPhase=",AdsName()," source=0x",std::hex,s," output=0x",o,std::dec," source230=",s230," source234=",s234," source25C=",s25c," abs25C=",abs25c," predicate=",projection?"projection":"early"," source260=0x",std::hex,(int)s260," source261=0x",(int)s261," source262=0x",(int)s262,std::dec," output30=",o30," output38=",o38," output9D0=",d0," output9D4=",d4," output9D8=",d8," reads=",ok);}
void In(SafetyHookContext&){gAds.store(1);Log("ADS marker: phase=IN.");} void Out(SafetyHookContext&){gAds.store(2);Log("ADS marker: phase=OUT.");}
void Enter(SafetyHookContext&){gAds.store(0);Log("Cinematic marker: ENTER.");} void Exit(SafetyHookContext&){gAds.store(0);Log("Cinematic marker: EXIT.");}
void Call(SafetyHookContext&c){auto s=(std::uintptr_t)c.rsi,o=(std::uintptr_t)c.rbx;gSeq.fetch_add(1);gSource.store(s);gOutput.store(o);Snapshot("PRE",s,o);}
DWORD WINAPI Init(void*){WCHAR p[MAX_PATH]{};GetModuleFileNameW(gModule,p,MAX_PATH);auto path=std::filesystem::path(p).remove_filename()/"STALKER2AdsWorldFovConsumerRuntimeGate204.log";std::ofstream(path,std::ios::trunc).close();try{gLog=spdlog::basic_logger_mt("STALKER2AdsWorldFovConsumerRuntimeGate204",path.string(),true);gLog->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");gLog->flush_on(spdlog::level::info);}catch(...){return 0;}auto b=(std::uintptr_t)gExe;bool ok=Match(b+kAdsInRva,kAdsIn,sizeof(kAdsIn))&&Match(b+kAdsOutRva,kAdsOut,sizeof(kAdsOut))&&Match(b+kCallRva,kCall,sizeof(kCall))&&Match(b+0xAF4FA4,kPrologue,sizeof(kPrologue))&&Match(b+kEnterRva,kEnter,sizeof(kEnter))&&Match(b+kExitRva,kExit,sizeof(kExit));Log("Runtime identity gate: current Steam 2.0.4 anchors validated=",ok?"true":"false"," callsiteRva=0xAF42A7 consumerRva=0xAF4FA4 predicateSourceRva=0xAF429C.");if(!ok){Log("Setup refused safely: instruction contract mismatch.");return 0;}try{gIn=safetyhook::create_mid((void*)(b+kAdsInRva),In);gOut=safetyhook::create_mid((void*)(b+kAdsOutRva),Out);gCall=safetyhook::create_mid((void*)(b+kCallRva),Call);gEnter=safetyhook::create_mid((void*)(b+kEnterRva),Enter);gExit=safetyhook::create_mid((void*)(b+kExitRva),Exit);if(!gIn||!gOut||!gCall||!gEnter||!gExit)throw std::runtime_error("hook creation failed");if(!gIn.enable()||!gOut.enable()||!gCall.enable()||!gEnter.enable()||!gExit.enable())throw std::runtime_error("hook enable failed");Log("Read-only runtime gate installed; PRE-only predicate observation; no camera, primitive or render-state writes.");}catch(const std::exception&e){gExit.reset();gEnter.reset();gCall.reset();gOut.reset();gIn.reset();Log("Setup refused safely: ",e.what());}return 0;}
}
BOOL APIENTRY DllMain(HMODULE m,DWORD r,LPVOID){if(r!=DLL_PROCESS_ATTACH)return TRUE;gModule=m;DisableThreadLibraryCalls(m);auto t=CreateThread(nullptr,0,Init,nullptr,0,nullptr);if(t)CloseHandle(t);return TRUE;}
