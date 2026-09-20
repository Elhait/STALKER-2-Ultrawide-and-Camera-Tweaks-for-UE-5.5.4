#pragma once

#include "../config/feature_config.hpp"

#include <cstddef>
#include <cstdint>

namespace cinematics
{
    using AutoAspectResolver = float (*)();
    using WritablePredicate = bool (*)(std::uintptr_t address, std::size_t size);
    using AspectValidator = bool (*)(float aspect);

    bool OverrideEnabled(config::CinematicAspectPolicy policy);
    float ResolveAspect(config::CinematicAspectPolicy policy, float nativeAspect,
        float cinemaAspect, float wideAspect, AutoAspectResolver autoResolver);

    struct AspectStoreResolution
    {
        std::uint8_t* store{};
        std::size_t matches{};
        bool imageValid{};
    };

    AspectStoreResolution ResolveAspectStore(void* executable, const char* signature,
        std::uintptr_t aspectOffset, const std::uint8_t* storePrefix,
        std::size_t storePrefixSize, const std::uint8_t* expectedImmediate,
        std::size_t immediateOffset, std::size_t immediateSize,
        std::size_t storeInstructionLength, std::uint32_t expectedImmediateValue);

    struct AspectStoreApplication
    {
        float aspect{};
        bool writable{};
    };

    AspectStoreApplication ApplyAspectStore(std::uintptr_t targetObject,
        float resolvedAspect, float nativeAspect, std::uintptr_t aspectOffset,
        WritablePredicate isWritable, AspectValidator isValidAspect);
}
