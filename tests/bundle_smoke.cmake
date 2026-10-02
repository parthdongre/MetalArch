file(REMOVE "${TRACE_OUTPUT}")
execute_process(COMMAND "${TRACE_EXE}" record-bundle "${TRACE_MANIFEST}" "${TRACE_OUTPUT}"
  synthetic-ci-bundle RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Synthetic bundle M2 capture failed: ${status}")
endif()
