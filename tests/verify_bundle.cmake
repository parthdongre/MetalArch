execute_process(COMMAND "${TRACE_EXE}" verify "${TRACE_OUTPUT}"
  OUTPUT_VARIABLE report RESULT_VARIABLE status)
if(NOT status EQUAL 0)
 message(FATAL_ERROR "Synthetic bundle verification failed: ${status}")
endif()
string(REGEX MATCH "root_sha256=([a-f0-9]+)" match "${report}")
if(NOT CMAKE_MATCH_1)
 message(FATAL_ERROR "Root missing in verifier output")
endif()
execute_process(COMMAND "${TRACE_EXE}" verify "${TRACE_OUTPUT}" "${CMAKE_MATCH_1}" "${TRACE_MANIFEST}"
  RESULT_VARIABLE status)
if(NOT status EQUAL 0)
 message(FATAL_ERROR "Independent root/manifest pin failed: ${status}")
endif()
