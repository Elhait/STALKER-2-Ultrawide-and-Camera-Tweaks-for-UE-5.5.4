#include "../../src/gameplay/horplus_gameplay.hpp"

#include <cmath>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* name)
    {
        if (!condition) std::cerr << name << ": FAIL\n";
        return condition;
    }
}

int main()
{
    constexpr float nativeAspect = 16.0f / 9.0f;
    constexpr float runtimeAspect = 32.0f / 9.0f;
    constexpr std::uint8_t flags = 0x4;
    constexpr float nativeSamples[] = { 90.0f, 80.0f, 65.0f, 50.0f, 40.0f, 34.5502f };

    bool result = true;
    float previousOutput = 0.0f;
    for (const float nativeFov : nativeSamples) {
        float output = nativeFov;
        const bool applied = gameplay::TryTransformHorPlus(
            nativeFov, runtimeAspect, flags, nativeAspect, output);

        float expected = nativeFov;
        const bool expectedApplied = gameplay::TryTransformHorPlus(
            nativeFov, runtimeAspect, flags, nativeAspect, expected);

        result &= Check(applied && expectedApplied, "zoom_sample_transformed");
        result &= Check(std::isfinite(output), "zoom_output_finite");
        result &= Check(std::fabs(output - expected) <= 0.001f,
            "repeated_sample_output_is_stable");
        result &= Check(std::fabs(output - nativeFov) > 0.001f,
            "ultrawide_output_is_corrected");

        // The next native sample is deliberately read from nativeSamples,
        // never from the previous transformed output. This guards the
        // single-transform/no-feedback contract.
        if (previousOutput != 0.0f)
            result &= Check(std::fabs(nativeFov - previousOutput) > 0.001f,
                "native_sequence_is_not_feedback");
        previousOutput = output;
    }

    std::cout << "Zoom transition HorPlus harness: "
        << (result ? "PASS" : "FAIL") << "\n";
    return result ? 0 : 1;
}
