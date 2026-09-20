#pragma once

namespace dialogue
{
    float TransformProjectionSample(float incoming, float baseline, float target);
    float TransformExitSample(float incoming, float baseline, float target, float exitStart);
    float AdaptiveTarget(float baseline);
    float ReducedTarget(float baseline);
}
