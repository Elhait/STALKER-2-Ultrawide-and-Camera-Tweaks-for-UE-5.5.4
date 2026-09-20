#include "../../src/dialogue/dialogue_state.hpp"

#include <cmath>
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
    constexpr float nativeTarget = 90.0f;
    constexpr float targetEpsilon = 0.01f;
    constexpr float stabilityEpsilon = 0.01f;
    constexpr std::uintptr_t source = 0x1000;
    bool pass = true;

    dialogue::RecoveryRearm recovery;
    recovery.Begin(source, 89.4f, nativeTarget);
    pass &= Check(recovery.IsPending(), "recovery_tail_enters_pending");
    pass &= Check(recovery.Observe(source, 89.7f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Pending,
        "moving_tail_does_not_rearm");
    pass &= Check(recovery.IsPending(), "moving_tail_remains_pending");
    pass &= Check(recovery.Observe(source, 89.995f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Complete,
        "first_target_sample_rearms");
    pass &= Check(!recovery.IsPending(), "rearm_clears_pending_state");

    recovery.Begin(source, 89.4f, nativeTarget);
    pass &= Check(recovery.Observe(source, 89.974f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Pending,
        "tail_leaving_tolerance_does_not_rearm");
    pass &= Check(recovery.IsPending(), "tail_leaving_tolerance_remains_pending");
    pass &= Check(recovery.Observe(source, 89.985f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Pending,
        "returning_tail_requires_stability");
    pass &= Check(recovery.Observe(source, 89.999f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Complete,
        "returning_tail_first_target_sample");

    recovery.Begin(source, 89.9734f, nativeTarget);
    pass &= Check(recovery.Observe(source, 89.9736f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Pending,
        "next_dialogue_not_rearmed_immediately");
    pass &= Check(recovery.Observe(source, 89.9949f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Complete,
        "target_sample_without_followup_rearms");
    pass &= Check(!recovery.IsPending(), "target_sample_without_followup_clears_pending");
    recovery.Begin(source, 89.9734f, nativeTarget);
    pass &= Check(recovery.Observe(source, 89.9736f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Pending,
        "baseline_proximity_does_not_rearm");
    pass &= Check(recovery.Observe(source, 89.9949f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Complete,
        "native_target_completion_rearms_after_continuation");

    recovery.Begin(source, 89.4f, std::numeric_limits<float>::quiet_NaN());
    pass &= Check(recovery.Observe(source, 90.0f, std::numeric_limits<float>::quiet_NaN(),
        targetEpsilon, stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Pending,
        "invalid_target_fails_closed");
    pass &= Check(recovery.IsPending(), "invalid_target_keeps_pending");

    pass &= Check(recovery.Observe(0x2000, 90.0f, nativeTarget, targetEpsilon,
        stabilityEpsilon) == dialogue::RecoveryRearm::Decision::Cancel,
        "source_change_cancels_recovery");
    pass &= Check(!recovery.IsPending(), "source_change_clears_pending");

    std::cout << "dialogue_recovery_rearm=" << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
