cmake_minimum_required(VERSION 3.22)

project(Navalha2Juce VERSION 0.1.0 LANGUAGES C CXX)
find_package(Threads REQUIRED)

option(NAVALHA_BUILD_JUCE_APP "Build the JUCE standalone migration shell" ON)
option(NAVALHA_BUILD_TESTS "Build migration contract tests" ON)
option(NAVALHA_BUILD_TOOLS "Build migration validation tools" ON)
option(NAVALHA_ENABLE_WEBVIEW "Enable JUCE WebBrowserComponent integration" OFF)
set(NAVALHA_JUCE_PATH "" CACHE PATH "Path to a JUCE source checkout")
set(NAVALHA_PD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../NAVALHA2_PD" CACHE PATH
    "Path to the sibling Navalha 2 Pure Data/web repository")

add_library(navalha_core STATIC
    src/core/AlbumProject.cpp
    src/core/AssistedRng.cpp
    src/core/AssistedPerformer.cpp
    src/core/AudioComparison.cpp
    src/core/AudioEngine.cpp
    src/core/ControlTrace.cpp
    src/core/FragmentGesture.cpp
    src/core/FormDirector.cpp
    src/core/HeritagePitch.cpp
    src/core/Json.cpp
    src/core/LegacyFormat.cpp
    src/core/LookaheadLimiter.cpp
    src/core/MasteringAnalysis.cpp
    src/core/MasteringAlbum.cpp
    src/core/MasteringAlbumManifest.cpp
    src/core/MasteringProcessor.cpp
    src/core/MasteringRecipe.cpp
    src/core/OfflineRenderer.cpp
    src/core/OutputStage.cpp
    src/core/PatternTransform.cpp
    src/core/PortablePath.cpp
    src/core/PortableArchive.cpp
    src/core/PortableProject.cpp
    src/core/ProjectState.cpp
    src/core/ProjectJson.cpp
    src/core/RecordingWriterService.cpp
    src/core/Sequencer.cpp
    src/core/SessionModel.cpp
    src/core/SlicePlayer.cpp
    src/core/StereoMixer.cpp
    src/core/TakeCatalog.cpp
    src/core/TruePeakDetector.cpp
    src/core/WavStreamWriter.cpp
    src/core/WavMemoryReader.cpp
    src/core/WavMetadataRewriter.cpp
    src/core/WaveformPeaks.cpp
)

target_include_directories(navalha_core PUBLIC src)
target_compile_features(navalha_core PUBLIC cxx_std_20)
target_link_libraries(navalha_core PUBLIC Threads::Threads)
target_compile_definitions(
    navalha_core PRIVATE NAVALHA_JUCE_VERSION="${PROJECT_VERSION}")

