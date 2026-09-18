# stb single header libraries (https://github.com/nothings/stb), public domain.
# Only stb_image.h is vendored: it decodes the atlas PNG files.

add_library(stb INTERFACE)

target_include_directories(stb INTERFACE ${CMAKE_CURRENT_LIST_DIR})

add_library(stb_image INTERFACE)
target_link_libraries(stb_image INTERFACE stb)
