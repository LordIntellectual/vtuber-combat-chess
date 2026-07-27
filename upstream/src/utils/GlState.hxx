#ifndef NCA_GL_STATE_HXX_
#define NCA_GL_STATE_HXX_

#include "../gl_compat.hxx"

/* Full reset of GL state that smoke / sparks / cel / shadow passes may leave
   dirty. Call before any fixed-function or env-texture sky/sun draw. */
inline void ncaResetPipelineState() {
  glUseProgram(0);

  // Generic attributes + instancing (smoke / sparks)
  for (int i = 0; i < 8; i++) {
    glDisableVertexAttribArray(i);
    glVertexAttribDivisor(i, 0);
  }

  // Client arrays (mesh / HUD)
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // Multitexture units 0..3 — smoke binds TEXTURE0..2 and can leave unit 2
  // active; fixed-function then enables/binds on the wrong unit (purple void).
  for (int u = 3; u >= 0; u--) {
    glActiveTexture(GL_TEXTURE0 + u);
#ifdef GL_CLIENT_ACTIVE_TEXTURE
    glClientActiveTexture(GL_TEXTURE0 + u);
#endif
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  }
  glActiveTexture(GL_TEXTURE0);
#ifdef GL_CLIENT_ACTIVE_TEXTURE
  glClientActiveTexture(GL_TEXTURE0);
#endif

  glDisable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_TRUE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glDisable(GL_PROGRAM_POINT_SIZE);
  glColor4f(1.f, 1.f, 1.f, 1.f);

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
}

#endif
