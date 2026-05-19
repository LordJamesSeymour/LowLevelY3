cmake_minimum_required(VERSION 3.22)

project(LLGP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Builds the same C++/SFML source files on Raspberry Pi/Linux.
# Your existing Visual Studio solution can still be used on Windows.
find_package(SFML 3 COMPONENTS Graphics Window System Audio REQUIRED)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Do not accidentally compile generated/build folders if they exist inside the project.
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/build/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/cmake-build-.*/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/x64/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/Debug/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/Release/.*")

add_executable(LLGP ${PROJECT_SOURCES})

target_include_directories(LLGP PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(LLGP PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
)
