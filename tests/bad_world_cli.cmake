# Runs the game on a copy of data/ whose map has an unknown tile on line 2 and checks that it
# exits with code 2, naming the file and line. Arguments: HEROES, SOURCE_DIR, WORK_DIR.
file(REMOVE_RECURSE "${WORK_DIR}")
file(COPY "${SOURCE_DIR}/data" "${SOURCE_DIR}/assets" DESTINATION "${WORK_DIR}")
file(READ "${WORK_DIR}/data/world/overworld.txt" map)
string(REGEX REPLACE "^([^\n]*\n)." "\\1Q" map "${map}")
file(WRITE "${WORK_DIR}/data/world/overworld.txt" "${map}")
execute_process(COMMAND "${HEROES}" --data-dir "${WORK_DIR}/data"
                RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_QUIET)
if(NOT code EQUAL 2 OR NOT err MATCHES "world/overworld.txt: line 2: unknown tile character 'Q'")
    message(FATAL_ERROR "expected exit 2 naming line 2, got ${code}: ${err}")
endif()
