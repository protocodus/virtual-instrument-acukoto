cmake_minimum_required(VERSION 3.22)

# Acukoto: a physically modelled koto. The JUCE-free engine and performer,
# the demo renderer and the analysis tools; there is no plug-in yet.
project(Acukoto VERSION 0.25.5 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ACUKOTO_BUILD_TOOLS "Build the demo renderer and the analysis tools" ON)
option(ACUKOTO_ENABLE_SANITIZERS "Instrument the DSP, tests and tools with ASan and UBSan" OFF)
option(ACUKOTO_BUILD_FUZZER "Build AcukotoDspFuzz as a Clang libFuzzer target" OFF)
option(ACUKOTO_REQUIRE_PYTHON_TESTS
    "Fail to configure unless Python 3 is found for Acukoto.DemoMidiSources (CI sets it)" OFF)

if(ACUKOTO_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "ACUKOTO_ENABLE_SANITIZERS needs Clang or GCC")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

include(CTest)
find_package(Threads REQUIRED)

set(ACUKOTO_DSP_SOURCES
    Source/DSP/AcukotoEngine.cpp
    Source/DSP/AcukotoPerformer.cpp)

add_library(AcukotoDSP STATIC
    ${ACUKOTO_DSP_SOURCES}
    Source/DSP/AcukotoEngine.h
    Source/DSP/PluckShape.h
    Source/DSP/AcukotoPerformer.h
    Source/DSP/AcukotoPresets.h
    Source/DSP/OutputStage.h)
