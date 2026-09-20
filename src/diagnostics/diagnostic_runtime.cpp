#include "diagnostic_runtime.hpp"

#include <atomic>

namespace diagnostics
{
    namespace
    {
        std::atomic<bool> g_enabled{false};
    }

    void SetEnabled(bool enabled) noexcept
    {
        g_enabled.store(enabled, std::memory_order_release);
    }

    bool Enabled() noexcept
    {
        return g_enabled.load(std::memory_order_acquire);
    }
}