if (NAVALHA_BUILD_TESTS OR NAVALHA_BUILD_TOOLS)
    add_library(navalha_validation STATIC
        src/validation/TruePeakFixtures.cpp)
    target_include_directories(navalha_validation PUBLIC src)
    target_compile_features(navalha_validation PUBLIC cxx_std_20)
    if (MSVC)
        target_compile_options(navalha_validation PRIVATE /W4 /permissive-)
    else()
        target_compile_options(
            navalha_validation PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

if (MSVC)
    target_compile_options(navalha_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(navalha_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if (NAVALHA_BUILD_TESTS)
    enable_testing()
    add_executable(navalha_core_tests tests/SessionModelTests.cpp)
    target_link_libraries(
        navalha_core_tests PRIVATE navalha_core navalha_validation)
    add_test(NAME navalha_core_tests COMMAND navalha_core_tests)
    add_executable(navalha_golden_render_tests tests/GoldenRenderTests.cpp)
    target_link_libraries(navalha_golden_render_tests PRIVATE navalha_core)
    add_test(NAME navalha_golden_render_tests COMMAND navalha_golden_render_tests)
    add_executable(navalha_engine_stress_tests tests/EngineStressTests.cpp)
    target_link_libraries(navalha_engine_stress_tests PRIVATE navalha_core)
    add_test(NAME navalha_engine_stress_tests COMMAND navalha_engine_stress_tests)
    add_executable(navalha_recording_soak_tests tests/RecordingSoakTests.cpp)
    target_link_libraries(navalha_recording_soak_tests PRIVATE navalha_core)
    add_test(NAME navalha_recording_soak_tests COMMAND navalha_recording_soak_tests)
endif()

if (NAVALHA_BUILD_TOOLS)
    add_executable(navalha_compare_wav src/tools/CompareWav.cpp)
    target_link_libraries(navalha_compare_wav PRIVATE navalha_core)
    add_executable(navalha_render_portable src/tools/RenderPortable.cpp)
    target_link_libraries(navalha_render_portable PRIVATE navalha_core)
    add_executable(navalha_render_pitch_fixture src/tools/RenderPitchFixture.cpp)
    target_link_libraries(navalha_render_pitch_fixture PRIVATE navalha_core)
    add_executable(navalha_analyze_master src/tools/AnalyzeMaster.cpp)
    target_link_libraries(navalha_analyze_master PRIVATE navalha_core)
    add_executable(navalha_render_master src/tools/RenderMaster.cpp)
    target_link_libraries(navalha_render_master PRIVATE navalha_core)
    add_executable(navalha_inspect_album src/tools/InspectAlbum.cpp)
    target_link_libraries(navalha_inspect_album PRIVATE navalha_core)
    add_executable(navalha_render_album src/tools/RenderAlbum.cpp)
    target_link_libraries(navalha_render_album PRIVATE navalha_core)
    add_executable(navalha_inspect_project src/tools/InspectProject.cpp)
    target_link_libraries(navalha_inspect_project PRIVATE navalha_core)
    add_executable(
        navalha_render_true_peak_fixtures
        src/tools/RenderTruePeakFixtures.cpp)
    target_link_libraries(
        navalha_render_true_peak_fixtures
        PRIVATE navalha_core navalha_validation)
    if (NAVALHA_BUILD_TESTS)
        find_program(NAVALHA_FFMPEG_EXECUTABLE ffmpeg)
        if (NAVALHA_FFMPEG_EXECUTABLE)
            add_test(
                NAME navalha_true_peak_ffmpeg
                COMMAND ${CMAKE_COMMAND}
                    -DNAVALHA_TRUE_PEAK_TOOL=$<TARGET_FILE:navalha_render_true_peak_fixtures>
                    -DNAVALHA_FFMPEG=${NAVALHA_FFMPEG_EXECUTABLE}
                    -DNAVALHA_TRUE_PEAK_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/true-peak-validation
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/validation/ValidateTruePeakFfmpeg.cmake)
        endif()
        add_test(
            NAME navalha_inspect_project_v1
            COMMAND navalha_inspect_project
                    ${CMAKE_CURRENT_SOURCE_DIR}/validation/project_v1_reference.navalha)
        add_test(
            NAME navalha_inspect_project_v2
            COMMAND navalha_inspect_project
                    ${CMAKE_CURRENT_SOURCE_DIR}/validation/project_v2_reference.navalha)
        add_test(
            NAME navalha_reject_project_corrupt
            COMMAND navalha_inspect_project
                    ${CMAKE_CURRENT_SOURCE_DIR}/validation/project_corrupt.navalha)
        set_tests_properties(navalha_reject_project_corrupt
                             PROPERTIES WILL_FAIL TRUE)
        add_test(
            NAME navalha_reject_project_future
            COMMAND navalha_inspect_project
                    ${CMAKE_CURRENT_SOURCE_DIR}/validation/project_future.navalha)
        set_tests_properties(navalha_reject_project_future
                             PROPERTIES WILL_FAIL TRUE)
    endif()
endif()

if (NAVALHA_BUILD_JUCE_APP)
    if (NAVALHA_JUCE_PATH)
        if (EXISTS "${NAVALHA_JUCE_PATH}/CMakeLists.txt")
            add_subdirectory("${NAVALHA_JUCE_PATH}"
                             "${CMAKE_CURRENT_BINARY_DIR}/JUCE"
                             EXCLUDE_FROM_ALL)
            set(JUCE_FOUND TRUE)
        else()
            message(FATAL_ERROR
                "NAVALHA_JUCE_PATH does not contain a JUCE CMakeLists.txt: "
                "${NAVALHA_JUCE_PATH}")
        endif()
    else()
        find_package(JUCE CONFIG QUIET)
    endif()
    if (JUCE_FOUND)
        add_subdirectory(src/app)
    else()
        message(WARNING
            "JUCE was not found. The dependency is intentionally not vendored. "
            "Set JUCE_DIR or CMAKE_PREFIX_PATH after choosing the JUCE licence.")
    endif()
endif()

if (NAVALHA_BUILD_JUCE_APP AND JUCE_FOUND)
    set(CPACK_PACKAGE_NAME "navalha2")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VENDOR "Navalha 2")
    set(CPACK_PACKAGE_CONTACT "Navalha 2 <rasgo.instruments@gmail.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Navalha 2 — live sample slicing and performance instrument")

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(CPACK_GENERATOR "DEB")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Navalha 2 <rasgo.instruments@gmail.com>")
        set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(CPACK_PACKAGE_FILE_NAME
            "navalha2_${PROJECT_VERSION}_${CMAKE_SYSTEM_PROCESSOR}")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # NSIS: ships preinstalled on GitHub's windows-latest runner (via
        # Chocolatey in CI), no extra toolchain needed beyond that.
        set(CPACK_GENERATOR "NSIS")
        set(CPACK_NSIS_PACKAGE_NAME "Navalha 2")
        set(CPACK_NSIS_DISPLAY_NAME "Navalha 2")
        set(CPACK_NSIS_CONTACT "rasgo.instruments@gmail.com")
        set(CPACK_NSIS_MODIFY_PATH OFF)
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # DragNDrop (.dmg): no extra tooling beyond macOS's own hdiutil.
        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_DMG_VOLUME_NAME "Navalha 2")
    endif()

    include(CPack)
endif()
