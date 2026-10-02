# Clean only this designated, build-owned synthetic smoke artifact; actual recorder
# must still reject accidental overwrites of user-provided trace destinations.
file(REMOVE "${TRACE_OUTPUT}")
execute_process(COMMAND "${TRACE_EXE}" pack-ma1 "${TRACE_INPUT}" "${TRACE_OUTPUT}"
  fixture-ci synthetic.fixture RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Synthetic MA2 smoke packing failed: ${status}")
endif()
