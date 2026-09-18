# Vendored spine-cpp runtime, taken from the spine-runtimes 4.2 branch.
# https://github.com/EsotericSoftware/spine-runtimes (spine-cpp/spine-cpp)

file(GLOB SPINE_CPP_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/spine/*.cpp)

add_library(spine-cpp STATIC ${SPINE_CPP_SOURCES})

target_include_directories(spine-cpp PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

target_compile_features(spine-cpp PUBLIC cxx_std_11)

# Third party code: keep it quiet.
if(MSVC)
    target_compile_options(spine-cpp PRIVATE /w)
else()
    target_compile_options(spine-cpp PRIVATE -w)
endif()

# The runtime aborts through assert() on data it cannot parse. This build type
# does not define NDEBUG, so turn it on here: a malformed asset has to fail as
# a load error instead of taking the whole app down.
target_compile_options(spine-cpp PRIVATE $<$<NOT:$<CONFIG:Debug>>:-DNDEBUG>)
