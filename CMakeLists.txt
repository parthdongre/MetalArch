cmake_minimum_required(VERSION 3.20)
project(MetalArch VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(METALARCH_SANITIZE "Enable ASan/UBSan on supported compilers" OFF)
find_package(Threads REQUIRED)
add_library(metalarch STATIC src/core.cpp src/engines.cpp src/legacy_extensions.cpp)
target_include_directories(metalarch PUBLIC include)
target_link_libraries(metalarch PUBLIC Threads::Threads)
target_compile_options(metalarch PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
if(METALARCH_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(metalarch PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(metalarch PUBLIC -fsanitize=address,undefined)
endif()
add_executable(metalarch_cli src/main.cpp)
target_link_libraries(metalarch_cli PRIVATE metalarch)
add_executable(metalarch_tests tests/test_core.cpp)
target_link_libraries(metalarch_tests PRIVATE metalarch)
add_executable(metalarch_compare benchmarks/compare.cpp)
target_link_libraries(metalarch_compare PRIVATE metalarch)
add_executable(metalarch_bench benchmarks/bench.cpp)
target_link_libraries(metalarch_bench PRIVATE metalarch)
add_executable(metalarch_legacy_tests tests/test_legacy.cpp)
target_link_libraries(metalarch_legacy_tests PRIVATE metalarch)
enable_testing()
add_test(NAME metalarch_legacy_tests COMMAND metalarch_legacy_tests)
add_test(NAME metalarch_tests COMMAND metalarch_tests)
add_test(NAME metalarch_replay_smoke COMMAND metalarch_cli replay
         ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_70.ma1 p 8000000)
add_test(NAME metalarch_policy_smoke COMMAND metalarch_compare
         ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_70.ma1
         ${CMAKE_CURRENT_BINARY_DIR}/policy-smoke.csv 1 65 8000000)

add_test(NAME metalarch_replay_core COMMAND metalarch_cli replay
         ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_70.ma1 b2 8000000 core)
add_test(NAME metalarch_policy_smoke_core COMMAND metalarch_compare
         ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_70.ma1
         ${CMAKE_CURRENT_BINARY_DIR}/policy-smoke-core.csv 1 65 8000000 core)
