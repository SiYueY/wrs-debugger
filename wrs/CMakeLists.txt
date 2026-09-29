cmake_minimum_required(VERSION 3.20)
project(wrs_debugger_native LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TRANSMITTER_PYTHON "Build pybind11 transmitter module" OFF)
option(BUILD_TRANSMITTER_SIMULATOR_E2E "Build Host-to-Simulator transmitter E2E tests" ON)

add_library(wrs_transmitter STATIC
  src/serial/port.cpp src/serial/tool.cpp src/serial/tty.cpp
  src/transmitter/client.cpp src/transmitter/discovery.cpp src/transmitter/protocol.cpp)
set_target_properties(wrs_transmitter PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(wrs_transmitter PUBLIC include)
target_compile_options(wrs_transmitter PRIVATE -Wall -Wextra -Wpedantic)

add_library(wrs_receiver STATIC
  src/receiver/client.cpp src/receiver/protocol.cpp src/receiver/driver_proxy.cpp)
set_target_properties(wrs_receiver PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(wrs_receiver PUBLIC include)
target_compile_options(wrs_receiver PRIVATE -Wall -Wextra -Wpedantic)

set(HUMANOID_DRIVER_ROOT "$ENV{HUMANOID_DRIVER_ROOT}" CACHE PATH "Path to humanoid-driver runtime root")
set(GENERIC_BIN_DIR "$ENV{GENERIC_BIN_DIR}" CACHE STRING "Platform library directory")
if(NOT HUMANOID_DRIVER_ROOT OR NOT GENERIC_BIN_DIR)
  message(FATAL_ERROR "HUMANOID_DRIVER_ROOT and GENERIC_BIN_DIR are required for Receiver DDS.")
endif()
set(FASTDDS_ROOT "${HUMANOID_DRIVER_ROOT}/thirdparty/fastdds")
set(DDS_WRAPPER_ROOT "${HUMANOID_DRIVER_ROOT}/thirdparty/dds_wrapper")
set(DDS_WRAPPER_LIBRARY "${DDS_WRAPPER_ROOT}/lib/${GENERIC_BIN_DIR}/libdds_wrapper.so")
set(FASTDDS_LIBRARY_DIR "${FASTDDS_ROOT}/lib/${GENERIC_BIN_DIR}")
set(ESTOP_DDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/receiver/dds/generated")
list(APPEND CMAKE_BUILD_RPATH "${FASTDDS_LIBRARY_DIR}" "${DDS_WRAPPER_ROOT}/lib/${GENERIC_BIN_DIR}")
if(NOT EXISTS "${DDS_WRAPPER_LIBRARY}" OR NOT EXISTS "${ESTOP_DDS_DIR}/srv/GetEStopConfig_.cxx")
  message(FATAL_ERROR "Receiver DDS dependencies or generated WirelessEStop types are unavailable.")
endif()
add_library(dds_wrapper SHARED IMPORTED GLOBAL)
set_target_properties(dds_wrapper PROPERTIES IMPORTED_LOCATION "${DDS_WRAPPER_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${DDS_WRAPPER_ROOT}/include")
foreach(service GetEStopBindingState GetEStopConfig SetEStopConfig StartEStopBinding)
  target_sources(wrs_receiver PRIVATE
    "${ESTOP_DDS_DIR}/srv/${service}_.cxx"
    "${ESTOP_DDS_DIR}/srv/${service}_PubSubTypes.cxx")
endforeach()
target_sources(wrs_receiver PRIVATE
  "${ESTOP_DDS_DIR}/msg/WirelessEStopState_.cxx"
  "${ESTOP_DDS_DIR}/msg/WirelessEStopState_PubSubTypes.cxx")
target_include_directories(wrs_receiver PRIVATE
  "${FASTDDS_ROOT}/include" "${DDS_WRAPPER_ROOT}/include"
  "${ESTOP_DDS_DIR}/srv" "${ESTOP_DDS_DIR}/msg")
target_link_libraries(wrs_receiver PUBLIC dds_wrapper
  "${FASTDDS_LIBRARY_DIR}/libfastrtps.so"
  "${FASTDDS_LIBRARY_DIR}/libfastcdr.so")
target_compile_options(wrs_receiver PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TRANSMITTER_PYTHON)
  find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
  find_package(pybind11 CONFIG REQUIRED)
  pybind11_add_module(
    wrs_debugger_native
    bindings/python/module.cpp
    bindings/python/transmitter.cpp
    bindings/python/receiver.cpp)
  target_link_libraries(wrs_debugger_native PRIVATE wrs_transmitter wrs_receiver)
  # The shipped Fast DDS DSOs have transitive dependencies outside the system loader path.
  target_link_options(wrs_debugger_native PRIVATE -Wl,--disable-new-dtags)
  target_include_directories(wrs_debugger_native PRIVATE include)
  target_compile_options(wrs_debugger_native PRIVATE -Wall -Wextra -Wpedantic)
  if(DEFINED WRS_PYTHON_OUTPUT_DIR)
    set_target_properties(wrs_debugger_native PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${WRS_PYTHON_OUTPUT_DIR}")
  endif()
endif()

include(CTest)
if(BUILD_TESTING)
  find_package(Threads REQUIRED)
  add_executable(wrs_transmitter_tests tests/transmitter_tests.cpp)
  target_link_libraries(wrs_transmitter_tests PRIVATE wrs_transmitter util Threads::Threads)
  target_compile_options(wrs_transmitter_tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME wrs_transmitter_tests COMMAND wrs_transmitter_tests)
  add_executable(wrs_receiver_tests tests/receiver_tests.cpp)
  target_link_libraries(wrs_receiver_tests PRIVATE wrs_receiver)
  target_compile_options(wrs_receiver_tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME wrs_receiver_tests COMMAND wrs_receiver_tests)
  if(BUILD_TRANSMITTER_SIMULATOR_E2E)
    set(BUILD_GUI OFF CACHE BOOL "Build simulator GUI" FORCE)
    set(BUILD_TESTING OFF)
    add_subdirectory(../simulator/transmitter_simulator ${CMAKE_CURRENT_BINARY_DIR}/transmitter_simulator)
    set(BUILD_TESTING ON)
    add_executable(transmitter_simulator_e2e_tests tests/transmitter_simulator_e2e_tests.cpp)
    target_link_libraries(transmitter_simulator_e2e_tests PRIVATE wrs_transmitter transmitter_runtime)
    target_compile_options(transmitter_simulator_e2e_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME transmitter_simulator_e2e_tests COMMAND transmitter_simulator_e2e_tests)
  endif()
  add_executable(wrs_receiver_dds_runtime_tests tests/receiver_dds_runtime_tests.cpp)
  target_link_libraries(wrs_receiver_dds_runtime_tests PRIVATE wrs_receiver Threads::Threads)
  target_compile_definitions(wrs_receiver_dds_runtime_tests PRIVATE DDS_TEST_PROFILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dds_profile.xml")
  add_test(NAME wrs_receiver_dds_runtime_tests COMMAND wrs_receiver_dds_runtime_tests)
  set_tests_properties(wrs_receiver_tests wrs_receiver_dds_runtime_tests PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${FASTDDS_LIBRARY_DIR}:${DDS_WRAPPER_ROOT}/lib/${GENERIC_BIN_DIR}")
endif()
