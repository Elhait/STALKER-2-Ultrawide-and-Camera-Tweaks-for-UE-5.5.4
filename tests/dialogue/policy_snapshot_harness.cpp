#include "../../src/dialogue/dialogue_state.hpp"

#include <iostream>

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
    using config::DialogueZoomPolicy;
    bool pass = true;
    dialogue::PolicySnapshot active;

    pass &= Check(!active.IsValid(), "snapshot_starts_invalid");
    active.Capture(DialogueZoomPolicy::Reduced);
    pass &= Check(active.IsValid(), "reduced_snapshot_valid");
    pass &= Check(active.Value() == DialogueZoomPolicy::Reduced,
        "reduced_snapshot_value");

    DialogueZoomPolicy selected = DialogueZoomPolicy::Adaptive;
    pass &= Check(active.Value() == DialogueZoomPolicy::Reduced,
        "selected_change_does_not_mutate_active");
    active.Clear();
    pass &= Check(!active.IsValid(), "completed_lifecycle_releases_active");

    active.Capture(selected);
    pass &= Check(active.Value() == DialogueZoomPolicy::Adaptive,
        "next_lifecycle_snapshots_latest_selected");

    active.Clear();
    active.Capture(DialogueZoomPolicy::Native);
    pass &= Check(active.Value() == DialogueZoomPolicy::Native,
        "native_policy_snapshots");
    selected = DialogueZoomPolicy::Disabled;
    pass &= Check(active.Value() == DialogueZoomPolicy::Native,
        "native_active_policy_is_immutable");

    active.Clear();
    active.Capture(DialogueZoomPolicy::Disabled);
    pass &= Check(active.Value() == DialogueZoomPolicy::Disabled,
        "disabled_policy_snapshots");

    std::cout << "dialogue_policy_snapshot=" << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
