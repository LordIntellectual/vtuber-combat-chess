#include "../gl_compat.hxx"
#include "SettingsMenu.hxx"
#include "PieceEditor.hxx"
#include "../Audio/AudioEngine.hxx"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "../../third_party/stb_easy_font.h"

// UI 0..1 → model-space outline extrusion (works for simple + complex meshes)
static const float kOutlineMin = 0.0f;
static const float kOutlineMax = 0.22f;

SettingsMenu::SettingsMenu()
  : open(false), clickConsumed(false), dragIndex(-1),
    lastW(1280), lastH(720), page(PAGE_ROOT),
    quitConfirmOpen(false), quitRequested(false),
    quitPanelX(0), quitPanelY(0), quitPanelW(420), quitPanelH(180),
    quitYesX(0), quitYesY(0), quitYesW(140), quitYesH(40),
    quitNoX(0), quitNoY(0), quitNoW(140), quitNoH(40),
    editSlider(-1), editLen(0), editBlink(0.f),
    master(0.9f), music(0.45f), sfx(0.7f), outline(0.15f),
    // White = red, Black = purple (project palette / multiplayer readability)
    outWhiteR(0.95f), outWhiteG(0.12f), outWhiteB(0.12f),
    outBlackR(0.62f), outBlackG(0.18f), outBlackB(0.92f),
    actionCamera(true),
    suggestedMoves(false),
    explosionForceUI(0.5f),
    audioEngine(nullptr), pieceEditor(nullptr), sliderCount(0), buttonCount(0) {
  editBuf[0] = '\0';
  rebuildPage();
}

bool SettingsMenu::consumeQuitRequest() {
  if (!quitRequested) return false;
  quitRequested = false;
  return true;
}

bool SettingsMenu::pieceEditorOpen() const {
  return pieceEditor && pieceEditor->isOpen();
}

float SettingsMenu::outlineFactor() const {
  float t = std::max(0.f, std::min(1.f, outline));
  return kOutlineMin + t * (kOutlineMax - kOutlineMin);
}

void SettingsMenu::syncFromAudio() {
  if (!audioEngine) return;
  master = audioEngine->masterVolume();
  music = audioEngine->musicVolume();
  sfx = audioEngine->sfxVolume();
}

void SettingsMenu::applyToAudio() {
  if (!audioEngine) return;
  audioEngine->setMasterVolume(master);
  audioEngine->setMusicVolume(music);
  audioEngine->setSfxVolume(sfx);
}

float SettingsMenu::panelHeightForPage() const {
  if (page == PAGE_SOUND) return 320.f;
  // thickness + 6 RGB sliders + action cam + reset colours + back
  if (page == PAGE_VIDEO) return 680.f;
  if (page == PAGE_GAMEPLAY) return 320.f;
  if (page == PAGE_ROOT) return 420.f; // + Return / Quit
  return 300.f;
}

