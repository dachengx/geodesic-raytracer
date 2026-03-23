cmake_minimum_required(VERSION 3.18)
project(geodesic_raytracer LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_ARCHITECTURES native)

find_package(OpenGL REQUIRED)

include(FetchContent)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# glad
add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/include)

add_executable(raytracer
  src/main.cpp
  src/renderer.cpp
  src/raytracer.cu
)

target_include_directories(raytracer PRIVATE src)
target_link_libraries(raytracer PRIVATE glad glfw OpenGL::GL)
set_target_properties(raytracer PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
