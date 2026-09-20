#pragma once

#include "../provenance.hpp"

#include <cstdint>

namespace runtime_evidence::dialogue
{
    inline constexpr Provenance kProvenance{
        "dialogue-candidate-context", "DIALOGUE_CANDIDATE_CONTEXT_REPAIR.md",
        "UNKNOWN", "UNKNOWN", "UNKNOWN"};

    struct CandidateContextCase
    {
        std::uintptr_t source;
        float nativeTarget;
        float initialSample;
        float descendingSample;
        float contradictoryTarget;
        std::uintptr_t changedSource;
    };

    // Source token and target are taken from the recorded coherent Candidate
    // evidence. The pointer is treated only as an opaque continuity token.
    inline constexpr CandidateContextCase kRecordedCandidate{
        0x1d354a2b648, 70.0f, 90.0f, 89.97f, 90.0f, 0x1d29554ae08};
}
