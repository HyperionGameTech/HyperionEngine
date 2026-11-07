execute_process(
    COMMAND "${NM}" --extern-only --defined-only "${ARCHIVE}"
    OUTPUT_VARIABLE nm_output
    ERROR_QUIET
    COMMAND_ERROR_IS_FATAL ANY)

string(REPLACE "\n" ";" nm_lines "${nm_output}")

set(function_symbols)
set(data_symbols)

foreach(line ${nm_lines})
  if(line MATCHES "^[0-9a-fA-F]+ ([TDBR]) ([^ .][^ ]*3JPH[^ ]*)$")
    if(CMAKE_MATCH_1 STREQUAL "T")
      list(APPEND function_symbols "${CMAKE_MATCH_2}")
    else()
      list(APPEND data_symbols "${CMAKE_MATCH_2}")
    endif()
  endif()
endforeach()

list(REMOVE_DUPLICATES function_symbols)
list(REMOVE_DUPLICATES data_symbols)

if(data_symbols)
  list(REMOVE_ITEM function_symbols ${data_symbols})
endif()

set(def_text "EXPORTS\n")

foreach(symbol ${function_symbols})
  string(APPEND def_text "    ${symbol}\n")
endforeach()

foreach(symbol ${data_symbols})
  string(APPEND def_text "    ${symbol} DATA\n")
endforeach()

file(WRITE "${OUTPUT}.tmp" "${def_text}")
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
