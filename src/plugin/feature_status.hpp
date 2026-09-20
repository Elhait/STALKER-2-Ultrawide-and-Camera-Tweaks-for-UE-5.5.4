#pragma once

namespace plugin
{
    enum class FeatureStatus
    {
        Disabled,
        Available,
        Failed
    };

    const char* FeatureStatusName(FeatureStatus status);

    bool DialogueLifecycleCapabilityAvailable(bool nonNativeDialogue,
        bool cinematicLifecycleObservation, bool gameplayRecoveryObservation);
}
