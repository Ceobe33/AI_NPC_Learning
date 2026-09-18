cmake_minimum_required(VERSION 3.20)

set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Disable FetchContent download" FORCE)
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)

# AppKit based file dialogs need the Objective-C++ compiler on macOS.
if(APPLE)
    project(SpineAnimationStudio LANGUAGES CXX OBJCXX)
else()
    project(SpineAnimationStudio LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find all third-party dependencies.
add_subdirectory(third_party)

# OpenGL is provided by the operating system.
find_package(OpenGL REQUIRED)

# Native file dialogs are implemented with AppKit on macOS.
if(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
endif()

# SpineRuntimeImpl.cpp is written once but compiled twice: once per spine
# runtime (4.2 and 3.8), each with its own namespace.
add_library(spine-runtime-42 OBJECT src/SpineRuntimeImpl.cpp)
target_compile_definitions(spine-runtime-42 PRIVATE SPINE_RUNTIME_42)
target_link_libraries(spine-runtime-42 PRIVATE spine-cpp stb)

add_library(spine-runtime-38 OBJECT src/SpineRuntimeImpl.cpp)
target_compile_definitions(spine-runtime-38 PRIVATE SPINE_RUNTIME_38)
target_link_libraries(spine-runtime-38 PRIVATE spine-cpp38 stb)

set(PROJECT_SOURCES
    src/main.cpp
    src/App.cpp
    src/SpineAsset.cpp
    src/SpineRenderer.cpp
    src/SpineVersion.cpp
    src/StbImage.cpp
    src/GifEncoder.cpp
)

if(APPLE)
    list(APPEND PROJECT_SOURCES src/FileDialog.mm)
else()
    list(APPEND PROJECT_SOURCES src/FileDialog.cpp)
endif()

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

# GLFW must not pull in the legacy OpenGL headers; we include the core
# profile headers explicitly through PlatformGL.h.
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    imgui
    spine-cpp
    spine-cpp38
    spine-runtime-42
    spine-runtime-38
    stb
    gif
    OpenGL::GL
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY})
endif()
