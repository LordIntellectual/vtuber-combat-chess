#include "MainMenu.hxx"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <sstream>

#include "../../third_party/stb_easy_font.h"

MainMenu::MainMenu()
  : visible(false),
    hostDialogOpen_(false),
    page_(PAGE_ROOT),
    pending(ACTION_NONE),
    lastW(1280), lastH(720),
    hoverBtn(-1),
    hoverRoom(-1),
    hoverPopupBtn(-1),
    selectedRoom_(-1),
    focusField(0),
    hostName("Stream Match"),
    blinkT(0.f),
    panelX(0), panelY(0), panelW(640), panelH(520),
    buttonCount(0),
    popupButtonCount(0),
    jpX(0), jpY(0), jpW(0), jpH(0),
    listX(0), listY(0), listW(0), listH(0),
    rowH(28.f),
    dlgX(0), dlgY(0), dlgW(0), dlgH(0),
    hnX(0), hnY(0), hnW(0), hnH(0),
    hpX(0), hpY(0), hpW(0), hpH(0) {
  rebuild();
}

void MainMenu::show() {
  visible = true;
  page_ = PAGE_ROOT;
  pending = ACTION_NONE;
  focusField = 0;
  hostDialogOpen_ = false;
  statusLine.clear();
  rebuild();
}

void MainMenu::hide() {
  visible = false;
  pending = ACTION_NONE;
  focusField = 0;
  hostDialogOpen_ = false;
}

void MainMenu::openHostDialog() {
  hostDialogOpen_ = true;
  focusField = 1; // room name
  hoverPopupBtn = -1;
  if (hostName.empty()) hostName = "Stream Match";
}

void MainMenu::closeHostDialog() {
  hostDialogOpen_ = false;
  focusField = 0;
  hoverPopupBtn = -1;
}

void MainMenu::setRooms(const std::vector<LobbyRoom>& rooms) {
  rooms_ = rooms;
  if (selectedRoom_ >= (int)rooms_.size()) selectedRoom_ = -1;
}

const LobbyRoom* MainMenu::selectedRoom() const {
  if (selectedRoom_ < 0 || selectedRoom_ >= (int)rooms_.size()) return nullptr;
  return &rooms_[selectedRoom_];
}

void MainMenu::rebuild() {
  buttonCount = 0;
  popupButtonCount = 0;
  if (page_ == PAGE_ROOT) {
    buttons[buttonCount++] = {"Single Player", 0, 0, 0, 0, (int)ACTION_SINGLE_PLAYER};
    buttons[buttonCount++] = {"Multiplayer", 0, 0, 0, 0, ID_GOTO_MP};
    buttons[buttonCount++] = {"Settings", 0, 0, 0, 0, (int)ACTION_OPEN_SETTINGS};
    buttons[buttonCount++] = {"Quit", 0, 0, 0, 0, (int)ACTION_QUIT};
  } else {
    buttons[buttonCount++] = {"Refresh List", 0, 0, 0, 0, ID_REFRESH};
    buttons[buttonCount++] = {"Host Online Room", 0, 0, 0, 0, ID_OPEN_HOST_DIALOG};
    buttons[buttonCount++] = {"Join Selected", 0, 0, 0, 0, (int)ACTION_JOIN_ONLINE};
    buttons[buttonCount++] = {"Back", 0, 0, 0, 0, ID_BACK};
  }
  // Popup buttons rebuilt in layout
  popupButtons[0] = {"Create", 0, 0, 0, 0, ID_HOST_CREATE};
  popupButtons[1] = {"Back", 0, 0, 0, 0, ID_HOST_DIALOG_BACK};
  popupButtonCount = 2;
}

