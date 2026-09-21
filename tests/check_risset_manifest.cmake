# The release workflow validates risset.json only once a tag has been pushed,
# which is the most expensive moment to learn that a field was left behind. The
# same checks run here, against the manifest's own version rather than a tag, so
# a stale field fails in ctest instead.
#
# Plain regex rather than string(JSON ...), which needs CMake 3.19; this project
# only asks for 3.16.

if(NOT DEFINED MANIFEST_FILE OR NOT DEFINED SOURCE_FILE)
    message(FATAL_ERROR "MANIFEST_FILE and SOURCE_FILE are required")
endif()

file(READ "${MANIFEST_FILE}" manifest)

if(NOT manifest MATCHES "\"version\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\"")
    message(FATAL_ERROR "risset.json has no well-formed version")
endif()
set(version "${CMAKE_MATCH_1}")

# Each platform, the archive it is served from and the file inside it. Kept in
# step with the expected table in .github/workflows/release.yml.
set(platforms   linux-x86_64     macos-arm64        macos-x86_64       windows-x86_64)
set(archives    linux-x86_64     macos-universal    macos-universal    windows-x86_64)
set(extractpaths libcsnum.so     libcsnum.dylib     libcsnum.dylib     csnum.dll)

list(LENGTH platforms count)
math(EXPR last "${count} - 1")
foreach(i RANGE ${last})
    list(GET platforms ${i} platform)
    list(GET archives ${i} archive)
    list(GET extractpaths ${i} extractpath)

    # the object for this platform, from its name to the closing brace
    if(NOT manifest MATCHES "\"platform\"[ \t]*:[ \t]*\"${platform}\"([^}]*)}")
        message(FATAL_ERROR "risset.json has no entry for ${platform}")
    endif()
    set(entry "${CMAKE_MATCH_1}")

    set(expected_url "/releases/download/v${version}/csnum-csound7-${archive}.zip")
    if(NOT entry MATCHES "\"url\"[ \t]*:[ \t]*\"([^\"]*)\"")
        message(FATAL_ERROR "risset.json: ${platform} has no url")
    endif()
    set(url "${CMAKE_MATCH_1}")
    if(NOT url MATCHES "${expected_url}$")
        message(FATAL_ERROR
            "risset.json binary does not match ${platform} asset;\n"
            "  version is ${version}, so the url must end in\n"
            "    ${expected_url}\n"
            "  but it is\n"
            "    ${url}")
    endif()

    if(NOT entry MATCHES "\"extractpath\"[ \t]*:[ \t]*\"${extractpath}\"")
        message(FATAL_ERROR "risset.json: ${platform} extractpath is not ${extractpath}")
    endif()
endforeach()

# The opcode list must name every opcode the OENTRY table registers, under its
# plain name, and nothing else.
string(REGEX MATCH "\"opcodes\"[ \t]*:[ \t]*\\[([^]]*)\\]" _ "${manifest}")
string(REGEX MATCHALL "\"(csn[a-z0-9_]+)\"" listed_quoted "${CMAKE_MATCH_1}")
set(listed "")
foreach(entry IN LISTS listed_quoted)
    string(REGEX REPLACE "\"" "" entry "${entry}")
    if(entry IN_LIST listed)
        message(FATAL_ERROR "risset.json lists ${entry} more than once")
    endif()
    list(APPEND listed "${entry}")
endforeach()

file(STRINGS "${SOURCE_FILE}" source_lines)
set(registered "")
foreach(line IN LISTS source_lines)
    if(line MATCHES "^[ \t]*\\{[ \t]*\"(csn[a-z0-9_]+)[a-z0-9_.]*\"[ \t]*,[ \t]*S\\(")
        list(APPEND registered "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(REMOVE_DUPLICATES registered)

set(missing "${registered}")
foreach(name IN LISTS listed)
    list(REMOVE_ITEM missing "${name}")
endforeach()
set(stale "${listed}")
foreach(name IN LISTS registered)
    list(REMOVE_ITEM stale "${name}")
endforeach()
if(missing OR stale)
    message(FATAL_ERROR
        "risset.json opcode list is out of step with the OENTRY table;\n"
        "  registered but not listed: ${missing}\n"
        "  listed but not registered: ${stale}")
endif()
