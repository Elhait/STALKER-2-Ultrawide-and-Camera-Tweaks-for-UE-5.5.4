#include "helper.hpp"
#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace {
constexpr std::uintptr_t kEnterRva = 0x2EE6936;
constexpr std::uintptr_t kExitRva = 0x2EE69A7;
constexpr std::uint8_t kEnterBytes[] = {0xE8, 0x71, 0x71, 0xC8, 0x03};
constexpr std::uint8_t kExitBytes[] = {0xE8, 0x00, 0x71, 0xC8, 0x03};

constexpr std::uint8_t kWriterSignature[] = {
    0xF6, 0x86, 0x62, 0x02, 0, 0, 0x10,
    0xF3, 0x0F, 0x10, 0x86, 0x30, 0x02, 0, 0,
    0x0F, 0x85, 0, 0, 0, 0,
    0x48, 0x8D, 0x4B, 0x30,
    0xF3, 0x0F, 0x11, 0x43, 0x30,
    0xF3, 0x0F, 0x10, 0x86, 0x54, 0x02, 0, 0,
    0xF3, 0x0F, 0x11, 0x43, 0x5C,
    0x0F, 0xB6, 0x96, 0x59, 0x02, 0, 0,
    0x8B, 0x43, 0x68, 0x83, 0xE2, 1, 0x83, 0xE0, 0xFE,
    9, 0xD0, 0x89, 0x43, 0x68,
    0x0F, 0xB6, 0x96, 0x59, 0x02, 0, 0,
    0x83, 0xE2, 4, 0x83, 0xE0, 0xFB,
    9, 0xD0, 0x89, 0x43, 0x68,
    0x8A, 0x96, 0x63, 0x02, 0, 0,
    0x88, 0x53, 0x6C};
constexpr std::size_t kWriterObserveOffset = 88;

HMODULE gModule{};
HMODULE gExecutable = GetModuleHandle(nullptr);
std::shared_ptr<spdlog::logger> gLogger;
SafetyHookMid gEnterHook, gExitHook, gWriterHook;
std::atomic<int> gPhase{0}; // 0 gameplay, 1 cinematic, 2 exit pending
std::atomic<std::uint64_t> gSequence{};

struct Snapshot {
    std::uintptr_t source{};
    std::uintptr_t output{};
    std::uint8_t axis{};
    std::uint8_t sourceFlags{};
    std::uint8_t outputAxis{};
    std::uint8_t outputValid{};
    int phase{};
    bool valid{};
};

Snapshot gLast{};

template <class T>
bool Read(std::uintptr_t address, T& value) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
        return false;
    }
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (address >= end || sizeof(T) > end - address) {
        return false;
    }
    std::memcpy(&value, reinterpret_cast<void*>(address), sizeof(T));
    return true;
}

bool Match(std::uintptr_t address, const std::uint8_t* bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        std::uint8_t value{};
        if (!Read(address + i, value) || value != bytes[i]) {
            return false;
        }
    }
    return true;
}

std::uintptr_t Rva(std::uintptr_t address) {
    const auto base = reinterpret_cast<std::uintptr_t>(gExecutable);
    return address >= base && address - base < 0x20000000 ? address - base : 0;
}

const char* PhaseName(int phase) {
    switch (phase) {
    case 1: return "cinematic";
    case 2: return "exit-pending";
    default: return "gameplay";
    }
}

void LogPhase(const char* marker, int phase) {
    if (gLogger) {
        gLogger->info("TRACE marker={} phase={} phaseId={} thread={}",
                      marker, PhaseName(phase), phase, GetCurrentThreadId());
    }
}

void Enter(SafetyHookContext&) {
    gPhase.store(1, std::memory_order_release);
    LogPhase("ENTER", 1);
}

void Exit(SafetyHookContext&) {
    gPhase.store(2, std::memory_order_release);
    LogPhase("EXIT", 2);
}