void MainMenu::layout(int w, int h) {
  lastW = w;
  lastH = h;
  if (page_ == PAGE_ROOT) {
    panelW = 520.f;
    panelH = 420.f;
  } else {
    panelW = std::min(720.f, (float)w - 40.f);
    panelH = std::min(520.f, (float)h - 40.f);
  }
  panelX = (w - panelW) * 0.5f;
  panelY = (h - panelH) * 0.5f;

  if (page_ == PAGE_ROOT) {
    const float btnW = 320.f, btnH = 48.f, gap = 16.f;
    float y = panelY + 100.f;
    for (int i = 0; i < buttonCount; ++i) {
      buttons[i].w = btnW;
      buttons[i].h = btnH;
      buttons[i].x = panelX + (panelW - btnW) * 0.5f;
      buttons[i].y = y;
      y += btnH + gap;
    }
    return;
  }

  // Multiplayer: list + join password + action buttons (no host fields)
  float y = panelY + 70.f;
  listX = panelX + 24.f;
  listW = panelW - 48.f;
  listY = y;
  listH = 200.f;
  y = listY + listH + 16.f;

  jpX = listX;
  jpY = y;
  jpW = listW * 0.55f;
  jpH = 32.f;
  y += 48.f;

  const float btnW = 200.f, btnH = 40.f, gap = 10.f;
  float bx = listX;
  float by = y;
  for (int i = 0; i < buttonCount; ++i) {
    buttons[i].w = btnW;
    buttons[i].h = btnH;
    if (bx + btnW > listX + listW + 1.f) {
      bx = listX;
      by += btnH + gap;
    }
    buttons[i].x = bx;
    buttons[i].y = by;
    bx += btnW + gap;
  }

  // Host dialog centered on screen
  dlgW = 440.f;
  dlgH = 280.f;
  dlgX = (w - dlgW) * 0.5f;
  dlgY = (h - dlgH) * 0.5f;

  hnX = dlgX + 28.f;
  hnY = dlgY + 88.f;
  hnW = dlgW - 56.f;
  hnH = 34.f;

  hpX = dlgX + 28.f;
  hpY = dlgY + 150.f;
  hpW = dlgW - 56.f;
  hpH = 34.f;

  float pbW = 150.f, pbH = 40.f;
  float gapB = 16.f;
  float total = pbW * 2 + gapB;
  float startX = dlgX + (dlgW - total) * 0.5f;
  float pby = dlgY + dlgH - 58.f;
  popupButtons[0].w = pbW;
  popupButtons[0].h = pbH;
  popupButtons[0].x = startX;
  popupButtons[0].y = pby;
  popupButtons[1].w = pbW;
  popupButtons[1].h = pbH;
  popupButtons[1].x = startX + pbW + gapB;
  popupButtons[1].y = pby;
}

int MainMenu::hitButton(float mx, float my) const {
  for (int i = 0; i < buttonCount; ++i) {
    const Btn& b = buttons[i];
    if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h)
      return i;
  }
  return -1;
}

int MainMenu::hitPopupButton(float mx, float my) const {
  for (int i = 0; i < popupButtonCount; ++i) {
    const Btn& b = popupButtons[i];
    if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h)
      return i;
  }
  return -1;
}

int MainMenu::hitRoom(float mx, float my) const {
  if (page_ != PAGE_MULTIPLAYER || hostDialogOpen_) return -1;
  if (mx < listX || mx > listX + listW || my < listY || my > listY + listH)
    return -1;
  int idx = (int)((my - listY - 4.f) / rowH);
  if (idx < 0 || idx >= (int)rooms_.size()) return -1;
  return idx;
}

