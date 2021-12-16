# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.

cmake_minimum_required (VERSION 3.8)

project ("pain")

find_package(glfw3 REQUIRED)
find_package(glad REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(game ${GAME_SOURCES})
target_include_directories(game PRIVATE include/)
target_link_libraries(game glfw glad::glad spdlog::spdlog glm::glm)
