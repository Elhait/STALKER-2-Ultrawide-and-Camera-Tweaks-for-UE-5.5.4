#include "horplus_gameplay.hpp"
#include "aspect_policy.hpp"

#include "../camera/horplus.hpp"

#include <cmath>

namespace gameplay
{
    HorPlusTransformResult EvaluateHorPlus(float inputFov, float aspect,
        std::uint8_t flags, float nativeAspect)
    {
        HorPlusTransformResult result{ inputFov, inputFov, false, false };
        if (!std::isfinite(inputFov) || inputFov <= 1.0f || inputFov >= 179.0f ||
            !IsUltrawideAspect(aspect, nativeAspect) ||
            (flags != 0x4 && flags != 0x5)) return result;

        result.eligible = true;
        result.outputFov = camera::HorPlus(inputFov, aspect, nativeAspect);
        result.applied = std::isfinite(result.outputFov) &&
            result.outputFov > 1.0f && result.outputFov < 179.0f;
        if (!result.applied) result.outputFov = result.inputFov;
        return result;
    }

    bool TryTransformHorPlus(float inputFov, float aspect, std::uint8_t flags,
        float nativeAspect, float& transformedFov)
    {
        const auto result = EvaluateHorPlus(inputFov, aspect, flags, nativeAspect);
        transformedFov = result.outputFov;
        return result.applied;
    }
}
