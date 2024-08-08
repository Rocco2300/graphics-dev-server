cmake_minimum_required(VERSION 3.28)
project(graphics-dev-srv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "/O2")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

find_package(minhook CONFIG REQUIRED)
find_package(ffmpeg COMPONENTS avcodec avfilter avformat avutil swscale)

add_executable(${PROJECT_NAME} src/server.cpp)

if (FFMPEG_FOUND)
    target_link_libraries(${PROJECT_NAME} ${FFMPEG_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
endif ()

add_library(shim SHARED src/shim.cpp)
target_link_libraries(shim PRIVATE opengl32 minhook::minhook)