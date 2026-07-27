#ifndef VCC_GL_COMPAT_HXX_
#define VCC_GL_COMPAT_HXX_

/*
 * Portable OpenGL + GLFW entry for Linux and Windows.
 * GLAD (OpenGL 3.3 compatibility) supplies modern entry points on Windows,
 * where the system gl.h is only OpenGL 1.1. Call vccInitGL() once after
 * glfwMakeContextCurrent().
 */

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>

/** Load GL function pointers via GLFW. Returns true on success. */
inline bool vccInitGL() {
  int ver = gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress));
  if (!ver) {
    std::fprintf(stderr, "[GL] gladLoadGL failed\n");
    return false;
  }
  std::printf("[GL] glad loaded (version report %d)\n", ver);
  return true;
}

#endif
