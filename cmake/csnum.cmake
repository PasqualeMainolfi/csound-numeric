if(BUILD_CSNUM_OPCODES)
    if(DEFINED APIVERSION AND NOT APIVERSION MATCHES "^7\\.0$")
        message(STATUS "csnum: skipped (requires Csound API 7, got APIVERSION=${APIVERSION}; configure with -DAPIVERSION=7.0)")
        return()
    endif()

    set(CSNUM_SOURCES
        src/csnum.c
        src/csnregistry.c)

    make_plugin(csnum "${CSNUM_SOURCES}")

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
