if(NOT DEFINED LOCALAGENT_DOCKER OR NOT DEFINED LOCALAGENT_SOURCE_DIR OR
   NOT DEFINED LOCALAGENT_TEST_DIR)
  message(FATAL_ERROR "Docker installer test parameters are required")
endif()

file(MAKE_DIRECTORY "${LOCALAGENT_TEST_DIR}")
set(image_id_file "${LOCALAGENT_TEST_DIR}/installer-image-id.txt")
file(REMOVE "${image_id_file}")

execute_process(
  COMMAND "${LOCALAGENT_DOCKER}" build --progress=plain
    --iidfile "${image_id_file}"
    --file "${LOCALAGENT_SOURCE_DIR}/tests/integration/Dockerfile.installer"
    "${LOCALAGENT_SOURCE_DIR}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Docker installer integration failed (${result}):\n${output}\n${error}")
endif()
if(NOT EXISTS "${image_id_file}")
  message(FATAL_ERROR
    "Docker installer integration succeeded without producing an image ID:\n${output}\n${error}")
endif()

file(READ "${image_id_file}" image_id)
string(STRIP "${image_id}" image_id)
if(image_id STREQUAL "")
  message(FATAL_ERROR "Docker installer integration did not produce an image ID")
endif()
execute_process(COMMAND "${LOCALAGENT_DOCKER}" image rm --force "${image_id}")
