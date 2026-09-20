#include "../../src/platform/win32/viewport.hpp"

#include <iostream>

int main()
{
    bool pass = true;
    pass &= platform::win32::IsUsableClientViewport(1920, 1080);
    pass &= platform::win32::IsUsableClientViewport(4000, 1000);
    pass &= !platform::win32::IsUsableClientViewport(1, 1080);
    pass &= !platform::win32::IsUsableClientViewport(1920, 1);
    pass &= !platform::win32::IsUsableClientViewport(0, 0);
    std::cout << "viewport_plausibility=" << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
