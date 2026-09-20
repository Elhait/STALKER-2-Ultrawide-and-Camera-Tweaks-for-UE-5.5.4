#include "../../src/dialogue/dialogue_fov.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
    bool Near(float actual, float expected, float tolerance = 0.02f)
    {
        return std::isfinite(actual) && std::fabs(actual - expected) <= tolerance;
    }

    bool Check(bool condition, const char* name)
    {
        if (!condition) std::fprintf(stderr, "%s: FAIL\n", name);
        return condition;
    }
}

int main()
{
    bool pass = true;
    // Independently recorded/validated dialogue endpoints for the native 90°
    // gameplay reference and the 110° gameplay reference.
    pass &= Check(Near(dialogue::AdaptiveTarget(90.0f), 70.0f), "adaptive_90_endpoint");
    pass &= Check(Near(dialogue::ReducedTarget(90.0f), 79.84411f), "reduced_90_endpoint");
    pass &= Check(Near(dialogue::AdaptiveTarget(110.0f), 90.0f), "adaptive_110_endpoint");
    pass &= Check(Near(dialogue::ReducedTarget(110.0f), 100.15589f), "reduced_110_endpoint");

    const float baseline = 110.0f;
    const float target = dialogue::ReducedTarget(baseline);
    const float enter = dialogue::TransformProjectionSample(baseline, baseline, target);
    const float exit = dialogue::TransformExitSample(target, baseline, target, target);
    pass &= Check(Near(enter, baseline) && Near(
        dialogue::TransformProjectionSample(70.0f, baseline, target), target),
        "projection_endpoints");
    if (!Near(exit, target)) std::fprintf(stderr, "exit=%f target=%f\n", exit, target);
    pass &= Check(Near(exit, target), "exit_start_endpoint");
    pass &= Check(Near(dialogue::TransformExitSample(baseline, baseline, baseline, target), baseline),
        "exit_baseline_endpoint");

    const float p1 = dialogue::TransformProjectionSample(100.0f, baseline, target);
    const float p2 = dialogue::TransformProjectionSample(90.0f, baseline, target);
    pass &= Check(p1 > p2 && p2 > target, "projection_monotonicity");
    pass &= Check(dialogue::TransformProjectionSample(40.0f, baseline, target) == 40.0f,
        "out_of_range_passthrough");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    pass &= Check(std::isnan(dialogue::TransformProjectionSample(nan, baseline, target)),
        "nan_passthrough");
    const float infinityExit = dialogue::TransformExitSample(inf, baseline, target, target);
    if (!std::isinf(infinityExit)) std::fprintf(stderr,
        "infinity_exit=%f\n", infinityExit);
    pass &= Check(std::isinf(infinityExit), "infinity_passthrough");

    std::printf("dialogue_fov=%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
