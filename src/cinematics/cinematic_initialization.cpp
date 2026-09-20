#include "cinematic_initialization.hpp"

#include <stdexcept>

namespace cinematics
{
    plugin::FeatureStatus InitializeTransactional(bool requested,
        ComponentInstall installAspect, ComponentInstall installFov,
        ComponentRollback rollbackAspect, ComponentRollback rollbackFov,
        ComponentCommit commit)
    {
        if (!requested) return plugin::FeatureStatus::Disabled;

        try {
            if (!installAspect || !installAspect())
                throw std::runtime_error("cinematic aspect component initialization failed");
            if (!installFov || !installFov())
                throw std::runtime_error("cinematic FOV component initialization failed");
            if (commit && !commit())
                throw std::runtime_error("cinematic component commit failed");
            return plugin::FeatureStatus::Available;
        } catch (...) {
            if (rollbackFov) rollbackFov();
            if (rollbackAspect) rollbackAspect();
            return plugin::FeatureStatus::Failed;
        }
    }
}