target_include_directories(AcukotoDSP PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(AcukotoDSP PUBLIC cxx_std_20)
set_target_properties(AcukotoDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(AcukotoDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(AcukotoDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

# A Reason Rack Extension compiles this engine and performer itself, as C++17
# (the Jukebox toolchain forces -std=c++17) with -Wall -Wextra -Werror. Build
# them that way here too, so a C++20-only construct or a new warning fails in
# this repository rather than only in the Rack Extension's.
if(NOT MSVC)
    add_library(AcukotoDSPCxx17 OBJECT ${ACUKOTO_DSP_SOURCES})
    target_include_directories(AcukotoDSPCxx17 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    set_target_properties(AcukotoDSPCxx17 PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_compile_options(AcukotoDSPCxx17 PRIVATE -Wall -Wextra -Werror -fno-exceptions)
endif()

if(ACUKOTO_BUILD_TOOLS)
    # One README demo per factory preset from Docs/audio/midi (Tools/
    # PresetDemoRenderer.h is the shared portfolio copy; do not edit it here).
    add_executable(RenderAcukotoPresetDemos Tools/RenderPresetDemos.cpp)
    target_link_libraries(RenderAcukotoPresetDemos PRIVATE AcukotoDSP Threads::Threads)

    # Any MIDI file through the Performer, and the strings' and body's design.
    add_executable(RenderAcukotoMidi Tools/RenderMidi.cpp)
    target_link_libraries(RenderAcukotoMidi PRIVATE AcukotoDSP)

    # Every string plucked alone, for measuring against recorded notes.
    add_executable(RenderAcukotoNotes Tools/RenderNotes.cpp)
    target_link_libraries(RenderAcukotoNotes PRIVATE AcukotoDSP)

    # A phrase script with the koto's left-hand techniques (oshide, ato-oshi,
    # yuri) and plucks by string number.
    add_executable(RenderAcukotoPhrase Tools/RenderPhrase.cpp)
    target_link_libraries(RenderAcukotoPhrase PRIVATE AcukotoDSP)

    # How evenly each preset's strings sound (StringSpec::touch).
    add_executable(AcukotoLevels Tools/MeasureLevels.cpp)
    target_link_libraries(AcukotoLevels PRIVATE AcukotoDSP)

    # Paired callback CPU/wall timing and raw sample comparison; native only.
    add_executable(AcukotoCpuProbe Tools/MeasureCpu.cpp)
    target_link_libraries(AcukotoCpuProbe PRIVATE AcukotoDSP)

    add_executable(AcukotoKeyboardSurvey Tools/KeyboardSurvey.cpp)
    target_link_libraries(AcukotoKeyboardSurvey PRIVATE AcukotoDSP)

    # Setup-only acoustic power observer. Compiles its own engine with the
    # observation hook; the normal DSP target has no diagnostic overhead.
    add_executable(AcukotoAcousticPower Tools/AuditAcousticPower.cpp Source/DSP/AcukotoEngine.cpp)
    target_include_directories(AcukotoAcousticPower PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(AcukotoAcousticPower PRIVATE ACUKOTO_RADIATION_DIAGNOSTICS)
endif()

if(BUILD_TESTING)
    if(ACUKOTO_REQUIRE_PYTHON_TESTS)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
    else()
        find_package(Python3 COMPONENTS Interpreter)
    endif()

    add_executable(AcukotoTuningTests Tests/TuningTests.cpp)
    target_link_libraries(AcukotoTuningTests PRIVATE AcukotoDSP)
    add_test(NAME Acukoto.Tuning COMMAND AcukotoTuningTests)
    set_tests_properties(Acukoto.Tuning PROPERTIES TIMEOUT 600)

    # Internal contact contract includes the engine implementation itself;
    # production DSP has no test hooks or diagnostic overhead.
    add_executable(AcukotoHandContactTests Tests/HandContactTests.cpp)
    target_include_directories(AcukotoHandContactTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    add_test(NAME Acukoto.HandContact COMMAND AcukotoHandContactTests)
    set_tests_properties(Acukoto.HandContact PROPERTIES TIMEOUT 60)

    add_executable(AcukotoEngineTests Tests/EngineTests.cpp)
    target_link_libraries(AcukotoEngineTests PRIVATE AcukotoDSP)
    add_test(NAME Acukoto.Engine COMMAND AcukotoEngineTests)
    set_tests_properties(Acukoto.Engine PROPERTIES TIMEOUT 600)

    add_executable(AcukotoPlayingOwnershipTests Tests/PlayingOwnershipTests.cpp)
    target_link_libraries(AcukotoPlayingOwnershipTests PRIVATE AcukotoDSP)
    add_test(NAME Acukoto.PlayingOwnership COMMAND AcukotoPlayingOwnershipTests)
    set_tests_properties(Acukoto.PlayingOwnership PROPERTIES TIMEOUT 120)

    add_executable(AcukotoRangeSafetyTests Tests/RangeSafetyTests.cpp)
    target_link_libraries(AcukotoRangeSafetyTests PRIVATE AcukotoDSP)
    add_test(NAME Acukoto.RangeSafety COMMAND AcukotoRangeSafetyTests)
    set_tests_properties(Acukoto.RangeSafety PROPERTIES TIMEOUT 180)

    add_executable(AcukotoPluckShapeTests Tests/PluckShapeTests.cpp)
    target_include_directories(AcukotoPluckShapeTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    add_test(NAME Acukoto.PluckShape COMMAND AcukotoPluckShapeTests)

    # Offline-only port/work instrumentation: no per-sample diagnostic sums
    # or observer fields in the production DSP library or Rack build.
    add_executable(AcukotoStrikeEnergyTests Tests/StrikeEnergyTests.cpp Source/DSP/AcukotoEngine.cpp)
    target_include_directories(AcukotoStrikeEnergyTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(AcukotoStrikeEnergyTests PRIVATE ACUKOTO_ENERGY_DIAGNOSTICS)
    add_test(NAME Acukoto.StrikeEnergy COMMAND AcukotoStrikeEnergyTests)
    set_tests_properties(Acukoto.StrikeEnergy PROPERTIES TIMEOUT 600)

    add_executable(AcukotoAcousticPowerBudgetTests Tests/AcousticPowerBudgetTests.cpp)
    target_include_directories(AcukotoAcousticPowerBudgetTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    add_test(NAME Acukoto.AcousticPowerBudget COMMAND AcukotoAcousticPowerBudgetTests)
    if(ACUKOTO_BUILD_TOOLS)
        add_test(NAME Acukoto.AcousticPowerReport COMMAND AcukotoAcousticPower --min-hz 298 --max-hz 300 --polar 16)
        set_tests_properties(Acukoto.AcousticPowerReport PROPERTIES TIMEOUT 120)
    endif()

    # U4 input fuzzing (Tests/DspFuzz.cpp). The replay build runs on every
    # platform: committed reproducers in Tests/Fixtures/fuzz, then seeded
    # random inputs. The libFuzzer build compiles the DSP with coverage, so it
    # needs Clang; its short run is the fuzz tier.
    add_executable(AcukotoDspFuzzReplay Tests/DspFuzz.cpp)
    target_link_libraries(AcukotoDspFuzzReplay PRIVATE AcukotoDSP)
    target_compile_definitions(AcukotoDspFuzzReplay PRIVATE PROTOCODUS_FUZZ_MAIN)
    file(GLOB ACUKOTO_FUZZ_REPRODUCERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/fuzz/*")
    add_test(NAME Acukoto.DspFuzzReplay
             COMMAND AcukotoDspFuzzReplay -seed=1 -runs=100 -max_len=1024 ${ACUKOTO_FUZZ_REPRODUCERS})
    set_tests_properties(Acukoto.DspFuzzReplay PROPERTIES TIMEOUT 900)
    if(ACUKOTO_BUILD_FUZZER)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "ACUKOTO_BUILD_FUZZER needs Clang")
        endif()
        add_executable(AcukotoDspFuzz Tests/DspFuzz.cpp ${ACUKOTO_DSP_SOURCES})
        target_include_directories(AcukotoDspFuzz PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
        target_compile_options(AcukotoDspFuzz PRIVATE
            -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(AcukotoDspFuzz PRIVATE -fsanitize=fuzzer,address,undefined)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus" "${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts")
        add_test(NAME Acukoto.DspFuzz
                 COMMAND AcukotoDspFuzz -max_total_time=120 -seed=1 -max_len=4096 -rss_limit_mb=4096
                         "-artifact_prefix=${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts/"
                         "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus")
        set_tests_properties(Acukoto.DspFuzz PROPERTIES LABELS fuzz TIMEOUT 600)
    endif()

    if(Python3_Interpreter_FOUND)
        # The committed demo phrases are what the writer writes (G3a).
        add_test(NAME Acukoto.DemoMidiSources
                 COMMAND "${Python3_EXECUTABLE}" -B
                         "${CMAKE_CURRENT_SOURCE_DIR}/Tools/write_preset_midi.py" --check)
    endif()

    if(ACUKOTO_BUILD_TOOLS)
        # Every preset demo renders finite, audible, under the ceiling, 8-12 s
        # long and bit-identical on a second render.
        add_test(NAME Acukoto.AudioDemos
                 COMMAND RenderAcukotoPresetDemos --jobs 2 --check "${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/midi")
        set_tests_properties(Acukoto.AudioDemos PROPERTIES LABELS audio TIMEOUT 900)
        # Every preset's strings sound within 0.5 dB of one another, K-weighted.
        add_test(NAME Acukoto.Levels COMMAND AcukotoLevels --check)
        set_tests_properties(Acukoto.Levels PROPERTIES LABELS audio TIMEOUT 600)
        if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT ACUKOTO_ENABLE_SANITIZERS)
            add_test(NAME Acukoto.CpuBudget
                     COMMAND RenderAcukotoPresetDemos --cpu --json "${CMAKE_BINARY_DIR}/cpu.json"
                             "${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/midi")
            set_tests_properties(Acukoto.CpuBudget PROPERTIES LABELS perf RUN_SERIAL TRUE TIMEOUT 900)
        endif()
    endif()

    # Every test carries exactly one tier label so CI selects tiers with
    # `ctest -L`; tests not labelled above are unit contracts.
    # This focused contract includes the private implementation and must not
    # link AcukotoDSP again. It qualifies SIMD against the retained scalar loop.
    add_executable(AcukotoLoopPairTests Tests/LoopPairTests.cpp)
    target_include_directories(AcukotoLoopPairTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    add_test(NAME Acukoto.LoopPair COMMAND AcukotoLoopPairTests)

    add_executable(AcukotoModalRadiationTests Tests/ModalRadiationTests.cpp)
    target_include_directories(AcukotoModalRadiationTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    add_test(NAME Acukoto.ModalRadiation COMMAND AcukotoModalRadiationTests)

    get_property(ACUKOTO_TESTS DIRECTORY PROPERTY TESTS)
    foreach(test IN LISTS ACUKOTO_TESTS)
        get_test_property(${test} LABELS test_labels)
        if(NOT test_labels)
            set_tests_properties(${test} PROPERTIES LABELS unit)
        endif()
    endforeach()
endif()
