cmake_minimum_required(VERSION 3.20)
project(wrs_debugger_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HUMANOID_DRIVER_ROOT "$ENV{HUMANOID_DRIVER_ROOT}" CACHE PATH "Path to humanoid-driver")
set(GENERIC_BIN_DIR "$ENV{GENERIC_BIN_DIR}" CACHE STRING "Platform-specific third-party library directory")

if(NOT HUMANOID_DRIVER_ROOT)
  message(FATAL_ERROR
    "HUMANOID_DRIVER_ROOT is required. Source humanoid-driver/script/setup.sh before configuring.")
endif()

if(NOT GENERIC_BIN_DIR)
  message(FATAL_ERROR
    "GENERIC_BIN_DIR is required. Source humanoid-driver/script/setup.sh before configuring.")
endif()

set(FASTDDS_ROOT "${HUMANOID_DRIVER_ROOT}/thirdparty/fastdds")
set(DDS_WRAPPER_ROOT "${HUMANOID_DRIVER_ROOT}/thirdparty/dds_wrapper")
set(DDS_WRAPPER_LIBRARY "${DDS_WRAPPER_ROOT}/lib/${GENERIC_BIN_DIR}/libdds_wrapper.so")
set(FASTDDS_LIBRARY_DIR "${FASTDDS_ROOT}/lib/${GENERIC_BIN_DIR}")
set(DDS_LIBRARY_DIRS "${FASTDDS_LIBRARY_DIR}" "${DDS_WRAPPER_ROOT}/lib/${GENERIC_BIN_DIR}")

foreach(directory IN LISTS DDS_LIBRARY_DIRS)
  if(NOT EXISTS "${directory}")
    message(FATAL_ERROR "Required DDS library directory does not exist: ${directory}")
  endif()
endforeach()

if(NOT EXISTS "${FASTDDS_ROOT}/include")
  message(FATAL_ERROR "Required FastDDS include directory does not exist: ${FASTDDS_ROOT}/include")
endif()

if(NOT EXISTS "${DDS_WRAPPER_ROOT}/include")
  message(FATAL_ERROR "Required DDS Wrapper include directory does not exist: ${DDS_WRAPPER_ROOT}/include")
endif()

if(NOT EXISTS "${DDS_WRAPPER_LIBRARY}")
  message(FATAL_ERROR "Required DDS Wrapper library does not exist: ${DDS_WRAPPER_LIBRARY}")
endif()

file(GLOB FASTDDS_LIBRARIES CONFIGURE_DEPENDS "${FASTDDS_LIBRARY_DIR}/*.so")
if(NOT FASTDDS_LIBRARIES)
  message(FATAL_ERROR "No FastDDS shared libraries found in: ${FASTDDS_LIBRARY_DIR}")
endif()

add_library(dds_wrapper SHARED IMPORTED GLOBAL)
set_target_properties(dds_wrapper PROPERTIES
  IMPORTED_LOCATION "${DDS_WRAPPER_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${DDS_WRAPPER_ROOT}/include")

add_library(wrs_receiver_dds STATIC
  wrs/src/receiver/dds_runtime.cpp
)
target_include_directories(wrs_receiver_dds
  PUBLIC wrs/include
  PRIVATE "${FASTDDS_ROOT}/include" "${DDS_WRAPPER_ROOT}/include")
target_link_libraries(wrs_receiver_dds PRIVATE dds_wrapper ${FASTDDS_LIBRARIES})
target_compile_options(wrs_receiver_dds PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(wrs_receiver_dds PROPERTIES BUILD_RPATH "${DDS_LIBRARY_DIRS}")

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
option(BUILD_TRANSMITTER_SIMULATOR_E2E "Build Host-to-Simulator transmitter E2E tests" OFF)
if(BUILD_TESTING)
  find_package(Threads REQUIRED)
  add_executable(wrs_transmitter_tests wrs/tests/transmitter_tests.cpp)
  target_link_libraries(wrs_transmitter_tests PRIVATE wrs_transmitter util Threads::Threads)
  target_compile_options(wrs_transmitter_tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME wrs_transmitter_tests COMMAND wrs_transmitter_tests)

  add_executable(wrs_receiver_dds_runtime_tests wrs/tests/receiver_dds_runtime_tests.cpp)
  target_link_libraries(wrs_receiver_dds_runtime_tests PRIVATE wrs_receiver_dds Threads::Threads)
  target_compile_definitions(wrs_receiver_dds_runtime_tests PRIVATE
    DDS_TEST_PROFILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/wrs/tests/data/dds_profile.xml")
  target_compile_options(wrs_receiver_dds_runtime_tests PRIVATE -Wall -Wextra -Wpedantic)
  set_target_properties(wrs_receiver_dds_runtime_tests PROPERTIES BUILD_RPATH "${DDS_LIBRARY_DIRS}")
  add_test(NAME wrs_receiver_dds_runtime_tests COMMAND wrs_receiver_dds_runtime_tests)

  if(BUILD_TRANSMITTER_SIMULATOR_E2E)
    set(BUILD_GUI OFF CACHE BOOL "Build the Dear ImGui simulator UI" FORCE)
    add_subdirectory(simulator/transmitter_simulator)
    add_executable(transmitter_simulator_e2e_tests wrs/tests/transmitter_simulator_e2e_tests.cpp)
    target_link_libraries(transmitter_simulator_e2e_tests PRIVATE wrs_transmitter transmitter_runtime)
    target_compile_options(transmitter_simulator_e2e_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME transmitter_simulator_e2e_tests COMMAND transmitter_simulator_e2e_tests)
  endif()
endif()
