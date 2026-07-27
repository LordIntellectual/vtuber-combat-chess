#include "../gl_compat.hxx"
#include "VictoryScreen.hxx"
#include <cstdio>
#include <cstring>
#include <algorithm>

#include "../../third_party/stb_easy_font.h"

VictoryScreen::VictoryScreen()
  : open(false), reason(END_NONE), whiteWon(true), replayRequested(false),
    btnX(0), btnY(0), btnW(280), btnH(48), lastW(1280), lastH(720) {}

void VictoryScreen::show(int endReason, bool wonWhite) {
  open = true;
  reason = endReason;
  whiteWon = wonWhite;
  replayRequested = false;
}

void VictoryScreen::hide() {
  open = false;
  replayRequested = false;
}

bool VictoryScreen::consumeReplay() {
  if (!replayRequested) return false;
  replayRequested = false;
  return true;
}

void VictoryScreen::layout(int screenW, int screenH) {
  lastW = screenW;
  lastH = screenH;
  btnW = 280.f;
  btnH = 48.f;
  btnX = (screenW - btnW) * 0.5f;
  btnY = screenH * 0.55f;
}

void VictoryScreen::drawRect(float x, float y, float w, float h,
                             float r, float g, float b, float a) {
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
}

void VictoryScreen::drawText(float x, float y, const char* text,
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

void VictoryScreen::draw(int screenW, int screenH) {
  if (!open) return;
  layout(screenW, screenH);

  glUseProgram(0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, screenW, screenH, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  // Dim board
  drawRect(0, 0, (float)screenW, (float)screenH, 0.f, 0.f, 0.f, 0.62f);

  const float panelW = 520.f;
  const float panelH = 260.f;
  const float panelX = (screenW - panelW) * 0.5f;
  const float panelY = screenH * 0.28f;

  drawRect(panelX, panelY, panelW, panelH, 0.05f, 0.07f, 0.12f, 0.96f);
  glColor4f(1.f, 0.85f, 0.25f, 0.95f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(panelX, panelY);
  glVertex2f(panelX + panelW, panelY);
  glVertex2f(panelX + panelW, panelY + panelH);
  glVertex2f(panelX, panelY + panelH);
  glEnd();

  const char* title = (reason == END_FORFEIT) ? "FORFEIT" : "CHECKMATE";
  // Rough centre for title
  float titleScale = 3.2f;
  float titleW = (float)strlen(title) * 7.f * titleScale;
  drawText(panelX + (panelW - titleW) * 0.5f, panelY + 28.f,
           title, 1.f, 0.9f, 0.3f, titleScale);

  char sub[96];
  if (reason == END_FORFEIT) {
    snprintf(sub, sizeof(sub), "%s wins — opponent has only a king left",
             whiteWon ? "White" : "Black");
  } else {
    snprintf(sub, sizeof(sub), "%s delivers checkmate",
             whiteWon ? "White" : "Black");
  }
  float subW = (float)strlen(sub) * 7.f * 1.4f;
  drawText(panelX + (panelW - subW) * 0.5f, panelY + 90.f,
           sub, 0.85f, 0.9f, 1.f, 1.4f);

  drawText(panelX + 40.f, panelY + 130.f,
           "Remaining enemy pieces explode in celebration!",
           0.55f, 0.65f, 0.75f, 1.15f);

  // Replay button
  drawRect(btnX, btnY, btnW, btnH, 0.15f, 0.45f, 0.25f, 1.f);
  glColor4f(0.4f, 1.f, 0.6f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(btnX, btnY);
  glVertex2f(btnX + btnW, btnY);
  glVertex2f(btnX + btnW, btnY + btnH);
  glVertex2f(btnX, btnY + btnH);
  glEnd();
  const char* replay = "REPLAY";
  float rw = (float)strlen(replay) * 7.f * 2.0f;
  drawText(btnX + (btnW - rw) * 0.5f, btnY + 14.f,
           replay, 0.9f, 1.f, 0.95f, 2.0f);

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

bool VictoryScreen::onMouseButton(int button, int action, float mx, float my) {
  if (!open) return false;
  if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return true;
  layout(lastW, lastH);
  if (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH) {
    replayRequested = true;
    return true;
  }
  return true; // consume all clicks while open
}
