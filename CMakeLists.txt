cmake_minimum_required(VERSION 3.24)
project(SplitBox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_executable(SplitBox WIN32
    src/main.cpp
    src/split_screen.cpp
    src/openmouse_bridge.cpp
)

target_compile_definitions(SplitBox PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(SplitBox PRIVATE
    comctl32
    dwmapi
    shell32
    shlwapi
    user32
    gdi32
    ole32
)

if(MSVC)
    target_compile_options(SplitBox PRIVATE /utf-8)
endif()

FetchContent_Declare(
    openmouse_source
    GIT_REPOSITORY https://github.com/alstonmendonca/openMouse.git
    GIT_TAG dee7810ec23aac5de13c544a57e9e519b176062c
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(openmouse_source)

add_dependencies(SplitBox openmouse)

set_target_properties(SplitBox openmouse PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)
