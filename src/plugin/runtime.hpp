#pragma once

#include <Windows.h>

#include "worker_lifecycle.hpp"
#include "feature_status.hpp"

namespace plugin
{
    void SetModuleHandle(HMODULE module);
    DWORD WINAPI InitializeThread(void* parameter);
    void Shutdown();
    void NotifyProcessDetach(bool processTerminating);
}
