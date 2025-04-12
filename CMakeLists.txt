cmake_minimum_required(VERSION 3.10)
project(Turnbinds LANGUAGES CXX)

# Set the vcpkg toolchain file (optional if vcpkg integrate install was run)
set(CMAKE_TOOLCHAIN_FILE "C:/Users/Jacob/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable
add_executable(turnbinds
    main.cpp
    ConsoleUI.cpp
    Settings.cpp
    MouseSimulation.cpp
    CS2Monitor.cpp
    NetworkUtils.cpp
    Utils.cpp
)

# Include directories (headers are in the same directory)
target_include_directories(turnbinds PRIVATE .)

# Find and link nlohmann_json
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(turnbinds PRIVATE nlohmann_json::nlohmann_json)

# Link Windows libraries
target_link_libraries(turnbinds PRIVATE winmm iphlpapi ws2_32)

# Windows-specific definitions
if (WIN32)
    target_compile_definitions(turnbinds PRIVATE NOMINMAX _WIN32_WINNT=0x0600)
endif()