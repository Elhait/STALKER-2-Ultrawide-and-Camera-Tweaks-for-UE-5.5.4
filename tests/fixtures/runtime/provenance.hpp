#pragma once

namespace runtime_evidence
{
    struct Provenance
    {
        const char* scenarioId;
        const char* sourceReference;
        const char* gameSha256;
        const char* modSha256;
        const char* buildVersion;
    };
}
