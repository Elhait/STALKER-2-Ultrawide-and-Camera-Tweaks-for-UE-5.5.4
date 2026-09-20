#include "feature_status.hpp"

namespace plugin
{
    const char* FeatureStatusName(FeatureStatus status)
    {
        switch (status) {
        case FeatureStatus::Disabled: return "DISABLED";
        case FeatureStatus::Available: return "AVAILABLE";
        case FeatureStatus::Failed: return "FAILED";
        }
        return "FAILED";
    }

    bool DialogueLifecycleCapabilityAvailable(bool nonNativeDialogue,
        bool cinematicLifecycleObservation, bool gameplayRecoveryObservation)
    {
        return !nonNativeDialogue ||
            (cinematicLifecycleObservation && gameplayRecoveryObservation);
    }
}
