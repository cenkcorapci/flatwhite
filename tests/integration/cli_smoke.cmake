if(NOT DEFINED LOCALAGENT_BINARY OR NOT DEFINED LOCALAGENT_SOURCE_DIR OR
   NOT DEFINED LOCALAGENT_TEST_DIR OR NOT DEFINED LOCALAGENT_CASE)
  message(FATAL_ERROR "integration test parameters are required")
endif()

file(REMOVE_RECURSE "${LOCALAGENT_TEST_DIR}")
file(MAKE_DIRECTORY "${LOCALAGENT_TEST_DIR}")

function(run_localagent output_var error_var result_var)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "LOCALAGENT_FAKE_MODEL=1"
      "${LOCALAGENT_BINARY}" ${ARGN} --data-dir "${LOCALAGENT_TEST_DIR}"
    WORKING_DIRECTORY "${LOCALAGENT_SOURCE_DIR}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result
  )
  set(${output_var} "${output}" PARENT_SCOPE)
  set(${error_var} "${error}" PARENT_SCOPE)
  set(${result_var} "${result}" PARENT_SCOPE)
endfunction()

if(LOCALAGENT_CASE STREQUAL "fake_json")
  run_localagent(output error result -p "integration prompt" --output-format json)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "fake-model command failed (${result}): ${error}")
  endif()

  string(JSON answer GET "${output}" answer)
  string(JSON state GET "${output}" state)
  if(NOT answer STREQUAL "Hello from localagent." OR NOT state STREQUAL "completed")
    message(FATAL_ERROR "unexpected fake-model output: ${output}")
  endif()
elseif(LOCALAGENT_CASE STREQUAL "sessions")
  run_localagent(output error result -p "integration session")
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "session setup command failed (${result}): ${error}")
  endif()

  run_localagent(sessions_output sessions_error sessions_result sessions list)
  if(NOT sessions_result EQUAL 0)
    message(FATAL_ERROR "session list command failed (${sessions_result}): ${sessions_error}")
  endif()
  if(NOT sessions_output MATCHES "integration session")
    message(FATAL_ERROR "created session was not listed: ${sessions_output}")
  endif()
else()
  message(FATAL_ERROR "unknown integration case: ${LOCALAGENT_CASE}")
endif()
