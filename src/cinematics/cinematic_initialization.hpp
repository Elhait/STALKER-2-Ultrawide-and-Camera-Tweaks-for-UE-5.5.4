#pragma once

#include "../plugin/feature_status.hpp"

namespace cinematics
{
    using ComponentInstall = bool (*)();
    using ComponentRollback = void (*)() noexcept;
    using ComponentCommit = bool (*)();

    plugin::FeatureStatus InitializeTransactional(bool requested,
        ComponentInstall installAspect, ComponentInstall installFov,
        ComponentRollback rollbackAspect, ComponentRollback rollbackFov,
        ComponentCommit commit = nullptr);
}
