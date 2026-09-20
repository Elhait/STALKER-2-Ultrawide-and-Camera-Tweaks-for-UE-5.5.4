#pragma once

#include "fov_observation.hpp"

#include <cstdint>
#include <limits>
#include <mutex>

namespace camera
{
    struct GameplayAspectRestorationState
    {
        float aspect{std::numeric_limits<float>::quiet_NaN()};
        FovWriterSourceToken source{};
        std::uint64_t observationSequence{};
        bool valid{};
    };

    enum class GameplayAspectRestorationDisposition : std::uint8_t
    {
        Updated,
        Retained,
        Invalidated,
    };

    struct GameplayAspectRestorationResult
    {
        GameplayAspectRestorationDisposition disposition{
            GameplayAspectRestorationDisposition::Retained};
        GameplayAspectRestorationState state{};
        bool changed{};
    };

    const char* GameplayAspectRestorationDispositionName(
        GameplayAspectRestorationDisposition value) noexcept;

    enum class GameplayAspectRestorationDecision : std::uint8_t
    {
        NoAction,
        Defer,
        ConsumeNoWrite,
        Restore,
    };

    struct GameplayAspectRestorationDecisionInput
    {
        bool pending{};
        bool targetIsHorPlus{};
        bool gameplayEnabled{};
        bool coordinatorIsGameplay{};
        bool currentAspectReadable{};
        bool currentAspectValid{};
        bool currentAspectUltrawide{};
        bool restorationValid{};
        bool restorationAspectValid{};
        bool restorationAspectUltrawide{};
    };

    struct GameplayAspectRestorationDecisionResult
    {
        GameplayAspectRestorationDecision decision{
            GameplayAspectRestorationDecision::NoAction};
        float restorationAspect{std::numeric_limits<float>::quiet_NaN()};
    };

    const char* GameplayAspectRestorationDecisionName(
        GameplayAspectRestorationDecision value) noexcept;
    GameplayAspectRestorationDecisionResult ResolveGameplayAspectRestorationDecision(
        const GameplayAspectRestorationDecisionInput& input,
        float restorationAspect) noexcept;

    class GameplayAspectRestorationStore
    {
    public:
        GameplayAspectRestorationResult Update(
            float aspect, FovWriterSourceToken source);
        GameplayAspectRestorationResult Retain();
        GameplayAspectRestorationResult Invalidate();
        GameplayAspectRestorationState Read() const;

    private:
        mutable std::mutex mutex_;
        std::uint64_t nextSequence_{};
        GameplayAspectRestorationState state_{};
    };
}
