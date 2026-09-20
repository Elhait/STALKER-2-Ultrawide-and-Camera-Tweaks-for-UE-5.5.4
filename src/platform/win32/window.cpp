#include "window.hpp"

namespace
{
    BOOL CALLBACK FindCurrentProcessWindowCallback(HWND window, LPARAM parameter)
    {
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId == GetCurrentProcessId() && IsWindowVisible(window) &&
            GetWindow(window, GW_OWNER) == nullptr) {
            *reinterpret_cast<HWND*>(parameter) = window;
            return FALSE;
        }
        return TRUE;
    }
}

namespace platform::win32
{
    HWND FindCurrentProcessWindow()
    {
        HWND window = nullptr;
        EnumWindows(FindCurrentProcessWindowCallback, reinterpret_cast<LPARAM>(&window));
        return window;
    }
}
