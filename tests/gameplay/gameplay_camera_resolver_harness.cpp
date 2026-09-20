#include "../../src/gameplay/gameplay_camera.hpp"
#include "../../src/hooks/signatures/signature_definitions.hpp"

#include <Windows.h>
#include <cstdio>
#include <cstring>

namespace
{
    struct PeFixture
    {
        std::uint8_t* base{};
        std::uint8_t* text{};

        PeFixture()
        {
            base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x5000,
                MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
            if (!base) return;
            auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
            dos->e_magic = IMAGE_DOS_SIGNATURE;
            dos->e_lfanew = 0x100;
            auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
            nt->Signature = IMAGE_NT_SIGNATURE;
            nt->FileHeader.NumberOfSections = 1;
            nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            auto* section = IMAGE_FIRST_SECTION(nt);
            std::memcpy(section->Name, ".text", 5);
            section->VirtualAddress = 0x1000;
            section->Misc.VirtualSize = 0x1000;
            section->Characteristics = IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
            text = base + section->VirtualAddress;
        }

        ~PeFixture() { if (base) VirtualFree(base, 0, MEM_RELEASE); }
    };

    void Place(PeFixture& fixture, std::size_t offset, bool validOperand)
    {
        std::memcpy(fixture.text + offset, hooks::signatures::CameraWriter,
            sizeof(hooks::signatures::CameraWriter));
        if (!validOperand)
            fixture.text[offset + hooks::signatures::FovWriteOffsetInCameraWriter + 3] = 0x42;
    }

    bool Check(bool value, const char* name)
    {
        if (!value) std::fprintf(stderr, "%s: FAIL\n", name);
        return value;
    }
}

int main()
{
    bool pass = true;
    PeFixture fixture;
    if (!fixture.base) return 1;
    auto zero = gameplay::ResolveCameraWriter(fixture.base);
    pass &= Check(zero.matches == 0 && zero.writeAddress == nullptr, "resolver_zero_match");
    Place(fixture, 0x80, true);
    auto one = gameplay::ResolveCameraWriter(fixture.base);
    pass &= Check(one.matches == 1 && one.writeAddress == fixture.text + 0x80 +
        hooks::signatures::FovWriteOffsetInCameraWriter, "resolver_one_valid_match");
    Place(fixture, 0x180, true);
    auto many = gameplay::ResolveCameraWriter(fixture.base);
    pass &= Check(many.matches == 2 && many.writeAddress == nullptr, "resolver_multiple_refusal");

    PeFixture invalid;
    Place(invalid, 0x80, false);
    auto signatureMismatch = gameplay::ResolveCameraWriter(invalid.base);
    pass &= Check(signatureMismatch.matches == 0 && signatureMismatch.writeAddress == nullptr,
        "resolver_signature_mismatch_refusal");

    std::printf("gameplay_camera_resolver=%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
