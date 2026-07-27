#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include "Hud.hxx"
#include "../PieceSet/PieceSet.hxx"
#include <cstdio>
#include <cstring>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../../third_party/stb_easy_font.h"

Hud::Hud() : visible(true), statusLine("Ready"), lastEvent("") {}

void Hud::drawPanel(float x, float y, float w, float h, float a) {
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  // Use deprecated fixed pipeline for HUD simplicity (compat profile)
  glBegin(GL_QUADS);
  glColor4f(0.02f, 0.02f, 0.06f, a);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}

void Hud::drawText(float x, float y, const char* text, float r, float g, float b, float scale) {
  char buffer[99999];
  int num_quads = stb_easy_font_print(0, 0, (char*)text, nullptr, buffer, sizeof(buffer));
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  // NDC-ish: we'll set ortho in draw()
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glTranslatef(x, y, 0);
  glScalef(scale, scale, 1);
  glColor3f(r, g, b);
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 16, buffer);
  glDrawArrays(GL_QUADS, 0, num_quads * 4);
  glDisableClientState(GL_VERTEX_ARRAY);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_DEPTH_TEST);
}

void Hud::draw(int width, int height, const ThemeManager& themes, bool aiOn, bool musicOn,
               const PieceSetManager* pieceSets) {
  // When hidden: tiny corner hint only so stream can go full board (H restores)
  if (!visible) {
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    char buf[99999];
    const char* hint = "H show UI";
    int nq = stb_easy_font_print(0, 0, (char*)hint, nullptr, buf, sizeof(buf));
    glColor4f(0.f, 0.f, 0.f, 0.45f);
    glBegin(GL_QUADS);
    glVertex2f(8, 8); glVertex2f(110, 8); glVertex2f(110, 28); glVertex2f(8, 28);
    glEnd();
    glPushMatrix();
    glTranslatef(14, 12, 0);
    glScalef(1.4f, 1.4f, 1);
    glColor3f(0.7f, 0.85f, 1.f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buf);
    glDrawArrays(GL_QUADS, 0, nq * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    return;
  }

  glUseProgram(0);
  glDisable(GL_CULL_FACE);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  // Top banner
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_QUADS);
  glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
  glVertex2f(0, 0);
  glVertex2f((float)width, 0);
  glVertex2f((float)width, 78);
  glVertex2f(0, 78);
  // Left control panel
  glColor4f(0.0f, 0.0f, 0.0f, 0.62f);
  glVertex2f(0, 78);
  glVertex2f(300, 78);
  glVertex2f(300, (float)height);
  glVertex2f(0, (float)height);
  glEnd();

  const Theme& th = themes.current();
  char line[256];

  auto text = [&](float x, float y, const char* t, float r=0.9f, float g=0.95f, float b=1.f, float s=1.6f) {
    char buf[99999];
    int nq = stb_easy_font_print(0, 0, (char*)t, nullptr, buf, sizeof(buf));
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(s, s, 1);
    glColor3f(r, g, b);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buf);
    glDrawArrays(GL_QUADS, 0, nq * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
  };

  text(12, 10, "VTUBER COMBAT CHESS", 0.4f, 1.f, 1.f, 2.0f);
  snprintf(line, sizeof(line), "Stage: %s", th.name);
  text(12, 36, line, 1.f, 0.5f, 0.9f, 1.5f);
  snprintf(line, sizeof(line), "%s", th.tagline);
  text(12, 54, line, 0.7f, 0.75f, 0.85f, 1.2f);

  text(12, 96, "CONTROLS", 1.f, 0.85f, 0.3f, 1.5f);
  text(12, 118, "1/2/3  Theme (Neon/Jungle/Ship)", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 134, "T      Cycle theme", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 150, "P      Cycle piece set", 0.4f, 1.f, 0.85f, 1.15f);
  text(12, 166, "S      Settings (Sound/Video/Pieces)", 0.4f, 1.f, 0.85f, 1.15f);
  text(12, 182, "H / U  Hide UI (stream view)", 0.4f, 1.f, 0.85f, 1.15f);
  text(12, 198, "A      Toggle AI (Stockfish)", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 214, "M      Toggle music", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 230, "F      FX intensity", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 246, "R      Reset board", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 262, "RMB    Orbit camera (360)", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 278, "Wheel  Zoom in / out", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 294, "LMB    Select / move piece", 0.85f, 0.9f, 1.f, 1.15f);
  text(12, 310, "Esc    Quit", 0.85f, 0.9f, 1.f, 1.15f);

  text(12, 344, "STATUS", 1.f, 0.85f, 0.3f, 1.5f);
  if (pieceSets) {
    snprintf(line, sizeof(line), "Pieces: %s", pieceSets->current().name.c_str());
    text(12, 366, line, 0.45f, 1.f, 0.8f, 1.25f);
  }
  float y0 = pieceSets ? 386.f : 366.f;
  snprintf(line, sizeof(line), "AI: %s", aiOn ? "ON (Stockfish)" : "OFF (human black)");
  text(12, y0, line, aiOn ? 0.4f : 1.f, aiOn ? 1.f : 0.5f, 0.5f, 1.25f);
  snprintf(line, sizeof(line), "Music: %s", musicOn ? "ON" : "OFF");
  text(12, y0 + 18.f, line, 0.85f, 0.9f, 1.f, 1.25f);
  snprintf(line, sizeof(line), "FX: %s", ThemeManager::fxName(themes.fx()));
  text(12, y0 + 36.f, line, 0.85f, 0.9f, 1.f, 1.25f);
  snprintf(line, sizeof(line), "State: %s", statusLine.c_str());
  text(12, y0 + 54.f, line, 0.9f, 0.95f, 0.7f, 1.25f);
  if (!lastEvent.empty()) {
    snprintf(line, sizeof(line), "Event: %s", lastEvent.c_str());
    text(12, y0 + 72.f, line, 1.f, 0.7f, 0.4f, 1.25f);
  }

  text(12, (float)height - 40, "vTuber Combat Chess — Lord Intellectual", 0.5f, 0.55f, 0.65f, 1.1f);

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}
