cmake_minimum_required(VERSION 3.10)
project(CarSideServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    CarSideServer_main.cpp
)

# Create executable
add_executable(CarSideServer ${SOURCES})

# Windows-specific libraries
target_link_libraries(CarSideServer
    d3d11.lib
    dxgi.lib
    mf.lib
    mfplat.lib
    mfuuid.lib
    ws2_32.lib
    kernel32.lib
    user32.lib
    gdi32.lib
    ole32.lib
)

# Include directories
target_include_directories(CarSideServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler definitions
target_compile_definitions(CarSideServer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
)

# Set output directory
set_target_properties(CarSideServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)