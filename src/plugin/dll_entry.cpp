#include "runtime.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_DETACH) {
        plugin::NotifyProcessDetach(reserved != nullptr);
        return TRUE;
    }
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    plugin::SetModuleHandle(module);
    DisableThreadLibraryCalls(module);
    const auto thread = CreateThread(nullptr, 0, plugin::InitializeThread, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
    return TRUE;
}
