# Standalone build for the csnum Csound plugin.
#
#


cmake_minimum_required(VERSION 3.16)
project(csound_numeric C)

include(CTest)

# csnum is Csound API 7 only.
set(APIVERSION "7.0" CACHE STRING "Csound API version (numeric requires 7.0)")

option(BUILD_CSNUM_OPCODES "Build csnum opcodes (Csound API 7)" ON)
option(csnum_USE_SYSTEM_SDL3 "Use an installed SDL3 instead of third_party/SDL" OFF)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX YES)
else()
    set(LINUX NO)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

option(USE_DOUBLE "Use doubles for audio calculations" ON)

if(BUILD_CSNUM_OPCODES)
find_path(CSOUND_INCLUDE_DIR
    NAMES csdl.h
    HINTS ENV CSOUND_INCLUDE_DIR ENV CSOUND_HOME
    PATHS
        /usr/local/include/csound
        /usr/include/csound
        /opt/homebrew/include/csound
        $ENV{HOME}/Library/Frameworks/CsoundLib64.framework/Headers
        /Library/Frameworks/CsoundLib64.framework/Headers
        /Applications/Csound/CsoundLib64.framework/Headers
    PATH_SUFFIXES include include/csound)

if(NOT CSOUND_INCLUDE_DIR)
    message(FATAL_ERROR
        "Csound plugin headers (csdl.h) not found. "
        "Pass -DCSOUND_INCLUDE_DIR=/path/to/csound/include "
        "(a Csound source checkout's include/ directory).")
endif()
message(STATUS "csnum: Csound headers at ${CSOUND_INCLUDE_DIR}")

set(CSOUND_EXTRA_INCLUDE_DIRS "" CACHE STRING
    "Extra Csound include dirs (semicolon-separated), e.g. the dir holding a generated version.h")
if(NOT EXISTS "${CSOUND_INCLUDE_DIR}/version.h")
    find_file(CSOUND_VERSION_H version.h
        PATHS "${CSOUND_INCLUDE_DIR}/.."
        PATH_SUFFIXES Android/CsoundAndroid/jni
        NO_DEFAULT_PATH)
    if(CSOUND_VERSION_H)
        get_filename_component(_vdir "${CSOUND_VERSION_H}" DIRECTORY)
        list(APPEND CSOUND_EXTRA_INCLUDE_DIRS "${_vdir}")
        message(STATUS "csnum: generated version.h at ${_vdir}")
    elseif(NOT CSOUND_EXTRA_INCLUDE_DIRS)
        message(WARNING
            "Csound version.h not found next to csdl.h. When building against a "
            "Csound source checkout, pass -DCSOUND_EXTRA_INCLUDE_DIRS=/dir/with/version.h")
    endif()
endif()

