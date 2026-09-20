#include "horplus.hpp"

#include <cmath>

namespace camera
{
    float HorPlus(float fov, float aspect, float nativeAspect)
    {
        constexpr float pi = 3.14159265358979323846f;
        const float half = fov * (pi / 360.0f);
        return 2.0f * std::atan(std::tan(half) * (aspect / nativeAspect)) * (180.0f / pi);
    }
}
