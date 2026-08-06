#include "MainMenu.hxx"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>

#include "../../third_party/stb_easy_font.h"

MainMenu::MainMenu()
  : visible(false),
    page_(PAGE_ROOT),
    pending(ACTION_NONE),
    lastW(1280), lastH(720),
    hoverBtn(-1),
    joinFieldFocused(false),
    joinAddr("127.0.0.1:7777"),
    hostPortNum(7777),
    blinkT(0.f),
    panelX(0), panelY(0), panelW(480), panelH(420),
    buttonCount(0),
    fieldX(0), fieldY(0), fieldW(0), fieldH(0) {
  rebuild();
}

void MainMenu::show() {
  visible = true;
  page_ = PAGE_ROOT;
  pending = ACTION_NONE;
  joinFieldFocused = false;
  statusLine.clear();
  rebuild();
}

void MainMenu::hide() {
  visible = false;
  pending = ACTION_NONE;
  joinFieldFocused = false;
}

void MainMenu::rebuild() {
  buttonCount = 0;
  if (page_ == PAGE_ROOT) {
    buttons[buttonCount++] = {"Single Player", 0, 0, 0, 0, (int)ACTION_SINGLE_PLAYER};
    buttons[buttonCount++] = {"Multiplayer", 0, 0, 0, 0, ID_GOTO_MP};
    buttons[buttonCount++] = {"Settings", 0, 0, 0, 0, (int)ACTION_OPEN_SETTINGS};
    buttons[buttonCount++] = {"Quit", 0, 0, 0, 0, (int)ACTION_QUIT};
  } else {
    buttons[buttonCount++] = {"Host Game", 0, 0, 0, 0, (int)ACTION_HOST};
    buttons[buttonCount++] = {"Join Game", 0, 0, 0, 0, (int)ACTION_JOIN};
    buttons[buttonCount++] = {"Back", 0, 0, 0, 0, ID_BACK};
  }
}

void MainMenu::layout(int w, int h) {
  lastW = w;
  lastH = h;
  panelW = 520.f;
  panelH = (page_ == PAGE_MULTIPLAYER) ? 460.f : 420.f;
  panelX = (w - panelW) * 0.5f;
  panelY = (h - panelH) * 0.5f;

  const float btnW = 320.f;
  const float btnH = 48.f;
  const float gap = 16.f;
  float y = panelY + 100.f;
  if (page_ == PAGE_MULTIPLAYER) y = panelY + 90.f;

  for (int i = 0; i < buttonCount; ++i) {
    // On multiplayer page, insert field between Host and Join
    if (page_ == PAGE_MULTIPLAYER && i == 1) {
      fieldW = btnW;
      fieldH = 40.f;
      fieldX = panelX + (panelW - fieldW) * 0.5f;
      fieldY = y;
      y += fieldH + gap + 8.f;
    }
    buttons[i].w = btnW;
    buttons[i].h = btnH;
    buttons[i].x = panelX + (panelW - btnW) * 0.5f;
    buttons[i].y = y;
    y += btnH + gap;
  }
  if (page_ != PAGE_MULTIPLAYER) {
    fieldX = fieldY = fieldW = fieldH = 0;
  }
}

int MainMenu::hitButton(float mx, float my) const {
  for (int i = 0; i < buttonCount; ++i) {
    const Btn& b = buttons[i];
    if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h)
      return i;
  }
  return -1;
}

bool MainMenu::hitField(float mx, float my) const {
  if (page_ != PAGE_MULTIPLAYER || fieldW <= 0) return false;
  return mx >= fieldX && mx <= fieldX + fieldW &&
         my >= fieldY && my <= fieldY + fieldH;
}

void MainMenu::drawRect(float x, float y, float w, float h,
                        float r, float g, float b, float a) {
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
}

