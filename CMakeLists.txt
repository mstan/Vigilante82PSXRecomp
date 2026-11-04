cmake_minimum_required(VERSION 3.20)
project(Vigilante82PSXRecomp C CXX)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/release/VERSION" V82_RELEASE_VERSION LIMIT_COUNT 1)
include(CTest)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp-v4" CACHE PATH "psxrecomp source")
set(PSX_NETPLAY ON CACHE BOOL "Enable two-player V8:2 netplay" FORCE)
set(PSX_LAUNCHER_MOD_COMMIT_WORKER_SAFE ON CACHE BOOL "V82 media preparers support serialized host-only worker calls" FORCE)
include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LauncherArtwork.cmake")
if(NOT PSXRECOMP_HAS_RECOMP_NET)
    message(FATAL_ERROR "V8:2 netplay requires recomp-net; initialize the framework submodule or set RECOMP_NET_ROOT.")
endif()
file(GLOB GAME_C CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/SLUS_008.68_full_[0-9][0-9].c")
if(NOT GAME_C)
    set(GAME_C "${CMAKE_CURRENT_SOURCE_DIR}/generated/SLUS_008.68_full.c")
endif()
file(GLOB TITLE_PLUGINS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/*.cpp")
set(V82_EXCLUDED_BUILTINS psx.enhancement.cd-speed psx.enhancement.fast-loading)
if(PSX_PUBLIC_RELEASE)
    list(APPEND V82_EXCLUDED_BUILTINS psx.enhancement.hd-textures)
endif()
# PGXP: the one runtime is the PGXP hook flavour (-DPSX_PGXP=1, overlay
# flavour 2), so the default-on psx.enhancement.pgxp package reaches dataflow
# coverage. The generated C already carries the PGXP_*() hook sites.
psxrecomp_add_runtime_target(psx-runtime
    PGXP
    GAME_GENERATED_FULL_C "${GAME_C}"
    GAME_GENERATED_DISPATCH_C "${CMAKE_CURRENT_SOURCE_DIR}/generated/SLUS_008.68_dispatch.c"
    DEBUG_PORT 4545
    MAX_PLAYERS 2
    GAME_VERSION "${V82_RELEASE_VERSION}"
    WINDOW_TITLE "Vigilante 8: 2nd Offense Recompiled"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    LAUNCHER_BOXART "${V82_LAUNCHER_BOXART}"
    EXCLUDE_BUILTIN_MODS ${V82_EXCLUDED_BUILTINS}
    PRELOADED_MODS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded"
    EXTRAS_SOURCES ${TITLE_PLUGINS})
set_target_properties(psx-runtime PROPERTIES OUTPUT_NAME "Vigilante82PSXRecomp")
include("${PSXRECOMP_ROOT}/runtime/playtest_diagnostics.cmake")
psxrecomp_playtest_diagnostics_defaults(psx-runtime MOD_ID v82.diagnostics)

# Playtest diagnostics (default on outside public releases; source is
# globbed into TITLE_PLUGINS). The build identity is part of every recording
# header, so a replay refuses a different build instead of reporting a false
# divergence.
set(V82_SOURCE_REV "nogit" CACHE STRING "Source revision stamped into the diagnostic build id (auto-detected from git when empty/nogit)")
if(PSX_SOURCE_COMMIT)
    string(SUBSTRING "${PSX_SOURCE_COMMIT}" 0 12 V82_SOURCE_REV)
endif()
find_package(Git QUIET)
if(GIT_FOUND AND V82_SOURCE_REV STREQUAL "nogit")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _V82_REV OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _V82_REV_RESULT)
    if(_V82_REV_RESULT EQUAL 0 AND _V82_REV)
        set(V82_SOURCE_REV "${_V82_REV}")
    endif()
endif()
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/v82_diagnostics.cpp" APPEND PROPERTY
    COMPILE_DEFINITIONS "V82_DIAGNOSTIC_BUILD_ID=\"${V82_RELEASE_VERSION}+${V82_SOURCE_REV}\"")
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    foreach(enhancement widescreen music unsplit)
        add_executable(v82-${enhancement}-test tests/test_${enhancement}.cpp)
        target_include_directories(v82-${enhancement}-test PRIVATE "${PSXRECOMP_ROOT}/runtime/include")
        add_test(NAME v82-${enhancement} COMMAND v82-${enhancement}-test)
    endforeach()
    add_test(NAME v82-music-import COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_music_import.py")
    add_test(NAME v82-media-revision COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_media_revision.py")
    add_executable(v82-media-receipt-test tests/test_media_receipt.cpp
        "${PSXRECOMP_ROOT}/runtime/src/cue_sheet.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/disc_path.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/iso_reader.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/mod_media.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/psx_sha256.c")
    target_include_directories(v82-media-receipt-test PRIVATE
        "${PSXRECOMP_ROOT}/runtime/include" "${PSXRECOMP_ROOT}/recompiler/include"
        "${PSXRECOMP_ROOT}/recompiler/lib/toml11")
    target_link_libraries(v82-media-receipt-test PRIVATE chdr-static)
    add_test(NAME v82-media-receipt COMMAND v82-media-receipt-test)
    # Host-only diagnostics tests (no disc, BIOS or window).
    find_package(Threads REQUIRED)
    add_executable(v82-diagnostic-writer-test tests/test_v82_diagnostic_writer.cpp)
    add_executable(v82-diagnostics-test tests/test_v82_diagnostics.cpp)
    foreach(_T v82-diagnostic-writer-test v82-diagnostics-test)
        target_include_directories(${_T} PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
        target_link_libraries(${_T} PRIVATE Threads::Threads)
    endforeach()
    add_test(NAME v82-diagnostic-writer COMMAND v82-diagnostic-writer-test
        "${CMAKE_CURRENT_BINARY_DIR}/diagnostic-writer-qa")
    set_tests_properties(v82-diagnostic-writer PROPERTIES TIMEOUT 15)
    add_test(NAME v82-diagnostics COMMAND v82-diagnostics-test "${CMAKE_CURRENT_BINARY_DIR}/diagnostics-qa")
    add_test(NAME v82-diagnostic-replay-prepare
        COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_replay_v82_diagnostics.py")
    add_test(NAME v82-animation-timing COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_animation_timing.py")
    add_executable(v82-lightning-test tests/test_lightning.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-lightning-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-lightning COMMAND v82-lightning-test)
    add_executable(v82-special-roster-test tests/test_special_roster.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-special-roster-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-special-roster COMMAND v82-special-roster-test)
    add_executable(v82-presentation-test tests/test_presentation.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-presentation-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-presentation COMMAND v82-presentation-test)
    add_test(NAME v82-presentation-import COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_presentation_import.py")
    add_executable(v82-draw-packets-test tests/test_draw_packets.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-draw-packets-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-draw-packets COMMAND v82-draw-packets-test)
    add_executable(v82-native-selector-test tests/test_native_selector.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-native-selector-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-native-selector COMMAND v82-native-selector-test)
    add_test(NAME v82-dreamland-audio COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dreamland_audio.py")
    add_test(NAME v82-dreamland-overlay COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dreamland_overlay.py")
    add_executable(v82-dreamland-runtime-test tests/test_dreamland_runtime.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-dreamland-runtime-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-dreamland-runtime COMMAND v82-dreamland-runtime-test)
    add_executable(v82-flying-controller-test tests/test_flying_controller.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-flying-controller-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-flying-controller COMMAND v82-flying-controller-test)
    add_executable(v82-original-specials-test tests/test_original_specials.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-original-specials-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-original-specials COMMAND v82-original-specials-test)
    add_executable(v82-guest-heap-test tests/test_guest_heap.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-guest-heap-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-guest-heap COMMAND v82-guest-heap-test)
    add_executable(v82-transformations-test tests/test_transformations.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-transformations-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-transformations COMMAND v82-transformations-test)
    add_executable(v82-vehicle-package-test tests/test_vehicle_package.cpp src/mods/v82_vehicle_package.cpp)
    target_include_directories(v82-vehicle-package-test PRIVATE src/mods)
    add_test(NAME v82-vehicle-package COMMAND v82-vehicle-package-test)
    set_tests_properties(v82-vehicle-package PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(v82-resident-test tests/test_resident.cpp)
    target_include_directories(v82-resident-test PRIVATE src/mods "${PSXRECOMP_ROOT}/runtime/include")
    add_test(NAME v82-resident COMMAND v82-resident-test)
    add_test(NAME v82-resident-catalog COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_resident_catalog.py")
    set_tests_properties(v82-resident-catalog PROPERTIES SKIP_RETURN_CODE 77
        ENVIRONMENT "V82_FRAMEWORK=${PSXRECOMP_ROOT}")
endif()

# The release folder includes the frozen conversion worker and encoder. Players
# choose disc images/ROMs; they never install Python or run preparation scripts.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(V82_PSXAVENC NAMES psxavenc
    HINTS "$ENV{LOCALAPPDATA}/psxrecomp/tools/psxavenc-0.3.1/bin"
    DOC "psxavenc 0.3.1 used by the bundled media importer")
if(NOT V82_PSXAVENC)
    message(FATAL_ERROR "Set V82_PSXAVENC to psxavenc 0.3.1 to package the media importer.")
endif()
file(GLOB_RECURSE V82_MEDIA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.py")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/media-tools/.ready"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_media_helper.py"
        --output "${CMAKE_CURRENT_BINARY_DIR}/media-tools" --encoder "${V82_PSXAVENC}"
    DEPENDS ${V82_MEDIA_SOURCES} "${V82_PSXAVENC}"
    COMMENT "Packaging automatic owner-media importer" VERBATIM)
add_custom_target(v82-media-tools ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/media-tools/.ready")
add_dependencies(psx-runtime v82-media-tools)
