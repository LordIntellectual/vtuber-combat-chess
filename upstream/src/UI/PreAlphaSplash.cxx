#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include "PreAlphaSplash.hxx"
#include <cstdio>
#include <cstring>
#include <algorithm>

#include "../../third_party/stb_easy_font.h"

PreAlphaSplash::PreAlphaSplash()
  : lastW(1280), lastH(720),
    panelX(0), panelY(0), panelW(560), panelH(280),
    okX(0), okY(0), okW(180), okH(44),
    quitX(0), quitY(0), quitW(140), quitH(44),
    result(RESULT_NONE) {}

void PreAlphaSplash::layout(int w, int h) {
  lastW = w;
  lastH = h;
  panelW = 580.f;
  panelH = 300.f;
  panelX = (w - panelW) * 0.5f;
  panelY = (h - panelH) * 0.5f;

  okW = 200.f;
  okH = 44.f;
  quitW = 140.f;
  quitH = 44.f;
  const float gap = 20.f;
  const float total = okW + gap + quitW;
  okX = panelX + (panelW - total) * 0.5f;
  quitX = okX + okW + gap;
  okY = panelY + panelH - 70.f;
  quitY = okY;
}

bool PreAlphaSplash::hit(float mx, float my, float x, float y, float w, float h) const {
  return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

void PreAlphaSplash::drawRect(float x, float y, float w, float h,
                              float r, float g, float b, float a) {
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
}

void PreAlphaSplash::drawText(float x, float y, const char* text,
                              float r, float g, float b, float scale) {
  char buf[99999];
  int nq = stb_easy_font_print(0, 0, (char*)text, nullptr, buf, sizeof(buf));
  glPushMatrix();
  glTranslatef(x, y, 0);
  glScalef(scale, scale, 1);
  glColor3f(r, g, b);
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 16, buf);
  glDrawArrays(GL_QUADS, 0, nq * 4);
  glDisableClientState(GL_VERTEX_ARRAY);
  glPopMatrix();
}

void PreAlphaSplash::draw() {
  glUseProgram(0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glViewport(0, 0, lastW, lastH);
  glClearColor(0.02f, 0.03f, 0.06f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, lastW, lastH, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Soft vignette panel
  drawRect(0, 0, (float)lastW, (float)lastH, 0.01f, 0.02f, 0.05f, 1.f);
  drawRect(panelX, panelY, panelW, panelH, 0.06f, 0.08f, 0.14f, 0.98f);

  glColor4f(1.f, 0.75f, 0.25f, 0.95f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(panelX, panelY);
  glVertex2f(panelX + panelW, panelY);
  glVertex2f(panelX + panelW, panelY + panelH);
  glVertex2f(panelX, panelY + panelH);
  glEnd();

  drawText(panelX + 28, panelY + 24, "PRE-ALPHA NOTICE", 1.f, 0.85f, 0.3f, 2.2f);

  // Message — split across lines for readability (stb_easy_font is monospace-ish)
  drawText(panelX + 28, panelY + 70,
           "vTuber Combat Chess is in Pre-Alpha.",
           0.95f, 0.95f, 1.f, 1.45f);
  drawText(panelX + 28, panelY + 100,
           "Menus, assets and sounds are not final.",
           0.85f, 0.88f, 0.95f, 1.35f);
  drawText(panelX + 28, panelY + 128,
           "Expect bugs.",
           0.85f, 0.88f, 0.95f, 1.35f);

  // I understand
  drawRect(okX, okY, okW, okH, 0.12f, 0.35f, 0.22f, 1.f);
  glColor4f(0.4f, 1.f, 0.65f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(okX, okY);
  glVertex2f(okX + okW, okY);
  glVertex2f(okX + okW, okY + okH);
  glVertex2f(okX, okY + okH);
  glEnd();
  drawText(okX + 28, okY + 13, "I understand", 0.9f, 1.f, 0.95f, 1.45f);

  // Quit
  drawRect(quitX, quitY, quitW, quitH, 0.35f, 0.12f, 0.12f, 1.f);
  glColor4f(1.f, 0.5f, 0.45f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(quitX, quitY);
  glVertex2f(quitX + quitW, quitY);
  glVertex2f(quitX + quitW, quitY + quitH);
  glVertex2f(quitX, quitY + quitH);
  glEnd();
  drawText(quitX + 45, quitY + 13, "Quit", 1.f, 0.9f, 0.9f, 1.45f);
}

PreAlphaSplash::Result PreAlphaSplash::run(GLFWwindow* window) {
  if (!window) return RESULT_QUIT;
  result = RESULT_NONE;

  // No game callbacks yet — poll input here so the player has no control
  // over the board. Music may already be playing from main().
  while (result == RESULT_NONE && !glfwWindowShouldClose(window)) {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (w < 1) w = 1280;
    if (h < 1) h = 720;
    layout(w, h);
    draw();
    glfwSwapBuffers(window);
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      // Esc = Quit on this screen
      result = RESULT_QUIT;
      break;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      double dx = 0, dy = 0;
      glfwGetCursorPos(window, &dx, &dy);
      // Framebuffer vs window coords (handle DPI scaling)
      int winW = 0, winH = 0;
      glfwGetWindowSize(window, &winW, &winH);
      float mx = (float)dx;
      float my = (float)dy;
      if (winW > 0 && winH > 0) {
        mx = (float)(dx * (double)w / (double)winW);
        my = (float)(dy * (double)h / (double)winH);
      }

      if (hit(mx, my, okX, okY, okW, okH)) {
        result = RESULT_UNDERSTAND;
        // Wait for release so the click does not fall through
        while (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
               !glfwWindowShouldClose(window)) {
          glfwPollEvents();
        }
        break;
      }
      if (hit(mx, my, quitX, quitY, quitW, quitH)) {
        result = RESULT_QUIT;
        while (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
               !glfwWindowShouldClose(window)) {
          glfwPollEvents();
        }
        break;
      }
    }
  }

  if (glfwWindowShouldClose(window) && result == RESULT_NONE)
    result = RESULT_QUIT;

  return result;
}
