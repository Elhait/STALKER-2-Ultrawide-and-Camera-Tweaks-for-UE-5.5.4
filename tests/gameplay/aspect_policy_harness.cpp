#include "../../../src/gameplay/aspect_policy.hpp"

#include <iostream>
#include <limits>

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
    constexpr float native = 16.0f / 9.0f;
    const float threshold = native + 0.001f;
    const float named[] = { 2.37037f, 2.38889f, 2.4f, 3.2f, 3.55556f };
    bool result = true;

    result &= Check(!gameplay::IsUltrawideAspect(native - 0.0005f, native), "below_native");
    result &= Check(!gameplay::IsUltrawideAspect(native, native), "native");
    result &= Check(!gameplay::IsUltrawideAspect(native + 0.0005f, native), "below_threshold");
    result &= Check(!gameplay::IsUltrawideAspect(threshold, native), "threshold_equal");
    result &= Check(gameplay::IsUltrawideAspect(native + 0.0015f, native), "above_threshold");

    for (int tick = 100; tick <= 177; ++tick)
        result &= Check(!gameplay::IsUltrawideAspect(static_cast<float>(tick) / 100.0f, native), "narrow_sweep");
    for (int tick = 178; tick <= 400; ++tick)
        result &= Check(gameplay::IsUltrawideAspect(static_cast<float>(tick) / 100.0f, native), "ultrawide_sweep");

    for (const float aspect : named) {
        result &= Check(gameplay::IsUltrawideAspect(aspect, native), "named_ultrawide");
        result &= Check(gameplay::IsConstrainedUltrawideAspect(aspect, 0x5, native), "constrained_0x5");
        result &= Check(!gameplay::IsConstrainedUltrawideAspect(aspect, 0x4, native), "constrained_0x4");
    }

    result &= Check(!gameplay::IsUltrawideAspect(std::numeric_limits<float>::quiet_NaN(), native), "nan");
    result &= Check(!gameplay::IsUltrawideAspect(std::numeric_limits<float>::infinity(), native), "infinity");
    result &= Check(!gameplay::IsConstrainedUltrawideAspect(3.2f, 0x6, native), "invalid_flags");

    std::cout << "Aspect policy harness: " << (result ? "PASS" : "FAIL") << "\n";
    return result ? 0 : 1;
}