void SettingsMenu::rebuildPage() {
  cancelEdit();
  quitConfirmOpen = false;
  sliderCount = 0;
  buttonCount = 0;
  if (page == PAGE_ROOT) {
    buttons[buttonCount++] = {"Sound", 0, 0, 0, 0, PAGE_SOUND};
    buttons[buttonCount++] = {"Video", 0, 0, 0, 0, PAGE_VIDEO};
    buttons[buttonCount++] = {"Gameplay", 0, 0, 0, 0, PAGE_GAMEPLAY};
    buttons[buttonCount++] = {"Piece editor", 0, 0, 0, 0, PAGE_PIECE_EDITOR};
    buttons[buttonCount++] = {"Return", 0, 0, 0, 0, -10}; // close menu
    buttons[buttonCount++] = {"Quit", 0, 0, 0, 0, -11};   // confirm exit
  } else if (page == PAGE_SOUND) {
    // percent 0–100 → value 0..1
    sliders[sliderCount++] = {"Master volume", &master, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    sliders[sliderCount++] = {"Music volume", &music, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    sliders[sliderCount++] = {"Sound effects", &sfx, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    buttons[buttonCount++] = {"< Back", 0, 0, 0, 0, -1};
  } else if (page == PAGE_VIDEO) {
    sliders[sliderCount++] = {"Outline thickness", &outline, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    sliders[sliderCount++] = {"White outline R", &outWhiteR, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    sliders[sliderCount++] = {"White outline G", &outWhiteG, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    sliders[sliderCount++] = {"White outline B", &outWhiteB, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    sliders[sliderCount++] = {"Black outline R", &outBlackR, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    sliders[sliderCount++] = {"Black outline G", &outBlackG, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    sliders[sliderCount++] = {"Black outline B", &outBlackB, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    buttons[buttonCount++] = {
      actionCamera ? "Action camera: ON" : "Action camera: OFF",
      0, 0, 0, 0, -2};
    buttons[buttonCount++] = {"Reset outline colours", 0, 0, 0, 0, -4};
    buttons[buttonCount++] = {"< Back", 0, 0, 0, 0, -1};
  } else if (page == PAGE_GAMEPLAY) {
    sliders[sliderCount++] = {"Explosion force", &explosionForceUI, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 100};
    buttons[buttonCount++] = {
      suggestedMoves ? "Suggested moves: ON" : "Suggested moves: OFF",
      0, 0, 0, 0, -3};
    buttons[buttonCount++] = {"< Back", 0, 0, 0, 0, -1};
  }
}

void SettingsMenu::openMenu() {
  open = true;
  dragIndex = -1;
  cancelEdit();
  quitConfirmOpen = false;
  page = PAGE_ROOT;
  syncFromAudio();
  rebuildPage();
}

void SettingsMenu::closeMenu() {
  if (pieceEditor && pieceEditor->isOpen()) pieceEditor->closeEditor();
  commitEdit();
  open = false;
  dragIndex = -1;
  quitConfirmOpen = false;
  page = PAGE_ROOT;
  applyToAudio();
  rebuildPage();
}

void SettingsMenu::toggle() {
  if (open) closeMenu();
  else openMenu();
}

bool SettingsMenu::handleBack() {
  if (pieceEditor && pieceEditor->isOpen()) {
    if (pieceEditor->isEditingValue()) {
      pieceEditor->onKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
      return true;
    }
    bool still = pieceEditor->handleBack();
    if (!still) {
      open = true;
      page = PAGE_ROOT;
      rebuildPage();
      return true;
    }
    return true;
  }
  if (!open) return false;
  if (quitConfirmOpen) {
    quitConfirmOpen = false;
    return true; // stay on settings root
  }
  if (editSlider >= 0) {
    cancelEdit();
    return true;
  }
  if (page != PAGE_ROOT) {
    page = PAGE_ROOT;
    dragIndex = -1;
    rebuildPage();
    return true;
  }
  closeMenu();
  return false;
}

void SettingsMenu::layoutQuitConfirm(int screenW, int screenH) {
  quitPanelW = 440.f;
  quitPanelH = 200.f;
  quitPanelX = (screenW - quitPanelW) * 0.5f;
  quitPanelY = (screenH - quitPanelH) * 0.5f;
  quitYesW = 150.f;
  quitYesH = 42.f;
  quitNoW = 150.f;
  quitNoH = 42.f;
  const float gap = 24.f;
  const float total = quitYesW + gap + quitNoW;
  quitYesX = quitPanelX + (quitPanelW - total) * 0.5f;
  quitNoX = quitYesX + quitYesW + gap;
  quitYesY = quitPanelY + quitPanelH - 62.f;
  quitNoY = quitYesY;
}

void SettingsMenu::layout(int screenW, int screenH) {
  lastW = screenW;
  lastH = screenH;

  const float panelW = 440.f;
  const float panelH = panelHeightForPage();

  const float panelX = (screenW - panelW) * 0.5f;
  const float panelY = (screenH - panelH) * 0.5f;
  const float trackX = panelX + 40.f;
  const float trackW = panelW - 80.f;
  const float trackH = 18.f;
  const float numW = 72.f;
  const float numH = 22.f;

  float y = panelY + 88.f;
  for (int i = 0; i < sliderCount; i++) {
    sliders[i].x = trackX;
    sliders[i].y = y;
    sliders[i].w = trackW;
    sliders[i].h = trackH;
    // Value box on the right of the label row (above the track)
    sliders[i].numW = numW;
    sliders[i].numH = numH;
    sliders[i].numX = trackX + trackW - numW;
    sliders[i].numY = y - 26.f;
    y += 56.f;
  }

  if (page == PAGE_ROOT) y = panelY + 90.f;
  for (int i = 0; i < buttonCount; i++) {
    buttons[i].x = panelX + 40.f;
    buttons[i].y = y;
    buttons[i].w = panelW - 80.f;
    buttons[i].h = 40.f;
    y += 52.f;
  }

  layoutQuitConfirm(screenW, screenH);
}

bool SettingsMenu::hitQuitYes(float mx, float my) const {
  return mx >= quitYesX && mx <= quitYesX + quitYesW &&
         my >= quitYesY && my <= quitYesY + quitYesH;
}

bool SettingsMenu::hitQuitNo(float mx, float my) const {
  return mx >= quitNoX && mx <= quitNoX + quitNoW &&
         my >= quitNoY && my <= quitNoY + quitNoH;
}

void SettingsMenu::drawQuitConfirm() {
  // Dim over the whole screen
  drawRect(0, 0, (float)lastW, (float)lastH, 0.f, 0.f, 0.f, 0.55f);
  drawRect(quitPanelX, quitPanelY, quitPanelW, quitPanelH, 0.08f, 0.09f, 0.14f, 0.98f);

  glColor4f(1.f, 0.45f, 0.35f, 0.95f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(quitPanelX, quitPanelY);
  glVertex2f(quitPanelX + quitPanelW, quitPanelY);
  glVertex2f(quitPanelX + quitPanelW, quitPanelY + quitPanelH);
  glVertex2f(quitPanelX, quitPanelY + quitPanelH);
  glEnd();

  drawText(quitPanelX + 28, quitPanelY + 28, "QUIT", 1.f, 0.55f, 0.4f, 2.0f);
  drawText(quitPanelX + 28, quitPanelY + 72,
           "Are you sure you want to quit?",
           0.9f, 0.92f, 1.f, 1.35f);

  // Yes (destructive)
  drawRect(quitYesX, quitYesY, quitYesW, quitYesH, 0.45f, 0.15f, 0.15f, 1.f);
  glColor4f(1.f, 0.5f, 0.45f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(quitYesX, quitYesY);
  glVertex2f(quitYesX + quitYesW, quitYesY);
  glVertex2f(quitYesX + quitYesW, quitYesY + quitYesH);
  glVertex2f(quitYesX, quitYesY + quitYesH);
  glEnd();
  drawText(quitYesX + 50.f, quitYesY + 12.f, "Yes", 1.f, 0.9f, 0.9f, 1.6f);

  // No (safe)
  drawRect(quitNoX, quitNoY, quitNoW, quitNoH, 0.12f, 0.28f, 0.2f, 1.f);
  glColor4f(0.4f, 1.f, 0.65f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(quitNoX, quitNoY);
  glVertex2f(quitNoX + quitNoW, quitNoY);
  glVertex2f(quitNoX + quitNoW, quitNoY + quitNoH);
  glVertex2f(quitNoX, quitNoY + quitNoH);
  glEnd();
  drawText(quitNoX + 55.f, quitNoY + 12.f, "No", 0.9f, 1.f, 0.95f, 1.6f);
}

void SettingsMenu::drawRect(float x, float y, float w, float h,
                            float r, float g, float b, float a) {
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
}

void SettingsMenu::drawText(float x, float y, const char* text,
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

void SettingsMenu::beginEdit(int index) {
  if (index < 0 || index >= sliderCount) return;
  commitEdit(); // finish any previous field
  editSlider = index;
  editLen = 0;
  editBuf[0] = '\0';
  editBlink = 0.f;
  const Slider& s = sliders[index];
  float v = std::max(0.f, std::min(1.f, *s.value));
  int disp = s.percent ? (int)(v * 100.f + 0.5f) : (int)(v + 0.5f);
  disp = std::max(s.minInt, std::min(s.maxInt, disp));
  editLen = snprintf(editBuf, sizeof(editBuf), "%d", disp);
  if (editLen < 0) editLen = 0;
  if (editLen >= (int)sizeof(editBuf)) editLen = (int)sizeof(editBuf) - 1;
}

void SettingsMenu::cancelEdit() {
  editSlider = -1;
  editLen = 0;
  editBuf[0] = '\0';
}

void SettingsMenu::applySliderSideEffects(int index) {
  (void)index;
  if (page == PAGE_SOUND) applyToAudio();
}

void SettingsMenu::commitEdit() {
  if (editSlider < 0 || editSlider >= sliderCount) {
    cancelEdit();
    return;
  }
  Slider& s = sliders[editSlider];
  // Empty / invalid → keep previous
  if (editLen <= 0) {
    cancelEdit();
    return;
  }
  char* end = nullptr;
  long n = std::strtol(editBuf, &end, 10);
  if (end == editBuf) {
    cancelEdit();
    return;
  }
  if (n < s.minInt) n = s.minInt;
  if (n > s.maxInt) n = s.maxInt;
  if (s.percent)
    *s.value = (float)n / 100.f;
  else
    *s.value = (float)n;
  *s.value = std::max(0.f, std::min(1.f, *s.value));
  applySliderSideEffects(editSlider);
  cancelEdit();
}

void SettingsMenu::draw(int screenW, int screenH) {
  if (pieceEditor && pieceEditor->isOpen()) return;
  if (!open) return;
  layout(screenW, screenH);
  editBlink += 0.05f;

  const float panelW = 440.f;
  const float panelH = panelHeightForPage();
  const float panelX = (screenW - panelW) * 0.5f;
  const float panelY = (screenH - panelH) * 0.5f;

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

  drawRect(0, 0, (float)screenW, (float)screenH, 0.f, 0.f, 0.f, 0.55f);
  // Purple-tinted panel, red border (match Main Menu / project palette)
  drawRect(panelX, panelY, panelW, panelH, 0.09f, 0.05f, 0.13f, 0.94f);

  glColor4f(1.f, 0.28f, 0.30f, 0.90f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(panelX, panelY);
  glVertex2f(panelX + panelW, panelY);
  glVertex2f(panelX + panelW, panelY + panelH);
  glVertex2f(panelX, panelY + panelH);
  glEnd();

  const char* title = "SETTINGS";
  if (page == PAGE_SOUND) title = "SETTINGS  /  SOUND";
  if (page == PAGE_VIDEO) title = "SETTINGS  /  VIDEO";
  if (page == PAGE_GAMEPLAY) title = "SETTINGS  /  GAMEPLAY";
  drawText(panelX + 24, panelY + 16, title, 0.85f, 0.55f, 1.f, 2.0f);

  const char* hint = "Click a category  |  S / Esc to close";
  if (page == PAGE_SOUND) hint = "Drag track or click number to type  |  Esc";
  if (page == PAGE_VIDEO) hint = "Click number to type exact %  |  Esc";
  if (page == PAGE_GAMEPLAY) hint = "Click number to type exact %  |  Esc";
  drawText(panelX + 24, panelY + 46, hint, 0.65f, 0.7f, 0.8f, 1.1f);

  char line[80];
  for (int i = 0; i < sliderCount; i++) {
    const Slider& s = sliders[i];
    float v = std::max(0.f, std::min(1.f, *s.value));
    int disp = s.percent ? (int)(v * 100.f + 0.5f) : (int)(v + 0.5f);

    // Label (left)
    drawText(s.x, s.y - 22.f, s.label, 0.9f, 0.92f, 1.f, 1.3f);

    // Clickable value box (right)
    bool editing = (editSlider == i);
    if (editing)
      drawRect(s.numX, s.numY, s.numW, s.numH, 0.22f, 0.12f, 0.28f, 1.f);
    else
      drawRect(s.numX, s.numY, s.numW, s.numH, 0.12f, 0.08f, 0.16f, 1.f);

    glColor4f(editing ? 1.f : 0.9f, editing ? 0.45f : 0.28f, editing ? 0.35f : 0.32f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(s.numX, s.numY);
    glVertex2f(s.numX + s.numW, s.numY);
    glVertex2f(s.numX + s.numW, s.numY + s.numH);
    glVertex2f(s.numX, s.numY + s.numH);
    glEnd();

    if (editing) {
      // Show typed buffer + blinking cursor
      bool cursorOn = (std::fmod(editBlink, 1.2f) < 0.7f);
      if (cursorOn)
        snprintf(line, sizeof(line), "%s%s", editBuf, s.percent ? "%|" : "|");
      else
        snprintf(line, sizeof(line), "%s%s", editBuf, s.percent ? "%" : "");
    } else {
      if (s.percent)
        snprintf(line, sizeof(line), "%d%%", disp);
      else
        snprintf(line, sizeof(line), "%d", disp);
    }
    // Center-ish in the box
    float tw = (float)std::strlen(line) * 7.f * 1.25f;
    float tx = s.numX + std::max(4.f, (s.numW - tw) * 0.5f);
    drawText(tx, s.numY + 5.f, line, 1.f, 0.9f, 0.85f, 1.25f);

    // Track — purple fill, red-ish knob accent kept warm
    drawRect(s.x, s.y, s.w, s.h, 0.12f, 0.08f, 0.16f, 1.f);
    float fillW = s.w * v;
    drawRect(s.x, s.y, fillW, s.h, 0.55f, 0.28f, 0.85f, 1.f);
    float kx = s.x + fillW - 6.f;
    if (kx < s.x) kx = s.x;
    drawRect(kx, s.y - 4.f, 12.f, s.h + 8.f, 1.f, 0.35f, 0.32f, 1.f);
  }

  for (int i = 0; i < buttonCount; i++) {
    const Button& b = buttons[i];
    // Quit stands out slightly on the root menu
    if (b.action == -11)
      drawRect(b.x, b.y, b.w, b.h, 0.22f, 0.12f, 0.12f, 1.f);
    else if (b.action == -10)
      drawRect(b.x, b.y, b.w, b.h, 0.12f, 0.2f, 0.16f, 1.f);
    else
      drawRect(b.x, b.y, b.w, b.h, 0.14f, 0.08f, 0.20f, 1.f);
    glColor4f(1.f, 0.28f, 0.30f, 0.90f);
    if (b.action == -11) glColor4f(1.f, 0.5f, 0.45f, 0.95f);
    if (b.action == -10) glColor4f(0.45f, 1.f, 0.7f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(b.x, b.y);
    glVertex2f(b.x + b.w, b.y);
    glVertex2f(b.x + b.w, b.y + b.h);
    glVertex2f(b.x, b.y + b.h);
    glEnd();
    drawText(b.x + 16.f, b.y + 12.f, b.label, 0.85f, 0.95f, 1.f, 1.5f);
  }

  if (quitConfirmOpen)
    drawQuitConfirm();

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

int SettingsMenu::hitSlider(float mx, float my) const {
  for (int i = 0; i < sliderCount; i++) {
    const Slider& s = sliders[i];
    if (mx >= s.x - 4 && mx <= s.x + s.w + 4 &&
        my >= s.y - 10 && my <= s.y + s.h + 10)
      return i;
  }
  return -1;
}

int SettingsMenu::hitValueBox(float mx, float my) const {
  for (int i = 0; i < sliderCount; i++) {
    const Slider& s = sliders[i];
    if (mx >= s.numX && mx <= s.numX + s.numW &&
        my >= s.numY && my <= s.numY + s.numH)
      return i;
  }
  return -1;
}

int SettingsMenu::hitButton(float mx, float my) const {
  for (int i = 0; i < buttonCount; i++) {
    const Button& b = buttons[i];
    if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h)
      return i;
  }
  return -1;
}

void SettingsMenu::setSliderFromX(int index, float mx) {
  if (index < 0 || index >= sliderCount) return;
  const Slider& s = sliders[index];
  if (s.w <= 0.f) return;
  float t = (mx - s.x) / s.w;
  t = std::max(0.f, std::min(1.f, t));
  // Snap percent sliders to whole percent while dragging
  if (s.percent) {
    int p = (int)(t * 100.f + 0.5f);
    p = std::max(s.minInt, std::min(s.maxInt, p));
    *s.value = p / 100.f;
  } else {
    *s.value = t;
  }
  applySliderSideEffects(index);
}

bool SettingsMenu::onKey(int key, int action, int mods) {
  (void)mods;
  if (pieceEditor && pieceEditor->isOpen())
    return pieceEditor->onKey(key, action, mods);
  if (!open) return false;
  if (editSlider < 0) return false;
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;

  if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
    commitEdit();
    if (audioEngine) audioEngine->playSfx("sfx_select");
    return true;
  }
  if (key == GLFW_KEY_ESCAPE) {
    cancelEdit();
    return true;
  }
  if (key == GLFW_KEY_BACKSPACE) {
    if (editLen > 0) {
      editBuf[--editLen] = '\0';
    }
    return true;
  }

  // Digits 0–9 (main row + keypad)
  int digit = -1;
  if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) digit = key - GLFW_KEY_0;
  if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) digit = key - GLFW_KEY_KP_0;
  if (digit >= 0) {
    // Cap length (0–100 → max 3 digits)
    if (editLen < 3) {
      editBuf[editLen++] = (char)('0' + digit);
      editBuf[editLen] = '\0';
      // Live-clamp preview: if typed value already exceeds max, clamp buffer
      long n = std::strtol(editBuf, nullptr, 10);
      const Slider& s = sliders[editSlider];
      if (n > s.maxInt) {
        editLen = snprintf(editBuf, sizeof(editBuf), "%d", s.maxInt);
        if (editLen < 0) editLen = 0;
      }
    }
    return true;
  }
  // Consume other keys while editing so they don't trigger game shortcuts
  return true;
}

bool SettingsMenu::onMouseButton(int button, int action, float mx, float my) {
  if (pieceEditor && pieceEditor->isOpen()) {
    bool r = pieceEditor->onMouseButton(button, action, mx, my);
    if (pieceEditor->consumedClick()) {
      clickConsumed = true;
      pieceEditor->clearConsumedClick();
    }
    if (!pieceEditor->isOpen()) {
      open = true;
      page = PAGE_ROOT;
      rebuildPage();
    }
    return r || true;
  }
  if (!open) return false;
  if (button != GLFW_MOUSE_BUTTON_LEFT) return true;

  layout(lastW, lastH);

  if (action == GLFW_PRESS) {
    // Quit confirmation modal captures all clicks
    if (quitConfirmOpen) {
      if (hitQuitYes(mx, my)) {
        quitRequested = true;
        quitConfirmOpen = false;
        if (audioEngine) audioEngine->playSfx("sfx_select");
        clickConsumed = true;
        return true;
      }
      if (hitQuitNo(mx, my)) {
        quitConfirmOpen = false;
        if (audioEngine) audioEngine->playSfx("sfx_select");
        clickConsumed = true;
        return true;
      }
      // Click outside Yes/No keeps the popup open
      clickConsumed = true;
      return true;
    }

    // Click value box → edit
    int hitV = hitValueBox(mx, my);
    if (hitV >= 0) {
      beginEdit(hitV);
      if (audioEngine) audioEngine->playSfx("sfx_select");
      clickConsumed = true;
      return true;
    }

    // Click elsewhere while editing → commit
    if (editSlider >= 0) {
      commitEdit();
    }

    int hitS = hitSlider(mx, my);
    if (hitS >= 0) {
      dragIndex = hitS;
      setSliderFromX(hitS, mx);
      clickConsumed = true;
      return true;
    }
    int hitB = hitButton(mx, my);
    if (hitB >= 0) {
      int act = buttons[hitB].action;
      if (act == -10) {
        // Return → close settings
        closeMenu();
      } else if (act == -11) {
        // Quit → confirmation popup
        quitConfirmOpen = true;
        layoutQuitConfirm(lastW, lastH);
      } else if (act == -2) {
        actionCamera = !actionCamera;
        rebuildPage();
      } else if (act == -3) {
        suggestedMoves = !suggestedMoves;
        rebuildPage();
      } else if (act == -4) {
        // Reset per-side outline colours to red / purple defaults
        outWhiteR = 0.95f; outWhiteG = 0.12f; outWhiteB = 0.12f;
        outBlackR = 0.62f; outBlackG = 0.18f; outBlackB = 0.92f;
        rebuildPage();
      } else if (act < 0) {
        page = PAGE_ROOT;
        dragIndex = -1;
        rebuildPage();
      } else if (act == PAGE_PIECE_EDITOR) {
        open = false;
        if (pieceEditor) pieceEditor->openEditor();
      } else {
        page = static_cast<Page>(act);
        dragIndex = -1;
        rebuildPage();
        if (page == PAGE_SOUND) syncFromAudio();
      }
      if (audioEngine) audioEngine->playSfx("sfx_select");
      clickConsumed = true;
      return true;
    }
    clickConsumed = true;
    return true;
  }
  if (action == GLFW_RELEASE) {
    bool wasDrag = dragIndex >= 0;
    int released = dragIndex;
    dragIndex = -1;
    clickConsumed = true;
    if (wasDrag && page == PAGE_SOUND && released == 2 && audioEngine)
      audioEngine->playSfx("sfx_select");
    return true;
  }
  return true;
}

bool SettingsMenu::onMouseMove(float mx, float my) {
  if (pieceEditor && pieceEditor->isOpen())
    return pieceEditor->onMouseMove(mx, my);
  if (!open || dragIndex < 0) return open;
  layout(lastW, lastH);
  setSliderFromX(dragIndex, mx);
  return true;
}
