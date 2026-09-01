if(NOT DEFINED CSOUND_EXECUTABLE OR NOT DEFINED OPCODE_DIR OR NOT DEFINED CSD_FILE)
    message(FATAL_ERROR "CSOUND_EXECUTABLE, OPCODE_DIR and CSD_FILE are required")
endif()

execute_process(
    COMMAND "${CSOUND_EXECUTABLE}"
        --opcode-dir=${OPCODE_DIR}
        -n -d
        "${CSD_FILE}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr)

set(run_output "${run_stdout}${run_stderr}")
string(REPLACE "\r\n" "\n" run_output "${run_output}")
string(REPLACE "\r" "\n" run_output "${run_output}")
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "printing regression failed to run:\n${run_output}")
endif()

function(require_text text label)
    string(FIND "${run_output}" "${text}" match_at)
    if(match_at EQUAL -1)
        message(FATAL_ERROR "missing ${label} in printing output:\n${run_output}")
    endif()
endfunction()

function(require_count text expected label)
    set(rest "${run_output}")
    set(count 0)
    string(LENGTH "${text}" text_length)
    while(TRUE)
        string(FIND "${rest}" "${text}" match_at)
        if(match_at EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        math(EXPR next_at "${match_at} + ${text_length}")
        string(SUBSTRING "${rest}" ${next_at} -1 rest)
    endwhile()
    if(NOT count EQUAL expected)
        message(FATAL_ERROR "expected ${expected} ${label}, found ${count}:\n${run_output}")
    endif()
endfunction()

require_text(
    "CsnArr(shape=(2, 2), dtype=float64)\n[[1.2346 2]\n [3 4]]\n"
    "balanced matrix rendering with %.5g precision")
require_text(
    "CsnArr(shape=(4,), dtype=float64)\n[]\n"
    "empty-array rendering")
require_text(
    "CsnArr(shape=(1,), dtype=complex128)\n[1.2346-2.3457j]\n"
    "complex rendering with %.5g precision")
require_text(
    "CsnArr(shape=(1001,), dtype=float64)\n[0 1 2 ... 998 999 1000]\n"
    "summarized rendering")
require_text(
    "12346 12346]\nCSN_PRINT_LARGE_END"
    "rendering after print-buffer reallocation")
require_count(
    "CsnArr(shape=(2,), dtype=float64)\n[7 8]\n"
    2
    "independent k-rate renderings")
