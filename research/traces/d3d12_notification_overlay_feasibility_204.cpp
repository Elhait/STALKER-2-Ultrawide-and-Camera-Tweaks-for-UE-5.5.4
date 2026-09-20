#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>

#include <safetyhook.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
    using CreateDXGIFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    using CreateDXGIFactory2Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);
    using FactoryQueryInterfaceFn = HRESULT(STDMETHODCALLTYPE*)(IUnknown*, REFIID, void**);
    using CreateSwapChainForHwndFn = HRESULT(STDMETHODCALLTYPE*)(
        IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
    using CreateSwapChainFn = HRESULT(STDMETHODCALLTYPE*)(
        IDXGIFactory*, IUnknown*, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
    using CreateSwapChainForCoreWindowFn = HRESULT(STDMETHODCALLTYPE*)(
        IDXGIFactory2*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*,
        IDXGIOutput*, IDXGISwapChain1**);
    using CreateSwapChainForCompositionFn = HRESULT(STDMETHODCALLTYPE*)(
        IDXGIFactory2*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    std::mutex g_logMutex;
    std::mutex g_factoryMutex;
    std::ofstream g_log;
    std::atomic<bool> g_terminalDisabled{false};
    std::atomic<bool> g_factoryInstalled{false};
    bool g_returnedFactoryVmtReady{};
    bool g_factoryVmtReady{};
    std::atomic<bool> g_swapChainInstalled{false};
    std::atomic<bool> g_firstPresentLogged{false};
    std::atomic<std::uint64_t> g_presentCount{0};

    constexpr std::size_t kFactory2AbiMethodCount = 25;
    struct SafeFactoryClone
    {
        IUnknown* interfacePointer{};
        std::vector<void*> originalTable;
        std::vector<void*> clonedTable;
        HRESULT(STDMETHODCALLTYPE* queryInterface)(IUnknown*, REFIID, void**);
        HRESULT(STDMETHODCALLTYPE* createSwapChain)(IDXGIFactory*, IUnknown*, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
        HRESULT(STDMETHODCALLTYPE* createSwapChainForHwnd)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
        HRESULT(STDMETHODCALLTYPE* createSwapChainForCoreWindow)(IDXGIFactory2*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
        HRESULT(STDMETHODCALLTYPE* createSwapChainForComposition)(IDXGIFactory2*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
    };
    std::vector<std::unique_ptr<SafeFactoryClone>> g_safeFactoryClones;

    safetyhook::InlineHook g_createFactory1Hook;
    safetyhook::InlineHook g_createFactory2Hook;
    safetyhook::InlineHook g_createFactoryHook;
    safetyhook::VmtHook g_returnedFactoryVmt;
    safetyhook::VmHook g_returnedFactoryCreateSwapChainHook;
    safetyhook::VmtHook g_factoryVmt;
    safetyhook::VmHook g_createSwapChainHook;
    safetyhook::VmHook g_createSwapChainForHwndHook;
    safetyhook::VmHook g_createSwapChainForCoreWindowHook;
    safetyhook::VmHook g_createSwapChainForCompositionHook;
    safetyhook::VmtHook g_swapChainVmt;
    safetyhook::VmHook g_presentHook;
    safetyhook::VmHook g_resizeBuffersHook;

    void Log(const std::string& text)
    {
        std::scoped_lock lock(g_logMutex);
        if (!g_log.is_open()) return;
        g_log << text << '\n';
        g_log.flush();
    }

    std::string Hex(std::uintptr_t value)
    {
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "0x%llX", static_cast<unsigned long long>(value));
        return buffer;
    }

    std::string GuidText(REFIID iid)
    {
        wchar_t buffer[64]{};
        if (!StringFromGUID2(iid, buffer, static_cast<int>(std::size(buffer)))) return "<invalid-guid>";
        char narrow[128]{};
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, narrow, static_cast<int>(std::size(narrow)), nullptr, nullptr);
        return narrow;
    }

    std::string FactoryIidClass(REFIID iid)
    {
        if (IsEqualGUID(iid, __uuidof(IDXGIFactory))) return "IDXGIFactory";
        if (IsEqualGUID(iid, __uuidof(IDXGIFactory1))) return "IDXGIFactory1";
        if (IsEqualGUID(iid, __uuidof(IDXGIFactory2))) return "IDXGIFactory2";
        if (IsEqualGUID(iid, __uuidof(IDXGIFactory4))) return "IDXGIFactory4";
        if (IsEqualGUID(iid, __uuidof(IDXGIFactory5))) return "IDXGIFactory5";
        if (IsEqualGUID(iid, __uuidof(IDXGIFactory6))) return "IDXGIFactory6";
        if (IsEqualGUID(iid, __uuidof(IDXGIFactory7))) return "IDXGIFactory7";
        return "unknown";
    }

    void DisableForSession(const char* reason)
    {
        const bool wasDisabled = g_terminalDisabled.exchange(true, std::memory_order_acq_rel);
        if (!wasDisabled)
            Log(std::string("Overlay terminal failure; disabled for this session: ") + reason);
    }

    void LogQueueProvenance(IUnknown* deviceArgument, IDXGISwapChain1* swapChain)
    {
        if (!deviceArgument || !swapChain)
        {
            DisableForSession("missing swapchain or CreateSwapChain device argument");
            return;
        }

        ID3D12CommandQueue* queue = nullptr;
        const HRESULT queueHr = deviceArgument->QueryInterface(IID_PPV_ARGS(&queue));
        if (FAILED(queueHr) || !queue)
        {
            Log("CreateSwapChainForHwnd device argument is not an ID3D12CommandQueue; D3D12 render path not promoted.");
            DisableForSession("D3D12 command queue provenance unavailable");
            return;
        }

        const D3D12_COMMAND_QUEUE_DESC queueDesc = queue->GetDesc();
        Log("D3D12 provenance: swapchain=" + Hex(reinterpret_cast<std::uintptr_t>(swapChain)) +
            " queue=" + Hex(reinterpret_cast<std::uintptr_t>(queue)) +
            " queueType=" + std::to_string(static_cast<unsigned>(queueDesc.Type)) +
            " flags=" + std::to_string(static_cast<unsigned>(queueDesc.Flags)) + ".");

        ID3D12Device* device = nullptr;
        const HRESULT deviceHr = swapChain->GetDevice(IID_PPV_ARGS(&device));
        if (FAILED(deviceHr) || !device)
        {
            queue->Release();
            DisableForSession("active swapchain device unavailable");
            return;
        }

        Log("D3D12 provenance: device=" + Hex(reinterpret_cast<std::uintptr_t>(device)) +
            " swapchain device query passed.");
        device->Release();
        queue->Release();
    }

    HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
    {
        const auto count = g_presentCount.fetch_add(1, std::memory_order_relaxed) + 1;
        if (!g_firstPresentLogged.exchange(true, std::memory_order_acq_rel))
            Log("Present lifecycle reached: swapchain=" + Hex(reinterpret_cast<std::uintptr_t>(swapChain)) +
                " firstPresentCount=" + std::to_string(count) + ". No render resources are attached in Batch 1A bootstrap.");

        return g_presentHook.call<HRESULT>(swapChain, syncInterval, flags);
    }

    HRESULT STDMETHODCALLTYPE HookResizeBuffers(
        IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height,
        DXGI_FORMAT format, UINT swapChainFlags)
    {
        Log("ResizeBuffers observed: swapchain=" + Hex(reinterpret_cast<std::uintptr_t>(swapChain)) +
            " buffers=" + std::to_string(bufferCount) +
            " size=" + std::to_string(width) + "x" + std::to_string(height) + ".");
        const HRESULT result = g_resizeBuffersHook.call<HRESULT>(
            swapChain, bufferCount, width, height, format, swapChainFlags);
        Log(std::string("ResizeBuffers result: ") + (SUCCEEDED(result) ? "success." : "failure; no retry loop started."));
        return result;
    }

    void AttachSwapChain(IUnknown* device, IDXGISwapChain1* swapChain)
    {
        if (!swapChain || g_terminalDisabled.load(std::memory_order_acquire)) return;

        if (!g_swapChainInstalled.exchange(true, std::memory_order_acq_rel))
        {
            IDXGISwapChain* baseSwapChain = nullptr;
            if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&baseSwapChain))) || !baseSwapChain)
            {
                DisableForSession("IDXGISwapChain query failed");
                return;
            }

            auto vmt = safetyhook::VmtHook::create(baseSwapChain);
            if (!vmt)
            {
                baseSwapChain->Release();
                DisableForSession("swapchain VMT allocation failed");
                return;
            }
            g_swapChainVmt = std::move(*vmt);

            auto present = g_swapChainVmt.hook_method(8, HookPresent);
            auto resize = g_swapChainVmt.hook_method(13, HookResizeBuffers);
            if (!present || !resize)
            {
                baseSwapChain->Release();
                DisableForSession("swapchain Present/ResizeBuffers hook setup failed");
                return;
            }

            g_presentHook = std::move(*present);
            g_resizeBuffersHook = std::move(*resize);
            g_swapChainVmt.apply(baseSwapChain);
            Log("Active swapchain attached: Present and ResizeBuffers hooks installed.");
            LogQueueProvenance(device, swapChain);
            baseSwapChain->Release();
        }
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChain(
        IDXGIFactory* factory, IUnknown* device, const DXGI_SWAP_CHAIN_DESC* description,
        IDXGISwapChain** swapChain)
    {
        const HRESULT result = g_createSwapChainHook.call<HRESULT>(factory, device, description, swapChain);
        if (SUCCEEDED(result) && swapChain && *swapChain)
        {
            IDXGISwapChain1* swapChain1 = nullptr;
            if (SUCCEEDED((*swapChain)->QueryInterface(IID_PPV_ARGS(&swapChain1))) && swapChain1)
            {
                AttachSwapChain(device, swapChain1);
                swapChain1->Release();
            }
        }
        return result;
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChainReturned(
        IDXGIFactory* factory, IUnknown* device, const DXGI_SWAP_CHAIN_DESC* description,
        IDXGISwapChain** swapChain)
    {
        const HRESULT result = g_returnedFactoryCreateSwapChainHook.call<HRESULT>(
            factory, device, description, swapChain);
        if (SUCCEEDED(result) && swapChain && *swapChain)
        {
            IDXGISwapChain1* swapChain1 = nullptr;
            if (SUCCEEDED((*swapChain)->QueryInterface(IID_PPV_ARGS(&swapChain1))) && swapChain1)
            {
                AttachSwapChain(device, swapChain1);
                swapChain1->Release();
            }
        }
        return result;
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChainForHwnd(
        IDXGIFactory2* factory, IUnknown* device, HWND window,
        const DXGI_SWAP_CHAIN_DESC1* description,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDescription,
        IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
    {
        const HRESULT result = g_createSwapChainForHwndHook.call<HRESULT>(
            factory, device, window, description, fullscreenDescription,
            restrictToOutput, swapChain);
        if (SUCCEEDED(result) && swapChain && *swapChain)
            AttachSwapChain(device, *swapChain);
        return result;
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChainForCoreWindow(
        IDXGIFactory2* factory, IUnknown* device, IUnknown* window,
        const DXGI_SWAP_CHAIN_DESC1* description, IDXGIOutput* restrictToOutput,
        IDXGISwapChain1** swapChain)
    {
        const HRESULT result = g_createSwapChainForCoreWindowHook.call<HRESULT>(
            factory, device, window, description, restrictToOutput, swapChain);
        if (SUCCEEDED(result) && swapChain && *swapChain)
            AttachSwapChain(device, *swapChain);
        return result;
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChainForComposition(
        IDXGIFactory2* factory, IUnknown* device, const DXGI_SWAP_CHAIN_DESC1* description,
        IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
    {
        const HRESULT result = g_createSwapChainForCompositionHook.call<HRESULT>(
            factory, device, description, restrictToOutput, swapChain);
        if (SUCCEEDED(result) && swapChain && *swapChain)
            AttachSwapChain(device, *swapChain);
        return result;
    }

    SafeFactoryClone* FindSafeFactoryClone(IUnknown* object)
    {
        for (const auto& clone : g_safeFactoryClones)
            if (clone->interfacePointer == object) return clone.get();
        return nullptr;
    }

    bool ReplaceSafeFactoryVtable(IUnknown* object, SafeFactoryClone& clone)
    {
        auto location = reinterpret_cast<void***>(object);
        if (!location || !*location) return false;
        DWORD oldProtection{};
        if (!VirtualProtect(location, sizeof(void*), PAGE_READWRITE, &oldProtection)) return false;
        *location = clone.clonedTable.data();
        DWORD ignored{};
        VirtualProtect(location, sizeof(void*), oldProtection, &ignored);
        return true;
    }

    std::string ClassifyGameWindow(HWND window)
    {
        if (!window) return "NON_GAME";
        DWORD processId{};
        GetWindowThreadProcessId(window, &processId);
        if (processId != GetCurrentProcessId()) return "NON_GAME";
        return IsWindowVisible(window) ? "ACTIVE_GAME" : "AMBIGUOUS";
    }

    IUnknown* CanonicalIdentity(IUnknown* object)
    {
        if (!object) return nullptr;
        IUnknown* identity = nullptr;
        if (FAILED(object->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity)))) return nullptr;
        return identity;
    }

    void LogSafeCreationProvenance(const char* api, IUnknown* deviceArgument, HWND window,
        IDXGISwapChain1* swapChain, const DXGI_SWAP_CHAIN_DESC1* description)
    {
        if (!deviceArgument || !swapChain)
        {
            DisableForSession("missing creation argument or returned swapchain");
            return;
        }
        ID3D12CommandQueue* queue = nullptr;
        const HRESULT queueQuery = deviceArgument->QueryInterface(IID_PPV_ARGS(&queue));
        if (FAILED(queueQuery) || !queue)
        {
            Log(std::string(api) + ": pDevice QI(ID3D12CommandQueue)=FAIL; classification=NON_GAME.");
            DisableForSession("D3D12 command queue provenance unavailable");
            return;
        }
        ID3D12Device* queueDevice = nullptr;
        ID3D12Device* swapChainDevice = nullptr;
        const HRESULT queueDeviceResult = queue->GetDevice(IID_PPV_ARGS(&queueDevice));
        const HRESULT swapChainDeviceResult = swapChain->GetDevice(IID_PPV_ARGS(&swapChainDevice));
        IUnknown* queueIdentity = SUCCEEDED(queueDeviceResult) ? CanonicalIdentity(queueDevice) : nullptr;
        IUnknown* swapChainIdentity = SUCCEEDED(swapChainDeviceResult) ? CanonicalIdentity(swapChainDevice) : nullptr;
        const bool identityMatch = queueIdentity && swapChainIdentity && queueIdentity == swapChainIdentity;
        const auto queueDescription = queue->GetDesc();
        std::string line = std::string(api) + ": pDevice=" + Hex(reinterpret_cast<std::uintptr_t>(deviceArgument)) +
            " queue=" + Hex(reinterpret_cast<std::uintptr_t>(queue)) + " queueQI=" +
            (SUCCEEDED(queueQuery) ? "PASS" : "FAIL") + " queueType=" +
            std::to_string(static_cast<unsigned>(queueDescription.Type)) + " queueFlags=" +
            std::to_string(static_cast<unsigned>(queueDescription.Flags)) + " swapchain=" +
            Hex(reinterpret_cast<std::uintptr_t>(swapChain)) + " hwnd=" +
            Hex(reinterpret_cast<std::uintptr_t>(window)) + " classification=" + ClassifyGameWindow(window);
        if (description)
            line += " size=" + std::to_string(description->Width) + "x" + std::to_string(description->Height) +
                " format=" + std::to_string(static_cast<unsigned>(description->Format)) +
                " buffers=" + std::to_string(description->BufferCount) + " swapEffect=" +
                std::to_string(static_cast<unsigned>(description->SwapEffect)) + " flags=" + std::to_string(description->Flags);
        line += " queueDevice=" + Hex(reinterpret_cast<std::uintptr_t>(queueDevice)) +
            " swapchainDevice=" + Hex(reinterpret_cast<std::uintptr_t>(swapChainDevice)) +
            " comIdentityMatch=" + (identityMatch ? "PASS" : "FAIL") + ".";
        Log(line);
        if (queueIdentity) queueIdentity->Release();
        if (swapChainIdentity) swapChainIdentity->Release();
        if (queueDevice) queueDevice->Release();
        if (swapChainDevice) swapChainDevice->Release();
        queue->Release();
        const auto classification = ClassifyGameWindow(window);
        Log(std::string("Ownership: Swapchain <-> Queue <-> Device = ") + (identityMatch ? "PASS" : "FAIL") +
            "; game presentation provenance = " + classification + ".");
    }

    HRESULT STDMETHODCALLTYPE SafeHookQueryInterface(IUnknown* self, REFIID riid, void** result);
    HRESULT STDMETHODCALLTYPE SafeHookCreateSwapChain(IDXGIFactory* self, IUnknown* device, const DXGI_SWAP_CHAIN_DESC* description, IDXGISwapChain** swapChain);
    HRESULT STDMETHODCALLTYPE SafeHookCreateSwapChainForHwnd(IDXGIFactory2* self, IUnknown* device, HWND window, const DXGI_SWAP_CHAIN_DESC1* description, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen, IDXGIOutput* output, IDXGISwapChain1** swapChain);
    HRESULT STDMETHODCALLTYPE SafeHookCreateSwapChainForCoreWindow(IDXGIFactory2* self, IUnknown* device, IUnknown* window, const DXGI_SWAP_CHAIN_DESC1* description, IDXGIOutput* output, IDXGISwapChain1** swapChain);
    HRESULT STDMETHODCALLTYPE SafeHookCreateSwapChainForComposition(IDXGIFactory2* self, IUnknown* device, const DXGI_SWAP_CHAIN_DESC1* description, IDXGIOutput* output, IDXGISwapChain1** swapChain);

    void InstallSafeFactory2Clone(IUnknown* returnedFactory)
    {
        if (!returnedFactory || g_terminalDisabled.load(std::memory_order_acquire)) return;
        IDXGIFactory2* factory = nullptr;
        if (FAILED(returnedFactory->QueryInterface(IID_PPV_ARGS(&factory))) || !factory)
        {
            Log("IDXGIFactory2 QI failed; swapchain creation path not observed.");
            return;
        }
        Log("Factory interface observation: returned=" + Hex(reinterpret_cast<std::uintptr_t>(returnedFactory)) +
            " queriedIDXGIFactory2=" + Hex(reinterpret_cast<std::uintptr_t>(factory)) +
            " abi=IDXGIFactory2[25].");
        std::scoped_lock lock(g_factoryMutex);
        IUnknown* object = reinterpret_cast<IUnknown*>(factory);
        if (FindSafeFactoryClone(object)) { factory->Release(); return; }
        auto* originalTable = *reinterpret_cast<void***>(object);
        auto clone = std::make_unique<SafeFactoryClone>();
        clone->interfacePointer = object;
        clone->originalTable.assign(originalTable, originalTable + kFactory2AbiMethodCount);
        clone->clonedTable = clone->originalTable;
        clone->queryInterface = reinterpret_cast<decltype(clone->queryInterface)>(originalTable[0]);
        clone->createSwapChain = reinterpret_cast<decltype(clone->createSwapChain)>(originalTable[10]);
        clone->createSwapChainForHwnd = reinterpret_cast<decltype(clone->createSwapChainForHwnd)>(originalTable[15]);
        clone->createSwapChainForCoreWindow = reinterpret_cast<decltype(clone->createSwapChainForCoreWindow)>(originalTable[16]);
        clone->createSwapChainForComposition = reinterpret_cast<decltype(clone->createSwapChainForComposition)>(originalTable[24]);
        clone->clonedTable[0] = reinterpret_cast<void*>(&SafeHookQueryInterface);
        clone->clonedTable[10] = reinterpret_cast<void*>(&SafeHookCreateSwapChain);
        clone->clonedTable[15] = reinterpret_cast<void*>(&SafeHookCreateSwapChainForHwnd);
        clone->clonedTable[16] = reinterpret_cast<void*>(&SafeHookCreateSwapChainForCoreWindow);
        clone->clonedTable[24] = reinterpret_cast<void*>(&SafeHookCreateSwapChainForComposition);
        if (!ReplaceSafeFactoryVtable(object, *clone))
            DisableForSession("IDXGIFactory2 per-instance vtable replacement failed");
        else
            Log("IDXGIFactory2 per-instance COM clone installed; exact 25-entry ABI span.");
        g_safeFactoryClones.push_back(std::move(clone));
        factory->Release();
    }

    HRESULT STDMETHODCALLTYPE SafeHookQueryInterface(IUnknown* self, REFIID riid, void** result)
    {
        FactoryQueryInterfaceFn original{};
        { std::scoped_lock lock(g_factoryMutex); if (auto* c = FindSafeFactoryClone(self)) original = c->queryInterface; }
        if (!original) return E_NOINTERFACE;
        const HRESULT hr = original(self, riid, result);
        if (SUCCEEDED(hr) && result && *result && IsEqualGUID(riid, __uuidof(IDXGIFactory2)))
            InstallSafeFactory2Clone(static_cast<IUnknown*>(*result));
        return hr;
    }

    HRESULT STDMETHODCALLTYPE SafeHookCreateSwapChain(IDXGIFactory* self, IUnknown* device,
        const DXGI_SWAP_CHAIN_DESC* description, IDXGISwapChain** swapChain)
    {
        CreateSwapChainFn original{};
        { std::scoped_lock lock(g_factoryMutex); if (auto* c = FindSafeFactoryClone(reinterpret_cast<IUnknown*>(self))) original = c->createSwapChain; }
        if (!original) return E_FAIL;
        const HRESULT hr = original(self, device, description, swapChain);
        if (SUCCEEDED(hr) && swapChain && *swapChain)
        {
            IDXGISwapChain1* swapChain1 = nullptr;
            if (SUCCEEDED((*swapChain)->QueryInterface(IID_PPV_ARGS(&swapChain1))) && swapChain1)
            {
                DXGI_SWAP_CHAIN_DESC1 converted{};
                if (description)
                {
                    converted.Width = description->BufferDesc.Width;
                    converted.Height = description->BufferDesc.Height;
                    converted.Format = description->BufferDesc.Format;
                    converted.BufferCount = description->BufferCount;
                    converted.SwapEffect = description->SwapEffect;
                    converted.Flags = description->Flags;
                }
                LogSafeCreationProvenance("CreateSwapChain", device, nullptr, swapChain1, description ? &converted : nullptr);
                swapChain1->Release();
            }
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE SafeHookCreateSwapChainForHwnd(IDXGIFactory2* self, IUnknown* device, HWND window,
        const DXGI_SWAP_CHAIN_DESC1* description, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
        IDXGIOutput* output, IDXGISwapChain1** swapChain)
    {
        CreateSwapChainForHwndFn original{};
        { std::scoped_lock lock(g_factoryMutex); if (auto* c = FindSafeFactoryClone(reinterpret_cast<IUnknown*>(self))) original = c->createSwapChainForHwnd; }
        if (!original) return E_FAIL;
        const HRESULT hr = original(self, device, window, description, fullscreen, output, swapChain);
        if (SUCCEEDED(hr) && swapChain && *swapChain) LogSafeCreationProvenance("CreateSwapChainForHwnd", device, window, *swapChain, description);
        return hr;
    }

    HRESULT STDMETHODCALLTYPE SafeHookCreateSwapChainForCoreWindow(IDXGIFactory2* self, IUnknown* device, IUnknown* window,
        const DXGI_SWAP_CHAIN_DESC1* description, IDXGIOutput* output, IDXGISwapChain1** swapChain)
    {
        CreateSwapChainForCoreWindowFn original{};
        { std::scoped_lock lock(g_factoryMutex); if (auto* c = FindSafeFactoryClone(reinterpret_cast<IUnknown*>(self))) original = c->createSwapChainForCoreWindow; }
        if (!original) return E_FAIL;
        const HRESULT hr = original(self, device, window, description, output, swapChain);
        if (SUCCEEDED(hr) && swapChain && *swapChain) LogSafeCreationProvenance("CreateSwapChainForCoreWindow", device, nullptr, *swapChain, description);
        return hr;
    }

    HRESULT STDMETHODCALLTYPE SafeHookCreateSwapChainForComposition(IDXGIFactory2* self, IUnknown* device,
        const DXGI_SWAP_CHAIN_DESC1* description, IDXGIOutput* output, IDXGISwapChain1** swapChain)
    {
        CreateSwapChainForCompositionFn original{};
        { std::scoped_lock lock(g_factoryMutex); if (auto* c = FindSafeFactoryClone(reinterpret_cast<IUnknown*>(self))) original = c->createSwapChainForComposition; }
        if (!original) return E_FAIL;
        const HRESULT hr = original(self, device, description, output, swapChain);
        if (SUCCEEDED(hr) && swapChain && *swapChain) LogSafeCreationProvenance("CreateSwapChainForComposition", device, nullptr, *swapChain, description);
        return hr;
    }

    void InstallFactoryHooks(IUnknown* returnedFactory)
    {
        // DXGI objects expose COM vtables, not a guaranteed MSVC RTTI-backed
        // C++ VMT. SafetyHook::VmtHook assumes the latter and can shift the
        // COM method table, so this path is intentionally disabled until a
        // COM-safe interception strategy is implemented.
        InstallSafeFactory2Clone(returnedFactory);
        return;

#if 0
        if (!returnedFactory || g_terminalDisabled.load(std::memory_order_acquire)) return;
        std::scoped_lock lock(g_factoryMutex);

        IDXGIFactory2* factory2 = nullptr;
        if (FAILED(returnedFactory->QueryInterface(IID_PPV_ARGS(&factory2))) || !factory2)
        {
            DisableForSession("IDXGIFactory2 unavailable");
            return;
        }

        if (!g_returnedFactoryVmtReady)
        {
            auto returnedVmt = safetyhook::VmtHook::create(returnedFactory);
            if (!returnedVmt)
            {
                factory2->Release();
                DisableForSession("returned factory VMT allocation failed");
                return;
            }
            g_returnedFactoryVmt = std::move(*returnedVmt);
            auto returnedLegacyHook = g_returnedFactoryVmt.hook_method(10, HookCreateSwapChainReturned);
            if (!returnedLegacyHook)
            {
                factory2->Release();
                DisableForSession("returned factory CreateSwapChain hook setup failed");
                return;
            }
            g_returnedFactoryCreateSwapChainHook = std::move(*returnedLegacyHook);
            g_returnedFactoryVmtReady = true;
        }
        g_returnedFactoryVmt.apply(returnedFactory);

        if (!g_factoryVmtReady)
        {
            auto vmt = safetyhook::VmtHook::create(factory2);
            if (!vmt)
            {
                factory2->Release();
                DisableForSession("factory VMT allocation failed");
                return;
            }
            g_factoryVmt = std::move(*vmt);
            // IDXGIFactory includes MakeWindowAssociation and
            // GetWindowAssociation before CreateSwapChain. IDXGIFactory2
            // similarly places IsWindowedStereoEnabled before the Hwnd path.
            auto legacyHook = g_factoryVmt.hook_method(10, HookCreateSwapChain);
            auto hwndHook = g_factoryVmt.hook_method(15, HookCreateSwapChainForHwnd);
            auto coreWindowHook = g_factoryVmt.hook_method(16, HookCreateSwapChainForCoreWindow);
            auto compositionHook = g_factoryVmt.hook_method(24, HookCreateSwapChainForComposition);
            if (!legacyHook || !hwndHook || !coreWindowHook || !compositionHook)
            {
                factory2->Release();
                DisableForSession("DXGI swapchain creation hook setup failed");
                return;
            }
            g_createSwapChainHook = std::move(*legacyHook);
            g_createSwapChainForHwndHook = std::move(*hwndHook);
            g_createSwapChainForCoreWindowHook = std::move(*coreWindowHook);
            g_createSwapChainForCompositionHook = std::move(*compositionHook);
            g_factoryVmtReady = true;
        }
        g_factoryVmt.apply(factory2);
        g_factoryInstalled.store(true, std::memory_order_release);
        Log("DXGI factory interface attached: returned and queried interfaces covered; swapchain creation hooks active.");
        factory2->Release();
#endif
    }

    HRESULT WINAPI HookCreateDXGIFactory1(REFIID riid, void** factory)
    {
        const HRESULT result = g_createFactory1Hook.original<CreateDXGIFactoryFn>()(riid, factory);
        Log("CreateDXGIFactory1: requestedIID=" + GuidText(riid) + " (" + FactoryIidClass(riid) + ") returned=" +
            Hex(reinterpret_cast<std::uintptr_t>(factory ? *factory : nullptr)) + ".");
        if (SUCCEEDED(result) && factory && *factory)
            InstallFactoryHooks(static_cast<IUnknown*>(*factory));
        return result;
    }

    HRESULT WINAPI HookCreateDXGIFactory(REFIID riid, void** factory)
    {
        const HRESULT result = g_createFactoryHook.original<CreateDXGIFactoryFn>()(riid, factory);
        Log("CreateDXGIFactory: requestedIID=" + GuidText(riid) + " (" + FactoryIidClass(riid) + ") returned=" +
            Hex(reinterpret_cast<std::uintptr_t>(factory ? *factory : nullptr)) + ".");
        if (SUCCEEDED(result) && factory && *factory)
            InstallFactoryHooks(static_cast<IUnknown*>(*factory));
        return result;
    }

    HRESULT WINAPI HookCreateDXGIFactory2(UINT flags, REFIID riid, void** factory)
    {
        const HRESULT result = g_createFactory2Hook.original<CreateDXGIFactory2Fn>()(flags, riid, factory);
        Log("CreateDXGIFactory2: flags=" + std::to_string(flags) + " requestedIID=" + GuidText(riid) +
            " (" + FactoryIidClass(riid) + ") returned=" +
            Hex(reinterpret_cast<std::uintptr_t>(factory ? *factory : nullptr)) + ".");
        if (SUCCEEDED(result) && factory && *factory)
            InstallFactoryHooks(static_cast<IUnknown*>(*factory));
        return result;
    }

    DWORD WINAPI Initialize(void*)
    {
        char modulePath[MAX_PATH]{};
        GetModuleFileNameA(nullptr, modulePath, sizeof(modulePath));
        const auto logPath = std::filesystem::path(modulePath).parent_path() /
            "STALKER2NotificationOverlayFeasibility.log";
        g_log.open(logPath, std::ios::out | std::ios::trunc);
        Log("D3D12 notification overlay feasibility bootstrap started.");
        Log("Scope: factory/swapchain/device/command-queue provenance only; no camera/FOV hooks and no render resources.");

        HMODULE dxgi = nullptr;
        for (int attempt = 0; attempt < 120 && !dxgi; ++attempt)
        {
            dxgi = GetModuleHandleW(L"dxgi.dll");
            if (!dxgi) std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        if (!dxgi)
        {
            DisableForSession("dxgi.dll was not loaded during bounded bootstrap window");
            return 0;
        }

        auto createFactory1 = reinterpret_cast<CreateDXGIFactoryFn>(GetProcAddress(dxgi, "CreateDXGIFactory1"));
        auto createFactory2 = reinterpret_cast<CreateDXGIFactoryFn>(GetProcAddress(dxgi, "CreateDXGIFactory2"));
        auto createFactory = reinterpret_cast<CreateDXGIFactoryFn>(GetProcAddress(dxgi, "CreateDXGIFactory"));
        if (!createFactory1 || !createFactory2 || !createFactory)
        {
            DisableForSession("required DXGI factory exports unavailable");
            return 0;
        }

        g_createFactory1Hook = safetyhook::create_inline(createFactory1, HookCreateDXGIFactory1);
        g_createFactory2Hook = safetyhook::create_inline(createFactory2, HookCreateDXGIFactory2);
        g_createFactoryHook = safetyhook::create_inline(createFactory, HookCreateDXGIFactory);
        if (!g_createFactory1Hook || !g_createFactory2Hook || !g_createFactoryHook)
        {
            DisableForSession("DXGI factory inline hook setup failed");
            return 0;
        }
        Log("DXGI factory exports hooked; waiting for the active game swapchain.");
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
