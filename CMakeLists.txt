cmake_minimum_required(VERSION 3.24)
project(localagent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LOCALAGENT_BUILD_TESTS "Build unit tests" ON)
option(LOCALAGENT_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LOCALAGENT_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.0
)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.1
)
FetchContent_MakeAvailable(Catch2 nlohmann_json spdlog)

find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

add_library(localagent_core
  src/common/strong_id.cpp
  src/common/json_util.cpp
  src/common/error.cpp
  src/common/cancellation.cpp
  src/common/redactor.cpp
  src/config/config.cpp
  src/process/posix_process.cpp
  src/persistence/sqlite_store.cpp
  src/persistence/event_log.cpp
  src/permissions/permission_engine.cpp
  src/permissions/command_classifier.cpp
  src/model/model_types.cpp
  src/agent/types.cpp
  src/model/router/model_router.cpp
  src/model/ollama/ollama_backend.cpp
  src/model/ollama/stream_parser.cpp
  src/tools/filesystem/file_tools.cpp
  src/tools/filesystem/patch_engine.cpp
  src/tools/shell/shell_tool.cpp
  src/tools/git/git_tools.cpp
  src/tools/tool_registry.cpp
  src/context/context_builder.cpp
  src/context/rules_loader.cpp
  src/context/project_detector.cpp
  src/agent/planner/planner.cpp
  src/agent/verifier/verifier.cpp
  src/agent/state_machine/state_machine.cpp
  src/agent/runtime/agent_runtime.cpp
  src/cli/command_router.cpp
  src/app/session_controller.cpp
)
add_library(localagent::core ALIAS localagent_core)

target_include_directories(localagent_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(localagent_core
  PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    CURL::libcurl
    SQLite::SQLite3
    Threads::Threads
)

if(LOCALAGENT_ENABLE_ASAN)
  target_compile_options(localagent_core PUBLIC -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(localagent_core PUBLIC -fsanitize=address)
endif()
if(LOCALAGENT_ENABLE_UBSAN)
  target_compile_options(localagent_core PUBLIC -fsanitize=undefined -fno-omit-frame-pointer)
  target_link_options(localagent_core PUBLIC -fsanitize=undefined)
endif()

add_executable(localagent src/main.cpp)
target_link_libraries(localagent PRIVATE localagent::core)

if(LOCALAGENT_BUILD_TESTS)
  enable_testing()
  add_executable(localagent_tests
    tests/unit/test_main.cpp
    tests/unit/test_strong_id.cpp
    tests/unit/test_config.cpp
    tests/unit/test_budget.cpp
    tests/unit/test_cancellation.cpp
    tests/unit/test_permissions.cpp
    tests/unit/test_command_classifier.cpp
    tests/unit/test_redactor.cpp
    tests/unit/test_patch_engine.cpp
    tests/unit/test_model_router.cpp
    tests/unit/test_stream_parser.cpp
    tests/unit/test_context_builder.cpp
    tests/unit/test_rules_loader.cpp
    tests/unit/test_project_detector.cpp
    tests/unit/test_state_machine.cpp
    tests/unit/test_planner.cpp
    tests/unit/test_verifier.cpp
    tests/unit/test_event_log.cpp
    tests/unit/test_sqlite_store.cpp
    tests/unit/test_process.cpp
    tests/unit/test_tool_registry.cpp
    tests/unit/test_file_tools.cpp
    tests/unit/test_json_util.cpp
    tests/unit/test_error.cpp
    tests/unit/test_agent_runtime.cpp
    tests/unit/test_command_router.cpp
  )
  target_link_libraries(localagent_tests PRIVATE localagent::core Catch2::Catch2WithMain)
  include(CTest)
  include(Catch)
  catch_discover_tests(localagent_tests)
endif()

install(TARGETS localagent RUNTIME DESTINATION bin)
