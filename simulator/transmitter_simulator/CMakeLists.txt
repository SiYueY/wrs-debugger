cmake_minimum_required(VERSION 3.20)
project(transmitter_simulator LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)
include(FetchContent)

option(BUILD_GUI "Build the Dear ImGui simulator UI" ON)

add_library(transmitter_runtime STATIC
  src/protocol.cpp
  src/device.cpp
  src/fault.cpp
  src/history.cpp
  src/transport.cpp
  src/simulator.cpp
)
target_compile_features(transmitter_runtime PUBLIC cxx_std_17)
target_include_directories(transmitter_runtime PUBLIC include)
target_link_libraries(transmitter_runtime PUBLIC Threads::Threads)
target_compile_options(transmitter_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BUILD_GUI)
  find_package(OpenGL REQUIRED)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
    GIT_SHALLOW TRUE
  )
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(glfw imgui)
  add_library(transmitter_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(transmitter_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(transmitter_imgui PUBLIC glfw OpenGL::GL)
  target_compile_features(transmitter_imgui PUBLIC cxx_std_17)
  target_compile_options(transmitter_imgui PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(transmitter_simulator src/main.cpp)
target_link_libraries(transmitter_simulator PRIVATE transmitter_runtime)
target_compile_options(transmitter_simulator PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(BUILD_GUI)
  target_sources(transmitter_simulator PRIVATE src/app.cpp src/ui.cpp)
  target_link_libraries(transmitter_simulator PRIVATE transmitter_imgui)
  target_compile_definitions(transmitter_simulator PRIVATE TRANSMITTER_SIMULATOR_WITH_GUI=1)
endif()

if(BUILD_TESTING)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v2.13.10
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Catch2)
  add_executable(transmitter_simulator_tests
    tests/protocol_tests.cpp
    tests/device_tests.cpp
    tests/simulator_tests.cpp
  )
  target_link_libraries(transmitter_simulator_tests PRIVATE transmitter_runtime Catch2::Catch2)
  target_compile_options(transmitter_simulator_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME transmitter_simulator_tests COMMAND transmitter_simulator_tests)
endif()
