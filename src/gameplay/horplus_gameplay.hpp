#pragma once

#include <cstdint>

namespace gameplay
{
    struct HorPlusTransformResult
    {
        float inputFov{};
        float outputFov{};
        bool eligible{};
        bool applied{};
    };

    HorPlusTransformResult EvaluateHorPlus(float inputFov, float aspect,
        std::uint8_t flags, float nativeAspect);

    bool TryTransformHorPlus(float inputFov, float aspect, std::uint8_t flags,
        float nativeAspect, float& transformedFov);
}
