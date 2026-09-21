cmake_minimum_required(VERSION 3.20)
project(wrs_debugger_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wrs_transmitter STATIC
  wrs/src/serial/port.cpp
  wrs/src/serial/tool.cpp
  wrs/src/serial/tty.cpp
  wrs/src/transmitter/client.cpp
  wrs/src/transmitter/discovery.cpp
  wrs/src/transmitter/protocol.cpp
)
target_include_directories(wrs_transmitter PUBLIC wrs/include)
target_compile_options(wrs_transmitter PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
  find_package(Threads REQUIRED)
  add_executable(wrs_transmitter_tests wrs/tests/transmitter_tests.cpp)
  target_link_libraries(wrs_transmitter_tests PRIVATE wrs_transmitter util Threads::Threads)
  target_compile_options(wrs_transmitter_tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME wrs_transmitter_tests COMMAND wrs_transmitter_tests)
endif()
