#include "aspect_policy.hpp"

#include <cmath>

namespace gameplay
{
    bool IsUltrawideAspect(float aspect, float nativeAspect)
    {
        return std::isfinite(aspect) && aspect > nativeAspect + 0.001f;
    }

    bool IsConstrainedUltrawideAspect(float aspect, std::uint8_t flags, float nativeAspect)
    {
        return IsUltrawideAspect(aspect, nativeAspect) && flags == 0x5;
    }
}
