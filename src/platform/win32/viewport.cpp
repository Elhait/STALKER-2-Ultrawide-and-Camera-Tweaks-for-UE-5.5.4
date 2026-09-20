#include "viewport.hpp"

#include "window.hpp"

#include <limits>
#include <windows.h>

namespace platform::win32
{
    bool IsUsableClientViewport(long width, long height) noexcept
    {
        // During minimize/resize Windows can briefly report a one-pixel or
        // otherwise non-renderable client rectangle. Keep arbitrary ratios
        // valid; reject only dimensions that cannot represent a viewport.
        return width >= 16 && height >= 16;
    }

    float ReadClientViewportAspect()
    {
        HWND window = GetForegroundWindow();
        DWORD processId = 0;
        if (!window || (GetWindowThreadProcessId(window, &processId),
            processId != GetCurrentProcessId())) {
            window = FindCurrentProcessWindow();
        }

        RECT client{};
        if (window && GetClientRect(window, &client)) {
            const auto width = client.right - client.left;
            const auto height = client.bottom - client.top;
            if (IsUsableClientViewport(width, height))
                return static_cast<float>(width) / static_cast<float>(height);
        }

        DEVMODE display{ .dmSize = sizeof(DEVMODE) };
        if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &display) && display.dmPelsHeight != 0)
            return static_cast<float>(display.dmPelsWidth) / static_cast<float>(display.dmPelsHeight);

        return std::numeric_limits<float>::quiet_NaN();
    }
}
