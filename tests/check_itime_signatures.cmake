if(NOT DEFINED SOURCE_FILE OR NOT DEFINED CSD_FILE)
    message(FATAL_ERROR "SOURCE_FILE and CSD_FILE are required")
endif()

file(READ "${CSD_FILE}" csd_text)

file(STRINGS "${SOURCE_FILE}" source_lines)
set(registered "")
foreach(line IN LISTS source_lines)
    if(line MATCHES "^[ \t]*\\{[ \t]*\"(csn[a-z0-9_.]+)\"[ \t]*,[ \t]*S\\([^)]*\\)[ \t]*,[ \t]*[0-9]+[ \t]*,[ \t]*\"([^\"]*)\"[ \t]*,[ \t]*\"([^\"]*)\"")
        set(name "${CMAKE_MATCH_1}")
        set(output_signature "${CMAKE_MATCH_2}")
        set(input_signature "${CMAKE_MATCH_3}")

        # OENTRY suffixes are only human-readable reminders. Determine the
        # rate from the actual type signatures. k is a required k argument;
        # J, O, P and V are Csound's optional k arguments, differing only in
        # the value they default to (-1, 0, 1 and 0.5). An opcode whose trigger
        # is an optional P still runs at k-rate and does not belong in the
        # i-time inventory. The lowercase j/o/p/v are the i-rate counterparts
        # and stay out of this set.
        #
        # A lowercase a is an audio argument, which puts the opcode on the
        # performance path just as surely as a k does. No :CsnArr; or :Complex;
        # type name contains a lowercase a, so matching the bare letter picks
        # out the audio bridge and nothing else.
        if(output_signature MATCHES "[kJOPVa]" OR input_signature MATCHES "[kJOPVa]")
            continue()
        endif()
        list(APPEND registered "${name}")
    endif()
endforeach()
list(SORT registered)

string(FIND "${csd_text}" "; @covers-begin" covers_begin)
string(FIND "${csd_text}" "; @covers-end" covers_end)
if(covers_begin EQUAL -1 OR covers_end EQUAL -1 OR covers_end LESS covers_begin)
    message(FATAL_ERROR "Missing or invalid @covers inventory in ${CSD_FILE}")
endif()
math(EXPR covers_length "${covers_end} - ${covers_begin}")
string(SUBSTRING "${csd_text}" ${covers_begin} ${covers_length} covers_text)
string(REGEX MATCHALL "csn[a-z0-9_]+(\\.[a-z0-9_]+)*" covered "${covers_text}")

list(LENGTH covered covered_count)
set(covered_unique "${covered}")
list(REMOVE_DUPLICATES covered_unique)
list(LENGTH covered_unique covered_unique_count)
if(NOT covered_count EQUAL covered_unique_count)
    message(FATAL_ERROR "The @covers inventory contains duplicate OENTRY names")
endif()
list(SORT covered_unique)

if(NOT registered STREQUAL covered_unique)
    set(missing "${registered}")
    set(extra "${covered_unique}")
    foreach(name IN LISTS covered_unique)
        list(REMOVE_ITEM missing "${name}")
    endforeach()
    foreach(name IN LISTS registered)
        list(REMOVE_ITEM extra "${name}")
    endforeach()
    message(FATAL_ERROR "i-time signature inventory mismatch; missing=[${missing}], extra=[${extra}]")
endif()

# Dot suffixes are OENTRY dispatch reminders, never public opcode calls.
string(REGEX REPLACE ";[^\n]*" "" executable_csd "${csd_text}")
if(executable_csd MATCHES "csn[a-z0-9_]+\\.[a-z0-9_.]*[ \t]*\\(")
    message(FATAL_ERROR "Dotted csnum opcode call found in executable Csound code")
endif()

list(LENGTH registered registered_count)
message(STATUS "Verified ${registered_count} i-time OENTRY signatures")
