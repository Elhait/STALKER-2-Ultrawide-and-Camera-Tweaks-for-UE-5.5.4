#include "gameplay_camera.hpp"

#include "../hooks/instruction_validator.hpp"
#include "../hooks/signatures/signature_definitions.hpp"

#include <windows.h>
#include <cstring>
#include <limits>
#include <cmath>

namespace gameplay
{
    bool WriteAspectOnly(std::uintptr_t source, std::uintptr_t aspectOffset,
        float aspect, WritablePredicate isWritable)
    {
        if (!isWritable || source > (std::numeric_limits<std::uintptr_t>::max)() - aspectOffset)
            return false;
        const auto aspectAddress = source + aspectOffset;
        if (!isWritable(aspectAddress, sizeof(aspect))) return false;
        std::memcpy(reinterpret_cast<void*>(aspectAddress), &aspect, sizeof(aspect));
        return true;
    }

    GameplayContextChange EvaluateGameplayContextChange(
        std::uintptr_t previousSource, float previousFov,
        std::uintptr_t source, float fov, float fovJumpThreshold) noexcept
    {
        const bool sourceChanged = previousSource != 0 && source != 0 && previousSource != source;
        const bool materialFovJump = previousSource == source && std::isfinite(previousFov) &&
            std::isfinite(fov) && std::fabs(fov - previousFov) > fovJumpThreshold;
        return { sourceChanged, materialFovJump };
    }

    bool WriteAspectAndFlags(std::uintptr_t source, std::uintptr_t aspectOffset,
        std::uintptr_t flagsOffset, float aspect, std::uint8_t flags,
        WritablePredicate isWritable)
    {
        if (!isWritable || source > (std::numeric_limits<std::uintptr_t>::max)() - aspectOffset ||
            source > (std::numeric_limits<std::uintptr_t>::max)() - flagsOffset) return false;
        const auto aspectAddress = source + aspectOffset;
        const auto flagsAddress = source + flagsOffset;
        if (!isWritable(aspectAddress, sizeof(aspect)) ||
            !isWritable(flagsAddress, sizeof(flags))) return false;
        std::memcpy(reinterpret_cast<void*>(aspectAddress), &aspect, sizeof(aspect));
        std::memcpy(reinterpret_cast<void*>(flagsAddress), &flags, sizeof(flags));
        return true;
    }

    CameraWriterResolution ResolveCameraWriter(void* executable)
    {
        const auto* base = reinterpret_cast<const std::uint8_t*>(executable);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return {};
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return {};

        const auto* section = IMAGE_FIRST_SECTION(nt);
        const std::uint8_t* textStart = nullptr;
        std::size_t textSize = 0;
        for (std::uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
            if (std::memcmp(section->Name, ".text", 5) == 0) {
                textStart = base + section->VirtualAddress;
                textSize = section->Misc.VirtualSize;
                break;
            }
        }
        if (!textStart || textSize < sizeof(hooks::signatures::CameraWriter))
            return { nullptr, 0, true };

        std::uint8_t* match = nullptr;
        std::size_t matches = 0;
        for (std::size_t offset = 0; offset <= textSize - sizeof(hooks::signatures::CameraWriter); ++offset) {
            const auto* candidate = textStart + offset;
            bool matchesSignature = true;
            for (std::size_t i = 0; i < sizeof(hooks::signatures::CameraWriter); ++i) {
                // The 32-bit relative target of JNZ may move between builds.
                if ((i < 17 || i >= 21) && candidate[i] != hooks::signatures::CameraWriter[i]) {
                    matchesSignature = false;
                    break;
                }
            }
            if (matchesSignature) {
                match = const_cast<std::uint8_t*>(candidate);
                ++matches;
            }
        }
        if (matches != 1) return { nullptr, matches, true };

        auto* target = match + hooks::signatures::FovWriteOffsetInCameraWriter;
        const hooks::ExecutableSpan textSpan{ const_cast<std::uint8_t*>(textStart), textSize };
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        const bool verified = hooks::validation::DecodeInstruction(target, instruction, operands, textSpan) &&
            instruction.mnemonic == ZYDIS_MNEMONIC_MOVSS && instruction.operand_count_visible >= 2 &&
            operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            operands[0].mem.base == ZYDIS_REGISTER_RBX && operands[0].mem.disp.has_displacement &&
            operands[0].mem.disp.value == 0x30 && operands[1].reg.value == ZYDIS_REGISTER_XMM0;
        return verified ? CameraWriterResolution{ target, matches, true }
                        : CameraWriterResolution{ nullptr, matches, true };
    }
}
