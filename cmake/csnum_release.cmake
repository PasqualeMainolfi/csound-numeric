if(NOT BUILD_csnum_OPCODES OR NOT BUILD_csnum_VIEWER)
    return()
endif()

if(WIN32)
    set(csnum_RELEASE_PLATFORM "windows-x86_64")
elseif(APPLE)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" AND
       CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        set(csnum_RELEASE_PLATFORM "macos-universal")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        set(csnum_RELEASE_PLATFORM "macos-arm64")
    else()
        set(csnum_RELEASE_PLATFORM "macos-x86_64")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
       CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(csnum_RELEASE_PLATFORM "linux-x86_64")
else()
    set(csnum_RELEASE_PLATFORM
        "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(csnum_RELEASE_NAME "csnum-csound7-${csnum_RELEASE_PLATFORM}.zip")
set(csnum_RELEASE_DIR "${CMAKE_BINARY_DIR}/dist")
set(csnum_RELEASE_STAGE "${csnum_RELEASE_DIR}/csnum-release")

add_custom_target(csnum_release_package
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${csnum_RELEASE_STAGE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${csnum_RELEASE_STAGE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${csnum_RELEASE_STAGE}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:csnum>" "${csnum_RELEASE_STAGE}/"
    COMMAND "${CMAKE_COMMAND}" -E chdir "${csnum_RELEASE_STAGE}"
            "${CMAKE_COMMAND}" -E tar cf
            "${csnum_RELEASE_DIR}/${csnum_RELEASE_NAME}"
            --format=zip
            "$<TARGET_FILE_NAME:csnum>"
            licenses
    COMMENT "Packaging ${csnum_RELEASE_NAME} for Risset"
    VERBATIM)
