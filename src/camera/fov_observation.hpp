#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>

namespace camera
{
    enum class FovObservationBoundary : std::uint8_t
    {
        CameraWriter,
        CinematicEnter,
    };

    enum class FovSpace : std::uint8_t
    {
        Unknown,
        Native,
        Transformed,
    };

    enum class FovProvenance : std::uint8_t
    {
        Unavailable,
        NativeRegisterInput,
        NativeObjectField,
        ModTransformResult,
        PassThroughResult,
        CachedCinematicEnter,
        ResolvedAspect,
    };

    struct FovSample
    {
        float value{std::numeric_limits<float>::quiet_NaN()};
        FovSpace space{FovSpace::Unknown};
        FovProvenance provenance{FovProvenance::Unavailable};
        bool valid{};
    };

    struct ScalarSample
    {
        float value{std::numeric_limits<float>::quiet_NaN()};
        FovProvenance provenance{FovProvenance::Unavailable};
        bool valid{};
    };

    struct FovWriterSourceToken
    {
        std::uintptr_t value{};
        bool valid{};
    };

    struct CameraFovObservation
    {
        FovObservationBoundary boundary{FovObservationBoundary::CameraWriter};
        FovSample inputFov{};
        FovSample resultFov{};
        ScalarSample aspect{};
        FovWriterSourceToken writerSource{};
        std::uint8_t writerFlags{};
        bool writerFlagsValid{};
        std::uint64_t publicationSequence{};
    };

    const char* FovObservationBoundaryName(FovObservationBoundary value) noexcept;
    const char* FovSpaceName(FovSpace value) noexcept;
    const char* FovProvenanceName(FovProvenance value) noexcept;

    class CameraFovObservationStore
    {
    public:
        CameraFovObservation Publish(CameraFovObservation observation);
        std::optional<CameraFovObservation> ReadLatest(
            FovObservationBoundary boundary) const;

    private:
        static std::size_t Index(FovObservationBoundary boundary) noexcept;

        mutable std::mutex mutex_;
        std::uint64_t nextSequence_{};
        std::array<CameraFovObservation, 2> latest_{};
        std::array<bool, 2> valid_{};
    };
}
