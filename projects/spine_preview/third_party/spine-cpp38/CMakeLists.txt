# Vendored spine-cpp runtime (spine-runtimes 3.8 branch).
#
# The spine namespace has been renamed to spine38 (and the headers moved to
# include/spine38) so that this older runtime can be linked into the same
# binary as the 4.2 runtime, letting the app load both asset versions.

file(GLOB SPINE_CPP_38_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/spine/*.cpp)

add_library(spine-cpp38 STATIC ${SPINE_CPP_38_SOURCES})

target_include_directories(spine-cpp38 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# Vendored code: do not let its warnings drown out the project's own output.
if(MSVC)
    target_compile_options(spine-cpp38 PRIVATE /w)
else()
    target_compile_options(spine-cpp38 PRIVATE -w)
endif()

# The runtime aborts through assert() when it meets data it cannot parse. This
# build type does not define NDEBUG, so turn it on here: a malformed (or simply
# newer) asset must fail as a load error, never take the app down with it.
target_compile_options(spine-cpp38 PRIVATE $<$<NOT:$<CONFIG:Debug>>:-DNDEBUG>)
