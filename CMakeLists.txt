cmake_minimum_required(VERSION 3.24)
project(Genesis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Vulkan (uses VULKAN_SDK env var set by the SDK installer)
find_package(Vulkan REQUIRED)

# Find GLFW and GLM via vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# Shader compilation helper
include(cmake/CompileShaders.cmake)

# Subdirectories
add_subdirectory(engine)
add_subdirectory(app)
