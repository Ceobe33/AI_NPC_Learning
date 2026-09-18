#pragma once

// Minimal OpenGL 3.3 core header selection.
// On macOS the framework header already exposes the core profile entry points.
#if defined(__APPLE__)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
