#include "../../src/cinematics/cinematic_initialization.hpp"

#include <iostream>

namespace
{
    enum class FailureMode
    {
        None,
        Aspect,
        Fov,
        Commit,
    };

    FailureMode g_failureMode = FailureMode::None;
    bool g_aspectHook{};
    bool g_fovHook{};
    int g_aspectInstallCalls{};
    int g_fovInstallCalls{};
    int g_aspectRollbackCalls{};
    int g_fovRollbackCalls{};
    int g_commitCalls{};

    void ResetState(FailureMode failureMode)
    {
        g_failureMode = failureMode;
        g_aspectHook = false;
        g_fovHook = false;
        g_aspectInstallCalls = 0;
        g_fovInstallCalls = 0;
        g_aspectRollbackCalls = 0;
        g_fovRollbackCalls = 0;
        g_commitCalls = 0;
    }

    bool InstallAspect()
    {
        ++g_aspectInstallCalls;
        g_aspectHook = true;
        return g_failureMode != FailureMode::Aspect;
    }

    bool InstallFov()
    {
        ++g_fovInstallCalls;
        g_fovHook = true;
        return g_failureMode != FailureMode::Fov;
    }

    void RollbackAspect() noexcept
    {
        ++g_aspectRollbackCalls;
        g_aspectHook = false;
    }

    void RollbackFov() noexcept
    {
        ++g_fovRollbackCalls;
        g_fovHook = false;
    }

    bool Commit()
    {
        ++g_commitCalls;
        return g_failureMode != FailureMode::Commit;
    }

    bool Check(bool condition, const char* name)
    {
        if (!condition) std::cerr << name << ": FAIL\n";
        return condition;
    }

    bool CheckNativeBypass()
    {
        ResetState(FailureMode::None);
        const auto status = cinematics::InitializeTransactional(false,
            InstallAspect, InstallFov, RollbackAspect, RollbackFov);
        return Check(status == plugin::FeatureStatus::Disabled, "native_status") &&
            Check(!g_aspectHook && !g_fovHook, "native_hooks") &&
            Check(g_aspectInstallCalls == 0 && g_fovInstallCalls == 0,
                "native_install_bypass");
    }

    bool CheckSuccess()
    {
        ResetState(FailureMode::None);
        const auto status = cinematics::InitializeTransactional(true,
            InstallAspect, InstallFov, RollbackAspect, RollbackFov);
        return Check(status == plugin::FeatureStatus::Available, "success_status") &&
            Check(g_aspectHook && g_fovHook, "success_hooks") &&
            Check(g_aspectRollbackCalls == 0 && g_fovRollbackCalls == 0,
                "success_rollbacks");
    }

    bool CheckAspectFailure()
    {
        ResetState(FailureMode::Aspect);
        const auto status = cinematics::InitializeTransactional(true,
            InstallAspect, InstallFov, RollbackAspect, RollbackFov);
        return Check(status == plugin::FeatureStatus::Failed, "aspect_failure_status") &&
            Check(!g_aspectHook && !g_fovHook, "aspect_failure_hooks") &&
            Check(g_aspectInstallCalls == 1 && g_fovInstallCalls == 0,
                "aspect_failure_order") &&
            Check(g_aspectRollbackCalls == 1 && g_fovRollbackCalls == 1,
                "aspect_failure_rollbacks");
    }

    bool CheckFovFailure()
    {
        ResetState(FailureMode::Fov);
        const auto status = cinematics::InitializeTransactional(true,
            InstallAspect, InstallFov, RollbackAspect, RollbackFov);
        return Check(status == plugin::FeatureStatus::Failed, "fov_failure_status") &&
            Check(!g_aspectHook && !g_fovHook, "fov_failure_hooks") &&
            Check(g_aspectInstallCalls == 1 && g_fovInstallCalls == 1,
                "fov_failure_order") &&
            Check(g_aspectRollbackCalls == 1 && g_fovRollbackCalls == 1,
                "fov_failure_rollbacks");
    }

    bool CheckCommitFailure()
    {
        ResetState(FailureMode::Commit);
        const auto status = cinematics::InitializeTransactional(true,
            InstallAspect, InstallFov, RollbackAspect, RollbackFov, Commit);
        return Check(status == plugin::FeatureStatus::Failed, "commit_failure_status") &&
            Check(!g_aspectHook && !g_fovHook, "commit_failure_hooks") &&
            Check(g_commitCalls == 1, "commit_failure_commit") &&
            Check(g_aspectRollbackCalls == 1 && g_fovRollbackCalls == 1,
                "commit_failure_rollbacks");
    }
}

int main()
{
    const bool native = CheckNativeBypass();
    const bool success = CheckSuccess();
    const bool aspectFailure = CheckAspectFailure();
    const bool fovFailure = CheckFovFailure();
    const bool commitFailure = CheckCommitFailure();
    std::cout << "native=" << (native ? "PASS" : "FAIL")
        << " success=" << (success ? "PASS" : "FAIL")
        << " aspect_failure=" << (aspectFailure ? "PASS" : "FAIL")
        << " fov_failure=" << (fovFailure ? "PASS" : "FAIL") << "\n";
    std::cout << "commit_failure=" << (commitFailure ? "PASS" : "FAIL") << "\n";
    return native && success && aspectFailure && fovFailure && commitFailure ? 0 : 1;
}