void MainMenu::drawText(float x, float y, const char* text,
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

void MainMenu::drawButton(const Btn& b, bool hover) {
  float br = hover ? 0.14f : 0.08f;
  float bg = hover ? 0.18f : 0.10f;
  float bb = hover ? 0.28f : 0.16f;
  if (b.id == (int)ACTION_QUIT) {
    br = hover ? 0.40f : 0.28f;
    bg = hover ? 0.12f : 0.08f;
    bb = hover ? 0.12f : 0.08f;
  }
  drawRect(b.x, b.y, b.w, b.h, br, bg, bb, 0.96f);
  glColor4f(0.45f, 0.85f, 1.f, hover ? 1.f : 0.75f);
  if (b.id == (int)ACTION_QUIT)
    glColor4f(1.f, 0.55f, 0.5f, hover ? 1.f : 0.8f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(b.x, b.y);
  glVertex2f(b.x + b.w, b.y);
  glVertex2f(b.x + b.w, b.y + b.h);
  glVertex2f(b.x, b.y + b.h);
  glEnd();

  // Center label roughly
  float tw = (float)std::strlen(b.label) * 8.5f * 1.55f;
  float tx = b.x + (b.w - tw) * 0.5f;
  if (tx < b.x + 12.f) tx = b.x + 12.f;
  drawText(tx, b.y + 15.f, b.label, 0.95f, 0.97f, 1.f, 1.55f);
}

void MainMenu::draw(int screenW, int screenH) {
  if (!visible) return;
  layout(screenW, screenH);
  blinkT += 0.016f;

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

  // Dim the 3D scene
  drawRect(0, 0, (float)screenW, (float)screenH, 0.02f, 0.03f, 0.07f, 0.72f);

  // Panel
  drawRect(panelX, panelY, panelW, panelH, 0.05f, 0.07f, 0.12f, 0.97f);
  glColor4f(0.35f, 0.75f, 1.f, 0.9f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(panelX, panelY);
  glVertex2f(panelX + panelW, panelY);
  glVertex2f(panelX + panelW, panelY + panelH);
  glVertex2f(panelX, panelY + panelH);
  glEnd();

  drawText(panelX + 28, panelY + 28, "vTuber Combat Chess", 0.55f, 0.9f, 1.f, 2.0f);

  if (page_ == PAGE_ROOT) {
    drawText(panelX + 28, panelY + 62, "Main Menu", 0.85f, 0.88f, 0.95f, 1.4f);
  } else {
    drawText(panelX + 28, panelY + 58, "Multiplayer", 0.85f, 0.88f, 0.95f, 1.4f);
    drawText(panelX + 28, panelY + 78,
             "Host = White  |  Guest = Black  |  Port 7777 default",
             0.65f, 0.7f, 0.8f, 1.1f);
  }

  for (int i = 0; i < buttonCount; ++i)
    drawButton(buttons[i], i == hoverBtn);

  // Join address field (between Host and Join on MP page)
  if (page_ == PAGE_MULTIPLAYER && fieldW > 0) {
    drawText(fieldX, fieldY - 18.f, "Join address (HOST:PORT)",
             0.7f, 0.75f, 0.85f, 1.15f);
    float fr = joinFieldFocused ? 0.12f : 0.06f;
    float fg = joinFieldFocused ? 0.16f : 0.08f;
    float fb = joinFieldFocused ? 0.24f : 0.12f;
    drawRect(fieldX, fieldY, fieldW, fieldH, fr, fg, fb, 1.f);
    glColor4f(joinFieldFocused ? 0.5f : 0.35f, 0.85f, 1.f, 1.f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(fieldX, fieldY);
    glVertex2f(fieldX + fieldW, fieldY);
    glVertex2f(fieldX + fieldW, fieldY + fieldH);
    glVertex2f(fieldX, fieldY + fieldH);
    glEnd();

    std::string shown = joinAddr;
    if (joinFieldFocused && ((int)(blinkT * 2.f) % 2 == 0))
      shown.push_back('|');
    drawText(fieldX + 12, fieldY + 12, shown.c_str(), 0.95f, 0.97f, 1.f, 1.35f);
  }

  if (!statusLine.empty()) {
    drawText(panelX + 28, panelY + panelH - 36, statusLine.c_str(),
             1.f, 0.75f, 0.4f, 1.2f);
  }

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_DEPTH_TEST);
}

bool MainMenu::onMouseMove(float mx, float my) {
  if (!visible) return false;
  hoverBtn = hitButton(mx, my);
  return true;
}

bool MainMenu::onMouseButton(int button, int action, float mx, float my) {
  if (!visible) return false;
  if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
    return true; // still consume while visible

  if (page_ == PAGE_MULTIPLAYER && hitField(mx, my)) {
    joinFieldFocused = true;
    return true;
  }
  joinFieldFocused = false;

  int hi = hitButton(mx, my);
  if (hi < 0) return true;
  int id = buttons[hi].id;

  if (id == ID_GOTO_MP) {
    page_ = PAGE_MULTIPLAYER;
    statusLine.clear();
    rebuild();
    return true;
  }
  if (id == ID_BACK) {
    page_ = PAGE_ROOT;
    statusLine.clear();
    joinFieldFocused = false;
    rebuild();
    return true;
  }
  if (id == (int)ACTION_SINGLE_PLAYER) pending = ACTION_SINGLE_PLAYER;
  else if (id == (int)ACTION_OPEN_SETTINGS) pending = ACTION_OPEN_SETTINGS;
  else if (id == (int)ACTION_HOST) pending = ACTION_HOST;
  else if (id == (int)ACTION_JOIN) pending = ACTION_JOIN;
  else if (id == (int)ACTION_QUIT) pending = ACTION_QUIT;
  return true;
}

bool MainMenu::onKey(int key, int action, int mods) {
  (void)mods;
  if (!visible) return false;
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;

  if (key == GLFW_KEY_ESCAPE) {
    if (page_ == PAGE_MULTIPLAYER) {
      page_ = PAGE_ROOT;
      joinFieldFocused = false;
      statusLine.clear();
      rebuild();
      return true;
    }
    // Root: Esc = Quit (same as Quit button)
    pending = ACTION_QUIT;
    return true;
  }

  if (joinFieldFocused && page_ == PAGE_MULTIPLAYER) {
    if (key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_DELETE) {
      if (!joinAddr.empty()) joinAddr.pop_back();
      return true;
    }
    if (key == GLFW_KEY_ENTER) {
      pending = ACTION_JOIN;
      return true;
    }
    if (key == GLFW_KEY_TAB) {
      joinFieldFocused = false;
      return true;
    }
  }

  // Number keys 1-3 as shortcuts on root
  if (page_ == PAGE_ROOT && action == GLFW_PRESS) {
    if (key == GLFW_KEY_1) { pending = ACTION_SINGLE_PLAYER; return true; }
    if (key == GLFW_KEY_2) {
      page_ = PAGE_MULTIPLAYER;
      rebuild();
      return true;
    }
    if (key == GLFW_KEY_3) { pending = ACTION_OPEN_SETTINGS; return true; }
  }
  return joinFieldFocused; // consume keys while typing
}

bool MainMenu::onChar(unsigned int codepoint) {
  if (!visible || !joinFieldFocused || page_ != PAGE_MULTIPLAYER)
    return false;
  if (codepoint < 32 || codepoint > 126) return false;
  char c = (char)codepoint;
  // Allow hostnames, IPv4, port
  if (std::isalnum((unsigned char)c) || c == '.' || c == ':' || c == '-' || c == '_') {
    if (joinAddr.size() < 64) joinAddr.push_back(c);
    return true;
  }
  return true; // consume other printable
}

MainMenu::Action MainMenu::consumeAction() {
  Action a = pending;
  pending = ACTION_NONE;
  return a;
}
