#include "../../src/diagnostics/diagnostic_runtime.hpp"

#include <iostream>

int main()
{
    diagnostics::SetEnabled(false);
    const bool disabled = !diagnostics::Enabled();
    diagnostics::SetEnabled(true);
    const bool enabled = diagnostics::Enabled();
    diagnostics::SetEnabled(false);
    const bool reset = !diagnostics::Enabled();
    const bool pass = disabled && enabled && reset;
    std::cout << "diagnostics_gate=" << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
