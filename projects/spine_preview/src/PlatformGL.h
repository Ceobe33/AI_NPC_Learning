#pragma once

// Minimal OpenGL 3.3 core header selection.
//
// Everything this project uses lives in the GL 3.3 core profile, but the two
// host platforms expose it very differently:
//
//   macOS   <OpenGL/gl3.h> declares all of it, nothing has to be loaded.
//   others  <GL/gl.h> only declares GL 1.1, so the core profile comes from
//           glad (third_party/glad) instead. glad replaces the system header
//           entirely - including both is a hard error, which glad.h checks
//           for. It has to be initialised once a context is current, see
//           main.cpp.
#if defined(__APPLE__)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#else
#include <glad/glad.h>
#endif
