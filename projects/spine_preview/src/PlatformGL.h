#pragma once

// OpenGL header selection for the three hosts this application builds for.
//
// Everything the renderer uses lives inside the GL 3.3 core profile / the
// GLES 3.0 feature set, but each platform exposes those entry points in its
// own way:
//
//   macOS        <OpenGL/gl3.h> declares all of it, nothing has to be loaded.
//   Emscripten   the web provides only GLES (WebGL 2), declared in
//                <GLES3/gl3.h>. There is no dynamic loading: every symbol the
//                linker keeps is resolved against the JS library at build
//                time, so unused GL functions cost nothing.
//   others       <GL/gl.h> only declares GL 1.1, so the core profile comes
//                from glad (third_party/glad) instead. glad replaces the
//                system header entirely - including both is a hard error,
//                which glad.h checks for. It has to be initialised once a
//                context is current, see main.cpp.
// True only where the GL entry points have to be resolved at run time. macOS
// declares them all and Emscripten binds them at link time, so this is the one
// host that needs a loader initialised after the context is made current.
#if !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
#define SPINE_STUDIO_NEEDS_GL_LOADER 1
#endif

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>

// WebGL 2 is GLES 3.0. It has no glPolygonMode, no glTexImage2D with internal
// formats other than sized ones, and it drops the desktop-only helpers. The
// renderer was written against the common subset, so no host-specific code
// beyond this header switch is needed.
#elif defined(__APPLE__)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#else
#include <glad/glad.h>
#endif