# Csound 7 changed CS_CREATE_VARIABLE_FUNC from (cs, typeArg, ctx) to
# (cs, type, typeArg, ctx). Both signatures are still in the wild, so pick the
# one the headers in use declare instead of guessing.
include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${CSOUND_INCLUDE_DIR} ${CSOUND_EXTRA_INCLUDE_DIRS})
set(CMAKE_REQUIRED_DEFINITIONS -DCSOUNDAPI7 -DCS_API_VERSION_MAJOR=7)
if(USE_DOUBLE)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DUSE_DOUBLE)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_REQUIRED_FLAGS "-Werror=incompatible-pointer-types")
endif()
check_c_source_compiles("
#include <csdl.h>
static struct csvariable *probe(void *cs, const struct cstype *type,
                                const void *typeArg, struct insds *ctx) {
    (void) cs; (void) type; (void) typeArg; (void) ctx; return NULL;
}
int main(void) { CS_CREATE_VARIABLE_FUNC f = probe; (void) f; return 0; }
" CSNUM_CS_TYPE_ARG_4ARG)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_DEFINITIONS)
unset(CMAKE_REQUIRED_FLAGS)

# --- plugin install dir (matches csound-plugins layout) --------------------
if(APPLE)
    set(PLUGIN_INSTALL_DIR "$ENV{HOME}/Library/csound/${APIVERSION}/plugins64")
elseif(LINUX)
    set(PLUGIN_INSTALL_DIR "$ENV{HOME}/.local/lib/csound/${APIVERSION}/plugins64")
elseif(WIN32)
    set(PLUGIN_INSTALL_DIR "$ENV{LOCALAPPDATA}/csound/${APIVERSION}/plugins64")
endif()

set(BUILD_PLUGINS_DIR ${CMAKE_CURRENT_BINARY_DIR})
function(make_plugin pluginlib srcs)
    message(STATUS "make_plugin: ${pluginlib}, sources: ${srcs}")
    if(APPLE)
        add_library(${pluginlib} SHARED ${srcs})
    else()
        add_library(${pluginlib} MODULE ${srcs})
    endif()
    target_include_directories(${pluginlib} PRIVATE ${CSOUND_INCLUDE_DIR} ${CSOUND_EXTRA_INCLUDE_DIRS})
    target_compile_definitions(${pluginlib} PRIVATE CSOUNDAPI7 CS_API_VERSION_MAJOR=7)
    if(CSNUM_CS_TYPE_ARG_4ARG)
        target_compile_definitions(${pluginlib} PRIVATE CS_TYPE_ARG_4ARG)
    endif()
    if(USE_DOUBLE)
        target_compile_definitions(${pluginlib} PRIVATE USE_DOUBLE)
    endif()
    # dladdr()/Dl_info are behind _GNU_SOURCE on glibc (available by default on macOS/BSD).
    target_compile_definitions(${pluginlib} PRIVATE $<$<PLATFORM_ID:Linux>:_GNU_SOURCE>)
    set_property(TARGET ${pluginlib} PROPERTY C_STANDARD 11)
    set_target_properties(${pluginlib} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BUILD_PLUGINS_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${BUILD_PLUGINS_DIR})
    install(TARGETS ${pluginlib} LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}")
endfunction()

# --- the plugin itself (former in-tree fragment) ---------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/csnum.cmake)

if(BUILD_TESTING)
    add_test(
        NAME csnum_itime_signature_coverage
        COMMAND "${CMAKE_COMMAND}"
            -DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/src/csnum.c
            -DCSD_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/itime_regressions.csd
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_itime_signatures.cmake)

    find_program(CSOUND_EXECUTABLE NAMES csound)
    if(CSOUND_EXECUTABLE)
        add_test(
            NAME csnum_printing_regression
            COMMAND "${CMAKE_COMMAND}"
                -DCSOUND_EXECUTABLE=${CSOUND_EXECUTABLE}
                -DOPCODE_DIR=$<TARGET_FILE_DIR:csnum>
                -DCSD_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/printing_regression.csd
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_printing.cmake)
        add_test(
            NAME csnum_itime_regressions
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/itime_regressions.csd")
        add_test(
            NAME csnum_krate_empty_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_empty_regression.csd")
        add_test(
            NAME csnum_krate_creation_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_creation_regression.csd")
        add_test(
            NAME csnum_krate_reshape_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_reshape_regression.csd")
        add_test(
            NAME csnum_krate_roll_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_roll_regression.csd")
        add_test(
            NAME csnum_krate_transform_empty_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_transform_empty_regression.csd")
        add_test(
            NAME csnum_krate_insert_remove_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_insert_remove_regression.csd")
        add_test(
            NAME csnum_krate_concat_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_concat_regression.csd")
        add_test(
            NAME csnum_krate_pad_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_pad_regression.csd")
        add_test(
            NAME csnum_krate_vecmath_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_vecmath_regression.csd")
        add_test(
            NAME csnum_krate_binop_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_binop_regression.csd")
        add_test(
            NAME csnum_krate_self_alias_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_self_alias_regression.csd")
        add_test(
            NAME csnum_krate_self_alias_rejected
            COMMAND "${CSOUND_EXECUTABLE}"
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_self_alias_rejected.csd")
        set_tests_properties(csnum_krate_self_alias_rejected PROPERTIES
            PASS_REGULAR_EXPRESSION "is also this opcode's own output")
        add_test(
            NAME csnum_krate_self_alias_gradient_rejected
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_self_alias_gradient_rejected.csd")
        set_tests_properties(csnum_krate_self_alias_gradient_rejected PROPERTIES
            PASS_REGULAR_EXPRESSION "is also this opcode's own output")
        add_test(
            NAME csnum_krate_relayout_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_relayout_regression.csd")
        add_test(
            NAME csnum_krate_reduction_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_reduction_regression.csd")
        add_test(
            NAME csnum_krate_inplace_version_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_inplace_version_regression.csd")
        add_test(
            NAME csnum_krate_self_alias_cell_local
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_self_alias_cell_local.csd")
        add_test(
            NAME csnum_krate_optional_trig_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_optional_trig_regression.csd")
        add_test(
            NAME csnum_krate_version_cache_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_version_cache_regression.csd")
        add_test(
            NAME csnum_krate_file_reload_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_file_reload_regression.csd")
        add_test(
            NAME csnum_krate_truncate_resize_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_truncate_resize_regression.csd")
        add_test(
            NAME csnum_krate_interp_regression
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_interp_regression.csd")
        # Both failures are reported from under the registry mutex, so a wedged
        # lock shows up as a hung test rather than a wrong value: the marker is
        # only reached if the notes aborted and the registry stayed usable.
        add_test(
            NAME csnum_krate_interp_error_recovery
            COMMAND "${CSOUND_EXECUTABLE}"
                --run-unit-tests
                --opcode-dir=$<TARGET_FILE_DIR:csnum>
                -n -d
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/krate_interp_error_recovery.csd")
        set_tests_properties(csnum_krate_interp_error_recovery PROPERTIES
            PASS_REGULAR_EXPRESSION "csnum registry alive after the interp and resample failures"
            TIMEOUT 60)
    endif()
endif()
endif()

# --- dependency-free binary archive for GitHub Releases / Risset -----------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/csnum_release.cmake)
