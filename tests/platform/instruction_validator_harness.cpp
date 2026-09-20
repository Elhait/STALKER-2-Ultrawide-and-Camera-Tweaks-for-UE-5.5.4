#include "../../src/hooks/instruction_validator.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* name)
    {
        if (!condition) std::cerr << name << ": FAIL\n";
        return condition;
    }
}

int main()
{
    std::uint8_t bytes[64]{};
    bytes[4] = 0x90;
    bytes[5] = 0x91;
    const hooks::ExecutableSpan span{ bytes, 16 };
    const std::uint8_t needle[] = { 0x90, 0x91 };
    bool pass = hooks::validation::ContainsBytes(bytes + 4, 2, needle, sizeof(needle), span);
    pass &= !hooks::validation::ContainsBytes(bytes + 15, 2, needle, sizeof(needle), span);
    pass &= !hooks::validation::ContainsBytes(bytes + 4, 13, needle, sizeof(needle), span);
    std::uint8_t nop = 0x90;
    bytes[0] = nop;
    ZydisDecodedInstruction decoded{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
    pass &= hooks::validation::DecodeInstruction(bytes, decoded, operands,
        hooks::ExecutableSpan{ bytes, 1 });
    pass &= !hooks::validation::DecodeInstruction(bytes, decoded, operands,
        hooks::ExecutableSpan{ bytes, 0 });

    bytes[8] = 0xE8;
    const auto forwardTarget = reinterpret_cast<std::intptr_t>(bytes + 24);
    const auto forwardBase = reinterpret_cast<std::intptr_t>(bytes + 13);
    const auto forwardDisplacement = static_cast<std::int32_t>(forwardTarget - forwardBase);
    std::memcpy(bytes + 9, &forwardDisplacement, sizeof(forwardDisplacement));
    const hooks::ExecutableSpan callSpan{ bytes, 32 };
    pass &= Check(hooks::validation::IsCallRel32(bytes + 8, callSpan), "forward_call_decode");
    pass &= Check(hooks::validation::ResolveRel32CallTarget(bytes + 8, callSpan) == bytes + 24,
        "forward_call_target");

    bytes[28] = 0xE8;
    const auto backwardTarget = reinterpret_cast<std::intptr_t>(bytes + 16);
    const auto backwardBase = reinterpret_cast<std::intptr_t>(bytes + 33);
    const auto backwardDisplacement = static_cast<std::int32_t>(backwardTarget - backwardBase);
    std::memcpy(bytes + 29, &backwardDisplacement, sizeof(backwardDisplacement));
    pass &= Check(hooks::validation::IsCallRel32(bytes + 28,
        hooks::ExecutableSpan{ bytes, 40 }), "backward_call_decode");
    pass &= Check(hooks::validation::ResolveRel32CallTarget(bytes + 28,
            hooks::ExecutableSpan{ bytes, 40 }) == bytes + 16, "backward_call_target");

    bytes[40] = 0xE8;
    const auto outsideTarget = reinterpret_cast<std::intptr_t>(bytes + 60);
    const auto outsideBase = reinterpret_cast<std::intptr_t>(bytes + 45);
    const auto outsideDisplacement = static_cast<std::int32_t>(outsideTarget - outsideBase);
    std::memcpy(bytes + 41, &outsideDisplacement, sizeof(outsideDisplacement));
    pass &= !hooks::validation::ResolveRel32CallTarget(bytes + 40,
        hooks::ExecutableSpan{ bytes, 48 });
    pass &= !hooks::validation::IsCallRel32(bytes + 48,
        hooks::ExecutableSpan{ bytes, 50 });
    bytes[52] = 0x90;
    pass &= !hooks::validation::ResolveRel32CallTarget(bytes + 52,
        hooks::ExecutableSpan{ bytes, 64 });

    std::cout << "validator_decode_rel32_span=" << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
