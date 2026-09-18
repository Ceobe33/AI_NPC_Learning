include(FetchContent)

# ============================================================
# GLFW
# ============================================================

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_MakeAvailable(glfw)


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
