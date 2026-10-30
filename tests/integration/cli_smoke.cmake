if(NOT DEFINED FLATWHITE_BINARY OR NOT DEFINED FLATWHITE_SOURCE_DIR OR
   NOT DEFINED FLATWHITE_TEST_DIR OR NOT DEFINED FLATWHITE_CASE)
  message(FATAL_ERROR "integration test parameters are required")
endif()

file(REMOVE_RECURSE "${FLATWHITE_TEST_DIR}")
file(MAKE_DIRECTORY "${FLATWHITE_TEST_DIR}")

function(run_flatwhite output_var error_var result_var)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "FLATWHITE_FAKE_MODEL=1"
      "${FLATWHITE_BINARY}" ${ARGN} --data-dir "${FLATWHITE_TEST_DIR}"
    WORKING_DIRECTORY "${FLATWHITE_SOURCE_DIR}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result
  )
  set(${output_var} "${output}" PARENT_SCOPE)
  set(${error_var} "${error}" PARENT_SCOPE)
  set(${result_var} "${result}" PARENT_SCOPE)
endfunction()

if(FLATWHITE_CASE STREQUAL "fake_json")
  run_flatwhite(output error result -p "integration prompt" --output-format json)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "fake-model command failed (${result}): ${error}")
  endif()

  string(JSON answer GET "${output}" answer)
  string(JSON state GET "${output}" state)
  if(NOT answer STREQUAL "Hello from flatwhite." OR NOT state STREQUAL "completed")
    message(FATAL_ERROR "unexpected fake-model output: ${output}")
  endif()
elseif(FLATWHITE_CASE STREQUAL "sessions")
  run_flatwhite(output error result -p "integration session")
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "session setup command failed (${result}): ${error}")
  endif()

  run_flatwhite(sessions_output sessions_error sessions_result sessions list)
  if(NOT sessions_result EQUAL 0)
    message(FATAL_ERROR "session list command failed (${sessions_result}): ${sessions_error}")
  endif()
  if(NOT sessions_output MATCHES "integration session")
    message(FATAL_ERROR "created session was not listed: ${sessions_output}")
  endif()
elseif(FLATWHITE_CASE STREQUAL "interactive")
  set(input_file "${FLATWHITE_TEST_DIR}/interactive-input.txt")
  file(WRITE "${input_file}" "/help\n/model missing\n/new\n/exit\n")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "FLATWHITE_FAKE_MODEL=1"
      "${FLATWHITE_BINARY}" --data-dir "${FLATWHITE_TEST_DIR}"
    WORKING_DIRECTORY "${FLATWHITE_SOURCE_DIR}"
    INPUT_FILE "${input_file}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "interactive command failed (${result}): ${error}")
  endif()
  if(NOT output MATCHES "Commands:" OR NOT output MATCHES "session started" OR
     NOT error MATCHES "model not found: missing")
    message(FATAL_ERROR "unexpected interactive output: ${output}")
  endif()
else()
  message(FATAL_ERROR "unknown integration case: ${FLATWHITE_CASE}")
endif()
