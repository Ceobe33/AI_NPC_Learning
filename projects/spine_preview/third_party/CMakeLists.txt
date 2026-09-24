include(FetchContent)

if(EMSCRIPTEN)
    # No desktop GLFW here. Its source tree has no browser platform - only X11,
    # Wayland, Cocoa, Win32 and a null backend - so building it would either
    # fail or produce a window that never receives a canvas. Emscripten's
    # contrib port supplies a GLFW 3.4 implementation written against the DOM
    # instead; the compiler and linker flags activating it live in the root
    # CMakeLists.txt.
else()

    # ============================================================
    # GLFW
    # ============================================================

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )

    FetchContent_MakeAvailable(glfw)

endif()


# ============================================================
# Dear ImGui
# ============================================================

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)

FetchContent_MakeAvailable(imgui)

# ImGui does not provide a standard CMake target,
# so we create one ourselves.

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp

    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    glfw
)

# ============================================================
# Vendored dependencies
# ============================================================

add_subdirectory(spine-cpp)
add_subdirectory(spine-cpp38)
add_subdirectory(stb)
add_subdirectory(gif)

if(NOT EMSCRIPTEN)
    # glad exists for platforms whose system headers stop at GL 1.1. WebGL has
    # no dynamic loading - every entry point resolves against the JavaScript
    # library at link time - so there is nothing to load.
    add_subdirectory(glad)
endif()