int MainMenu::hitField(float mx, float my) const {
  auto hit = [&](float x, float y, float w, float h) {
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
  };
  if (hostDialogOpen_) {
    if (hit(hnX, hnY, hnW, hnH)) return 1;
    if (hit(hpX, hpY, hpW, hpH)) return 2;
    return 0;
  }
  if (page_ == PAGE_MULTIPLAYER) {
    if (hit(jpX, jpY, jpW, jpH)) return 3;
  }
  return 0;
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
  if (b.id == (int)ACTION_QUIT || b.id == ID_HOST_DIALOG_BACK) {
    if (b.id == (int)ACTION_QUIT) {
      br = hover ? 0.40f : 0.28f;
      bg = hover ? 0.12f : 0.08f;
      bb = hover ? 0.12f : 0.08f;
    }
  }
  if (b.id == ID_HOST_CREATE) {
    br = hover ? 0.12f : 0.08f;
    bg = hover ? 0.32f : 0.20f;
    bb = hover ? 0.22f : 0.14f;
  }
  drawRect(b.x, b.y, b.w, b.h, br, bg, bb, 0.96f);
  glColor4f(0.45f, 0.85f, 1.f, hover ? 1.f : 0.75f);
  if (b.id == ID_HOST_CREATE)
    glColor4f(0.5f, 1.f, 0.7f, hover ? 1.f : 0.85f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(b.x, b.y);
  glVertex2f(b.x + b.w, b.y);
  glVertex2f(b.x + b.w, b.y + b.h);
  glVertex2f(b.x, b.y + b.h);
  glEnd();
  drawText(b.x + 14.f, b.y + 12.f, b.label, 0.95f, 0.97f, 1.f, 1.35f);
}

void MainMenu::drawField(float x, float y, float w, float h, const std::string& text,
                         bool focused, const char* placeholder) {
  drawRect(x, y, w, h, focused ? 0.12f : 0.06f, focused ? 0.16f : 0.08f,
           focused ? 0.24f : 0.12f, 1.f);
  glColor4f(focused ? 0.5f : 0.35f, 0.85f, 1.f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
  std::string shown = text;
  if (shown.empty() && !focused && placeholder)
    drawText(x + 8, y + 9, placeholder, 0.5f, 0.55f, 0.6f, 1.2f);
  else {
    if (focused && ((int)(blinkT * 2.f) % 2 == 0)) shown.push_back('|');
    drawText(x + 8, y + 9, shown.c_str(), 0.95f, 0.97f, 1.f, 1.25f);
  }
}

void MainMenu::drawHostDialog() {
  // Dim multiplayer panel further
  drawRect(0, 0, (float)lastW, (float)lastH, 0.0f, 0.0f, 0.0f, 0.45f);

  drawRect(dlgX, dlgY, dlgW, dlgH, 0.06f, 0.08f, 0.14f, 0.98f);
  glColor4f(0.4f, 0.85f, 1.f, 0.95f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(dlgX, dlgY);
  glVertex2f(dlgX + dlgW, dlgY);
  glVertex2f(dlgX + dlgW, dlgY + dlgH);
  glVertex2f(dlgX, dlgY + dlgH);
  glEnd();

  drawText(dlgX + 28, dlgY + 24, "Host Online Room", 0.55f, 0.9f, 1.f, 1.8f);
  drawText(dlgX + 28, dlgY + 54, "Choose a name others will see in the list.",
           0.7f, 0.75f, 0.82f, 1.15f);

  drawText(hnX, hnY - 16, "Room name", 0.75f, 0.8f, 0.9f, 1.15f);
  drawField(hnX, hnY, hnW, hnH, hostName, focusField == 1, "Stream Match");

  drawText(hpX, hpY - 16, "Password (optional)", 0.75f, 0.8f, 0.9f, 1.15f);
  drawField(hpX, hpY, hpW, hpH, hostPass, focusField == 2, "leave empty for open room");

  for (int i = 0; i < popupButtonCount; ++i)
    drawButton(popupButtons[i], i == hoverPopupBtn);
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

  drawRect(0, 0, (float)screenW, (float)screenH, 0.02f, 0.03f, 0.07f, 0.72f);
  drawRect(panelX, panelY, panelW, panelH, 0.05f, 0.07f, 0.12f, 0.97f);
  glColor4f(0.35f, 0.75f, 1.f, 0.9f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(panelX, panelY);
  glVertex2f(panelX + panelW, panelY);
  glVertex2f(panelX + panelW, panelY + panelH);
  glVertex2f(panelX, panelY + panelH);
  glEnd();

  drawText(panelX + 24, panelY + 22, "vTuber Combat Chess", 0.55f, 0.9f, 1.f, 2.0f);

  if (page_ == PAGE_ROOT) {
    drawText(panelX + 24, panelY + 56, "Main Menu", 0.85f, 0.88f, 0.95f, 1.4f);
    for (int i = 0; i < buttonCount; ++i)
      drawButton(buttons[i], i == hoverBtn);
  } else {
    drawText(panelX + 24, panelY + 52, "Online Multiplayer",
             0.85f, 0.88f, 0.95f, 1.35f);

    drawRect(listX, listY, listW, listH, 0.03f, 0.04f, 0.07f, 1.f);
    glColor4f(0.3f, 0.5f, 0.7f, 0.9f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(listX, listY);
    glVertex2f(listX + listW, listY);
    glVertex2f(listX + listW, listY + listH);
    glVertex2f(listX, listY + listH);
    glEnd();

    if (rooms_.empty()) {
      drawText(listX + 12, listY + 20, "No rooms yet — Host Online Room, or Refresh.",
               0.6f, 0.65f, 0.7f, 1.2f);
    } else {
      int maxRows = (int)(listH / rowH);
      for (int i = 0; i < (int)rooms_.size() && i < maxRows; ++i) {
        float ry = listY + 4.f + i * rowH;
        bool sel = (i == selectedRoom_);
        bool hov = (i == hoverRoom);
        if (sel)
          drawRect(listX + 2, ry, listW - 4, rowH - 2, 0.12f, 0.22f, 0.35f, 1.f);
        else if (hov)
          drawRect(listX + 2, ry, listW - 4, rowH - 2, 0.08f, 0.12f, 0.18f, 1.f);
        const LobbyRoom& r = rooms_[i];
        std::ostringstream line;
        line << r.name << "  [" << r.players << "/" << r.maxPlayers << "]";
        if (r.hasPassword) line << "  (pw)";
        if (r.full) line << "  FULL";
        drawText(listX + 10, ry + 6, line.str().c_str(),
                 r.full ? 0.6f : 0.95f, 0.95f, 1.f, 1.15f);
      }
    }

    drawText(jpX, jpY - 16, "Join password (if room is locked)", 0.7f, 0.75f, 0.85f, 1.1f);
    drawField(jpX, jpY, jpW, jpH, joinPass, focusField == 3 && !hostDialogOpen_,
              "optional");

    for (int i = 0; i < buttonCount; ++i)
      drawButton(buttons[i], !hostDialogOpen_ && i == hoverBtn);

    if (hostDialogOpen_)
      drawHostDialog();
  }

  if (!statusLine.empty() && !hostDialogOpen_) {
    drawText(panelX + 24, panelY + panelH - 28, statusLine.c_str(),
             1.f, 0.75f, 0.4f, 1.15f);
  }

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_DEPTH_TEST);
}

bool MainMenu::onMouseMove(float mx, float my) {
  if (!visible) return false;
  if (hostDialogOpen_) {
    hoverPopupBtn = hitPopupButton(mx, my);
    hoverBtn = -1;
    hoverRoom = -1;
    return true;
  }
  hoverPopupBtn = -1;
  hoverBtn = hitButton(mx, my);
  hoverRoom = hitRoom(mx, my);
  return true;
}

bool MainMenu::onMouseButton(int button, int action, float mx, float my) {
  if (!visible) return false;
  if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
    return true;

  // Host dialog captures all clicks
  if (hostDialogOpen_) {
    int f = hitField(mx, my);
    if (f == 1 || f == 2) {
      focusField = f;
      return true;
    }
    int pi = hitPopupButton(mx, my);
    if (pi < 0) return true;
    int id = popupButtons[pi].id;
    if (id == ID_HOST_DIALOG_BACK) {
      closeHostDialog();
      return true;
    }
    if (id == ID_HOST_CREATE) {
      if (hostName.empty()) hostName = "Stream Match";
      closeHostDialog();
      pending = ACTION_HOST_ONLINE;
      return true;
    }
    return true;
  }

  if (page_ == PAGE_MULTIPLAYER) {
    int ri = hitRoom(mx, my);
    if (ri >= 0) {
      selectedRoom_ = ri;
      return true;
    }
    int f = hitField(mx, my);
    if (f == 3) {
      focusField = 3;
      return true;
    }
    focusField = 0;
  }

  int hi = hitButton(mx, my);
  if (hi < 0) return true;
  int id = buttons[hi].id;

  if (id == ID_GOTO_MP) {
    page_ = PAGE_MULTIPLAYER;
    statusLine.clear();
    closeHostDialog();
    rebuild();
    pending = ACTION_REFRESH_ROOMS;
    return true;
  }
  if (id == ID_BACK) {
    page_ = PAGE_ROOT;
    statusLine.clear();
    closeHostDialog();
    rebuild();
    return true;
  }
  if (id == ID_REFRESH) {
    pending = ACTION_REFRESH_ROOMS;
    return true;
  }
  if (id == ID_OPEN_HOST_DIALOG) {
    openHostDialog();
    return true;
  }
  if (id == (int)ACTION_SINGLE_PLAYER) pending = ACTION_SINGLE_PLAYER;
  else if (id == (int)ACTION_OPEN_SETTINGS) pending = ACTION_OPEN_SETTINGS;
  else if (id == (int)ACTION_JOIN_ONLINE) pending = ACTION_JOIN_ONLINE;
  else if (id == (int)ACTION_QUIT) pending = ACTION_QUIT;
  return true;
}

bool MainMenu::onKey(int key, int action, int mods) {
  (void)mods;
  if (!visible) return false;
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;

  if (key == GLFW_KEY_ESCAPE) {
    if (hostDialogOpen_) {
      closeHostDialog();
      return true;
    }
    if (page_ == PAGE_MULTIPLAYER) {
      page_ = PAGE_ROOT;
      focusField = 0;
      statusLine.clear();
      rebuild();
      return true;
    }
    pending = ACTION_QUIT;
    return true;
  }

  if (hostDialogOpen_) {
    std::string* target = nullptr;
    if (focusField == 1) target = &hostName;
    else if (focusField == 2) target = &hostPass;
    else {
      focusField = 1;
      target = &hostName;
    }
    if (key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_DELETE) {
      if (target && !target->empty()) target->pop_back();
      return true;
    }
    if (key == GLFW_KEY_ENTER) {
      if (hostName.empty()) hostName = "Stream Match";
      closeHostDialog();
      pending = ACTION_HOST_ONLINE;
      return true;
    }
    if (key == GLFW_KEY_TAB) {
      focusField = (focusField == 1) ? 2 : 1;
      return true;
    }
    return true;
  }

  if (focusField == 3 && page_ == PAGE_MULTIPLAYER) {
    if (key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_DELETE) {
      if (!joinPass.empty()) joinPass.pop_back();
      return true;
    }
    if (key == GLFW_KEY_ENTER) {
      pending = ACTION_JOIN_ONLINE;
      return true;
    }
  }

  if (page_ == PAGE_ROOT && action == GLFW_PRESS) {
    if (key == GLFW_KEY_1) { pending = ACTION_SINGLE_PLAYER; return true; }
    if (key == GLFW_KEY_2) {
      page_ = PAGE_MULTIPLAYER;
      rebuild();
      pending = ACTION_REFRESH_ROOMS;
      return true;
    }
    if (key == GLFW_KEY_3) { pending = ACTION_OPEN_SETTINGS; return true; }
  }
  return focusField > 0;
}

bool MainMenu::onChar(unsigned int codepoint) {
  if (!visible) return false;
  if (codepoint < 32 || codepoint > 126) return false;
  char c = (char)codepoint;

  if (hostDialogOpen_) {
    std::string* target = nullptr;
    if (focusField == 1) target = &hostName;
    else if (focusField == 2) target = &hostPass;
    else return true;
    if (target->size() >= 48) return true;
    if (focusField == 1) {
      if (std::isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' ||
          c == '.' || c == '\'')
        target->push_back(c);
    } else if (c != ' ') {
      target->push_back(c);
    }
    return true;
  }

  if (focusField == 3 && page_ == PAGE_MULTIPLAYER) {
    if (joinPass.size() < 48 && c != ' ') joinPass.push_back(c);
    return true;
  }
  return false;
}

MainMenu::Action MainMenu::consumeAction() {
  Action a = pending;
  pending = ACTION_NONE;
  return a;
}
