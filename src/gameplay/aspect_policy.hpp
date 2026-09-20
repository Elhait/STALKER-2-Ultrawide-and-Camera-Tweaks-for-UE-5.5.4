#pragma once

#include <cstdint>

namespace gameplay
{
    bool IsUltrawideAspect(float aspect, float nativeAspect);
    bool IsConstrainedUltrawideAspect(float aspect, std::uint8_t flags, float nativeAspect);
}