void Writer(SafetyHookContext& context) {
    const auto source = static_cast<std::uintptr_t>(context.rsi);
    const auto output = static_cast<std::uintptr_t>(context.rbx);
    const int phase = gPhase.load(std::memory_order_acquire);

    Snapshot current{};
    current.source = source;
    current.output = output;
    current.phase = phase;
    current.valid = Read(source + 0x258, current.axis) &&
                    Read(source + 0x259, current.sourceFlags) &&
                    Read(output + 0x60, current.outputAxis) &&
                    Read(output + 0x64, current.outputValid);

    const bool changed = !gLast.valid ||
        current.source != gLast.source || current.output != gLast.output ||
        current.axis != gLast.axis || current.sourceFlags != gLast.sourceFlags ||
        current.outputAxis != gLast.outputAxis || current.outputValid != gLast.outputValid ||
        current.phase != gLast.phase;

    if (changed && gLogger) {
        const auto sequence = ++gSequence;
        gLogger->info(
            "TRACE sample={} stage=writer source=0x{:X} output=0x{:X} phase={} phaseId={} "
            "source258=0x{:02X} source259=0x{:02X} gate02={} output60=0x{:02X} output64={} "
            "reads={} writerRva=0x{:X} thread={}",
            sequence, source, output, PhaseName(phase), phase,
            static_cast<unsigned>(current.axis), static_cast<unsigned>(current.sourceFlags),
            (current.sourceFlags & 0x2) != 0, static_cast<unsigned>(current.outputAxis),
            static_cast<unsigned>(current.outputValid), current.valid,
            Rva(context.rip), GetCurrentThreadId());
        gLogger->flush();
    }
    gLast = current;

    if (phase == 2) {
        gPhase.store(0, std::memory_order_release);
        LogPhase("EXIT_WRITER_OBSERVED", 0);
    }
}

bool FindWriter(std::uint8_t*& observedInstruction) {
    std::vector<std::uint8_t*> matches;
    Memory::ForEachExecutableSection(gExecutable, [&](std::uint8_t* section, std::size_t size) {
        for (std::size_t offset = 0; offset + sizeof(kWriterSignature) <= size; ++offset) {
            bool match = true;
            for (std::size_t i = 0; i < sizeof(kWriterSignature); ++i) {
                if (i < 17 || i >= 21) {
                    if (section[offset + i] != kWriterSignature[i]) {
                        match = false;
                        break;
                    }
                }
            }
            if (match) {
                matches.push_back(section + offset);
            }
        }
    });
    if (matches.size() != 1) {
        return false;
    }
    observedInstruction = matches.front() + kWriterObserveOffset;
    return observedInstruction[0] == 0x88 && observedInstruction[1] == 0x53 &&
           observedInstruction[2] == 0x6C;
}

DWORD WINAPI Init(void*) {
    WCHAR modulePath[MAX_PATH]{};
    GetModuleFileNameW(gModule, modulePath, MAX_PATH);
    const auto logPath = std::filesystem::path(modulePath).remove_filename() /
                         "STALKER2CinematicAxisPolicyLifecycleTrace204.log";
    std::ofstream(logPath, std::ios::trunc).close();

    try {
        gLogger = spdlog::basic_logger_mt("STALKER2CinematicAxisPolicyLifecycleTrace204",
                                          logPath.string(), true);
        gLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
        gLogger->flush_on(spdlog::level::info);
    } catch (...) {
        return 0;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(gExecutable);
    std::uint8_t* writerInstruction{};
    const bool identity = Match(base + kEnterRva, kEnterBytes, sizeof(kEnterBytes)) &&
                          Match(base + kExitRva, kExitBytes, sizeof(kExitBytes)) &&
                          FindWriter(writerInstruction);
    gLogger->info("TRACE identity_gate=current Steam 2.0.4 anchors validated={} "
                  "enterRva=0x{:X} exitRva=0x{:X} writerRva=0x{:X} readOnly=true",
                  identity, kEnterRva, kExitRva,
                  identity ? Rva(reinterpret_cast<std::uintptr_t>(writerInstruction)) : 0);
    if (!identity) {
        gLogger->error("TRACE setup refused safely: current-build instruction contract failed.");
        return 0;
    }

    try {
        gEnterHook = safetyhook::create_mid(reinterpret_cast<void*>(base + kEnterRva), Enter);
        gExitHook = safetyhook::create_mid(reinterpret_cast<void*>(base + kExitRva), Exit);
        gWriterHook = safetyhook::create_mid(writerInstruction, Writer);
        if (!gEnterHook || !gExitHook || !gWriterHook ||
            !gEnterHook.enable() || !gExitHook.enable() || !gWriterHook.enable()) {
            throw std::runtime_error("hook creation or enable failed");
        }
        gLogger->info("TRACE installed: writer boundary lifecycle observer; no memory writes.");
    } catch (const std::exception& error) {
        gWriterHook.reset();
        gExitHook.reset();
        gEnterHook.reset();
        gLogger->error("TRACE setup refused safely: {}", error.what());
    }
    return 0;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    gModule = module;
    DisableThreadLibraryCalls(module);
    if (auto thread = CreateThread(nullptr, 0, Init, nullptr, 0, nullptr)) {
        CloseHandle(thread);
    }
    return TRUE;
}
