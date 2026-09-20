#pragma once

#include <safetyhook.hpp>

namespace hooks
{
    // Batch 2 owner boundary only. Installation order, partial-failure paths
    // and destruction semantics remain owned by the existing coordinator.
    struct HookSet
    {
        SafetyHookMid gameplay;
        SafetyHookMid cinematicEnter;
        SafetyHookMid cinematicExit;
        SafetyHookMid cinematicAspectStore;
        SafetyHookMid dialogueBoundary;
#ifdef POST_EXIT_GAMEPLAY_OBSERVER_DIAGNOSTIC
        SafetyHookMid postExitGameplayObserver;
#endif
#ifdef POST_EXIT_FOV_STATE_CONSUMER_TRACE
        SafetyHookMid postExitConsumer;
#endif
#ifdef POST_EXIT_CAMERA_FOV_WRITE_OWNER_TRACE
        SafetyHookMid postExitWriteOwner;
#endif
#ifdef POST_EXIT_FOV_PRODUCER_STORE_TRACE
        SafetyHookMid postExitProducerStore;
#endif
    };
}
