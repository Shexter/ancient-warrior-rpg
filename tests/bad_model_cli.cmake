# Runs heroes-window on a copy of data/ and assets/ whose Fighter model is not a model, and checks
# that it exits with code 2 naming the file before any window opens. Arguments: HEROES,
# SOURCE_DIR, WORK_DIR.
file(REMOVE_RECURSE "${WORK_DIR}")
file(COPY "${SOURCE_DIR}/data" "${SOURCE_DIR}/assets" DESTINATION "${WORK_DIR}")
file(WRITE "${WORK_DIR}/assets/models/hero_fighter.glb" "not a model")
execute_process(COMMAND "${HEROES}" --data-dir "${WORK_DIR}/data"
                RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_QUIET)
if(NOT code EQUAL 2 OR NOT err MATCHES "hero_fighter.glb: not a loadable animated model")
    message(FATAL_ERROR "expected exit 2 naming the model, got ${code}: ${err}")
endif()
