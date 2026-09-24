cmake_minimum_required(VERSION 3.20)

# The build normally reuses the sources FetchContent already dropped into
# build/_deps, so it works offline. A fresh checkout (CI, another machine) has
# nothing there yet and needs -DFETCHCONTENT_FULLY_DISCONNECTED=OFF once, which
# only works because these are plain cache options rather than forced ones.
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Disable FetchContent download")
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")

# Where a WebAssembly build drops its .js/.wasm pair. It points into the static
# site (../../site) because that is what publishes it; override it to build the
# module standalone.
set(SPINE_STUDIO_WASM_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/../../site/source/spine-studio"
    CACHE PATH "Output directory for the Emscripten build")

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

if(NOT EMSCRIPTEN)
    # OpenGL is provided by the operating system. Emscripten is the exception:
    # there the GL symbols resolve against the JavaScript library instead.
    find_package(OpenGL REQUIRED)
endif()

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
    src/WebFileBridge.cpp
    src/LayoutStore.cpp
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

# GLFW must not pull in the OpenGL headers: each platform gets its core profile
# or ES headers through PlatformGL.h instead. Without this, glfw3.h drags in
# whatever it finds first - only GL 1.1 on Linux - and the two headers fight.
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    spine-cpp
    spine-cpp38
    spine-runtime-42
    spine-runtime-38
    stb
    gif
)

if(NOT EMSCRIPTEN)
    # Everything but the browser gets GLFW as a normal CMake target; there it
    # comes from the Emscripten port instead.
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
endif()

if(EMSCRIPTEN)
    # The browser replaces both native dependencies: GLFW comes from
    # Emscripten's contrib port and GL from WebGL. Neither has a CMake target,
    # everything about them is expressed as compiler and linker flags. See
    # third_party/CMakeLists.txt for why the desktop GLFW is not built here.
    set(SPINE_STUDIO_PORT "--use-port=contrib.glfw3")

    target_compile_options(${PROJECT_NAME} PRIVATE ${SPINE_STUDIO_PORT})

    # Emit a JavaScript wrapper plus a .wasm rather than a host executable or a
    # stand-alone HTML shell: the static site owns the page and boots the
    # module into a canvas of its own.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "spine-animation-studio"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${SPINE_STUDIO_WASM_OUTPUT}")

    target_link_options(${PROJECT_NAME} PRIVATE
        ${SPINE_STUDIO_PORT}
        # WebGL 2 / GLES 3.0, matching what PlatformGL.h and the shaders expect.
        -sMAX_WEBGL_VERSION=2
        -sMIN_WEBGL_VERSION=2
        -sFULL_ES3=1
        # The wasm heap cannot be sized up front: atlas images and export
        # framebuffers keep arriving until the user loads something else.
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=128MB
        # emscripten_set_main_loop hands control back to the browser every
        # frame, so the runtime must outlive main() returning.
        -sNO_EXIT_RUNTIME=1
        # Exported as a factory instead of running at load time, so the page
        # decides when to boot and where the canvas lives.
        -sMODULARIZE=1
        -sEXPORT_NAME=SpineAnimationStudio
        -sENVIRONMENT=web
        # WebFileBridge.cpp calls into this from JavaScript and needs FS to
        # copy the picked files into MEMFS.
        -sEXPORTED_FUNCTIONS=_main,_spine_studio_open_path
        -sEXPORTED_RUNTIME_METHODS=FS,ccall
    )
else()
    if(APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL ${COCOA_LIBRARY})
    else()
        # Off macOS nothing agrees on where GL lives: the system headers only
        # reach 1.1 and every 3.x entry point has to be loaded at run time.
        target_link_libraries(${PROJECT_NAME} PRIVATE glad OpenGL::GL)
    endif()
endif()
