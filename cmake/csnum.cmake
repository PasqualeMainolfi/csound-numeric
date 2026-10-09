if(BUILD_CSNUM_OPCODES)
    if(DEFINED APIVERSION AND NOT APIVERSION MATCHES "^7\\.0$")
        message(STATUS "csnum: skipped (requires Csound API 7, got APIVERSION=${APIVERSION}; configure with -DAPIVERSION=7.0)")
        return()
    endif()

    set(CSNUM_SOURCES
        src/csnum.c
        src/csncreate.c
        src/csnshape.c
        src/csnindex.c
        src/csnselect.c
        src/csnstats.c
        src/csnmath.c
        src/csnoperations.c
        src/csnvector.c
        src/csnsignal.c
        src/csninterp.c
        src/csnio.c
        src/csnaudio.c
        src/csnfilter.c
        src/csnregistry.c
        src/csnfile.c
        src/csnset.c
        src/csnfft.c
        src/csnlinalg.c
        src/csnmeasure.c
        src/csncoord.c
        src/csnspecial.c
        src/dr_libs.c)

    make_plugin(csnum "${CSNUM_SOURCES}")

    # dr_libs (dr_wav, dr_mp3, dr_flac), vendored single-header audio file
    # readers: src/dr_libs.c compiles their implementations, the other files
    # include the headers. Both are third-party code, so the directory is a
    # system include and that file is built without warnings, which keeps a
    # -Wall -Wextra -Wconversion build about csnum's own sources. Its symbols
    # are hidden: csnum uses them, but does not export them to clash with
    # another plugin that bundles its own copy.
    target_include_directories(csnum SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/dr_libs)
    if(MSVC)
        set_source_files_properties(src/dr_libs.c PROPERTIES COMPILE_OPTIONS "/w")
    else()
        set_source_files_properties(src/dr_libs.c PROPERTIES COMPILE_OPTIONS "-w;-fvisibility=hidden")
    endif()

    if(WIN32)
        target_link_libraries(csnum PRIVATE ws2_32)
    endif()

    # The ring buffer uses <stdatomic.h>. MSVC keeps C11 atomics behind an opt-in
    # flag; without it the header hard-errors with
    # "C atomic support is not enabled". The flag needs /std:c11, so the C
    # standard must also be required rather than merely preferred.
    if(MSVC)
        set_property(TARGET csnum PROPERTY C_STANDARD_REQUIRED ON)
    endif()

    # rpath so the bundled sibling libraries (below) resolve from the plugin's own dir
    if(APPLE)
        set_target_properties(csnum PROPERTIES BUILD_RPATH "@loader_path" INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        set_target_properties(csnum PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
    endif()

endif()
