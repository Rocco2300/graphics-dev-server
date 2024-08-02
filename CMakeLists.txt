cmake_minimum_required(VERSION 3.28)
project(graphics-dev-srv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(external/minhook/include)

add_subdirectory(external/minhook)

add_executable(${PROJECT_NAME} src/server.cpp)

add_library(shim SHARED src/shim.cpp)
target_link_libraries(shim minhook opengl32)