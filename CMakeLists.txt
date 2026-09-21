cmake_minimum_required(VERSION 3.24)
project(SplitBox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(SplitBox WIN32
    src/main.cpp
)

target_compile_definitions(SplitBox PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(SplitBox PRIVATE
    comctl32
    dwmapi
    shell32
    shlwapi
    user32
    gdi32
)
