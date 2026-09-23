# GL 3.3 core loader for every platform whose system GL headers stop at 1.1.
#
# macOS already exposes the core profile through <OpenGL/gl3.h>, so the target
# is skipped there and PlatformGL.h keeps using the framework header.
#
# Regenerate with (glad 0.1.x, "--reproducible" keeps it offline by using the
# spec bundled with the package):
#   python -m glad --reproducible --generator=c --spec=gl --profile=core \
#          --api=gl=3.3 --extensions= --out-path=<dir>
# then copy include/glad/glad.h, include/KHR/khrplatform.h and src/glad.c here.
if(APPLE)
    return()
endif()

add_library(glad STATIC src/glad.c)

target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
