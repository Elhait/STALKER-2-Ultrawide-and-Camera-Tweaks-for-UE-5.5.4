#include "../../src/diagnostics/matchgameplay_prediction.hpp"

#include <cmath>
#include <iostream>

namespace
{
    bool Near(float left, float right)
    {
        return std::fabs(left - right) < 0.01f;
    }
}

int main()
{
    using diagnostics::matchgameplay::Evaluate;
    using diagnostics::matchgameplay::PredictionInput;
    using diagnostics::matchgameplay::SampleSpace;

    PredictionInput native{};
    native.baselinePairValid = true;
    native.gameplayNative = 112.623f;
    native.gameplayHorPlus = 143.132f;
    native.gameplayAspect = 3.55556f;
    native.enterObservation = 90.0f;
    native.sampleNative = 123.423f;
    native.cinematicAspect = 3.55556f;
    const auto nativeResult = Evaluate(native, 1.77778f);

    PredictionInput cached = native;
    cached.numericGuardMatched = true;
    const auto cachedResult = Evaluate(cached, 1.77778f);

    const bool nativePass = nativeResult.candidateAvailable &&
        nativeResult.sampleSpace == SampleSpace::Native &&
        Near(nativeResult.nativeMatched, 140.527f) &&
        Near(nativeResult.horPlusMatched, 159.66f);
    const bool cachedPass = !cachedResult.candidateAvailable &&
        cachedResult.sampleSpace == SampleSpace::CachedTransformedEnter &&
        !std::isfinite(cachedResult.nativeMatched) &&
        !std::isfinite(cachedResult.horPlusMatched);
    const bool statePass = native.gameplayNative == cached.gameplayNative &&
        native.gameplayHorPlus == cached.gameplayHorPlus &&
        native.enterObservation == cached.enterObservation;
    const bool pass = nativePass && cachedPass && statePass;
    std::cout << "matchgameplay_native_prediction=" << (nativePass ? "PASS" : "FAIL") << "\n";
    std::cout << "matchgameplay_cached_enter_exclusion=" << (cachedPass ? "PASS" : "FAIL") << "\n";
    std::cout << "matchgameplay_context_preservation=" << (statePass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
