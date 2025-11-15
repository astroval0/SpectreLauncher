cmake_minimum_required(VERSION 4.0)
project(SpectreLauncher)

set(CMAKE_CXX_STANDARD 26)

add_executable(SpectreLauncher
        src/main.cpp
        src/steam_finder.cpp
        src/process_utils.cpp
        src/file_utils.cpp
        src/registry_utils.cpp
        src/page_trigger.cpp
)

target_include_directories(SpectreLauncher PRIVATE src)

set_target_properties(SpectreLauncher PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/built"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/built"
)