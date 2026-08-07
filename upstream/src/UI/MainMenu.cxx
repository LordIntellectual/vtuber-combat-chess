#define GL_GLEXT_PROTOTYPES
#include "MainMenu.hxx"
#include "../utils/utils.hxx"
#include "../utils/math.hxx"
#include "../utils/GlState.hxx"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
#include <cmath>

#include "../../third_party/stb_easy_font.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MainMenu::MainMenu()
  : visible(false),
    hostDialogOpen_(false),
    quitConfirmOpen_(false),
    page_(PAGE_ROOT),
    pending(ACTION_NONE),
    lastW(1280), lastH(720),
    hoverBtn(-1),
    hoverRoom(-1),
    hoverPopupBtn(-1),
    hoverQuitBtn(-1),
    selectedRoom_(-1),
    focusField(0),
    hostName("Stream Match"),
    blinkT(0.f),
    bgTex(0),
    bgLoaded(false),
    fbo_(0),
    fboColor_(0),
    fboDepth_(0),
    fboW_(0),
    fboH_(0),
    motionT_(0.f),
    camPanX_(0.f),
    camPanY_(0.f),
    planeTiltX_(0.f),
    planeTiltY_(0.f),
    menuPlaneZ_(-3.2f),
    bgPlaneZ_(-4.0f),
    fovYDeg_(42.f),
    uiLayerPanX_(0.f),
    uiLayerPanY_(0.f),
    effectCount_(0),
    effectRedCount_(0),
    effectPurpleCount_(0),
    effectSeeded_(false),
    cursorX_(0.f),
    cursorY_(0.f),
    cursorKnown_(false),
    panelX(0), panelY(0), panelW(640), panelH(520),
    buttonCount(0),
    popupButtonCount(0),
    jpX(0), jpY(0), jpW(0), jpH(0),
    listX(0), listY(0), listW(0), listH(0),
    rowH(28.f),
    dlgX(0), dlgY(0), dlgW(0), dlgH(0),
    hnX(0), hnY(0), hnW(0), hnH(0),
    hpX(0), hpY(0), hpW(0), hpH(0),
    quitPanelX(0), quitPanelY(0), quitPanelW(440), quitPanelH(200),
    quitYesX(0), quitYesY(0), quitYesW(150), quitYesH(42),
    quitNoX(0), quitNoY(0), quitNoW(150), quitNoH(42) {
  rebuild();
}

void MainMenu::destroyFbo() {
  if (fboColor_) { glDeleteTextures(1, &fboColor_); fboColor_ = 0; }
  if (fboDepth_) { glDeleteRenderbuffers(1, &fboDepth_); fboDepth_ = 0; }
  if (fbo_) { glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
  fboW_ = fboH_ = 0;
}

MainMenu::~MainMenu() {
  if (bgTex) {
    glDeleteTextures(1, &bgTex);
    bgTex = 0;
  }
  destroyFbo();
}

bool MainMenu::loadBackground(const std::string& pngPath) {
  try {
    GLuint tex = loadPNGTexture(pngPath);
    if (!tex) {
      std::cerr << "[MainMenu] background load returned 0: " << pngPath << "\n";
      return false;
    }
    if (bgTex) glDeleteTextures(1, &bgTex);
    bgTex = tex;
    bgLoaded = true;
    // Smooth scale for 1280x720 art on any window size
    glBindTexture(GL_TEXTURE_2D, bgTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "[MainMenu] background: " << pngPath << "\n";
    return true;
  } catch (const std::exception& e) {
    std::cerr << "[MainMenu] background load failed: " << e.what() << "\n";
    bgLoaded = false;
    return false;
  }
}

void MainMenu::show() {
  visible = true;
  page_ = PAGE_ROOT;
  pending = ACTION_NONE;
  focusField = 0;
  hostDialogOpen_ = false;
  quitConfirmOpen_ = false;
  statusLine.clear();
  // Reseed flakes when menu opens so density matches current window size.
  effectSeeded_ = false;
  rebuild();
}

void MainMenu::hide() {
  visible = false;
  pending = ACTION_NONE;
  focusField = 0;
  hostDialogOpen_ = false;
  quitConfirmOpen_ = false;
}

void MainMenu::openHostDialog() {
  hostDialogOpen_ = true;
  quitConfirmOpen_ = false;
  focusField = 1; // room name
  hoverPopupBtn = -1;
  if (hostName.empty()) hostName = "Stream Match";
}

void MainMenu::closeHostDialog() {
  hostDialogOpen_ = false;
  focusField = 0;
  hoverPopupBtn = -1;
}

void MainMenu::openQuitConfirm() {
  quitConfirmOpen_ = true;
  hostDialogOpen_ = false;
  focusField = 0;
  hoverQuitBtn = -1;
}

void MainMenu::closeQuitConfirm() {
  quitConfirmOpen_ = false;
  hoverQuitBtn = -1;
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
  // Guard against zero/invalid sizes (avoids off-screen UI)
  if (w < 1) w = 1280;
  if (h < 1) h = 720;
  lastW = w;
  lastH = h;
  if (page_ == PAGE_ROOT) {
    // Fit the panel snugly around fixed-size buttons (shrink box, not stretch buttons).
    const float btnW = 320.f, btnH = 48.f, gap = 16.f;
    const float padX = 24.f;   // was ~100px empty each side of 320@520
    const float padTop = 78.f; // title + "Main Menu" only
    const float padBot = 36.f; // foot + optional status line (drawn at H-28)
    const float stackH = (float)buttonCount * btnH
                       + (float)std::max(0, buttonCount - 1) * gap;
    panelW = btnW + padX * 2.f;
    panelH = padTop + stackH + padBot;
    panelX = (w - panelW) * 0.5f;
    panelY = (h - panelH) * 0.5f;
    float y = panelY + padTop;
    for (int i = 0; i < buttonCount; ++i) {
      buttons[i].w = btnW;
      buttons[i].h = btnH;
      buttons[i].x = panelX + padX;
      buttons[i].y = y;
      y += btnH + gap;
    }
  } else {
    panelW = std::min(720.f, (float)w - 40.f);
    panelH = std::min(520.f, (float)h - 40.f);
    panelX = (w - panelW) * 0.5f;
    panelY = (h - panelH) * 0.5f;
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

  // Quit confirmation always laid out (Main Menu root returns early used to skip this)
  quitPanelW = 440.f;
  quitPanelH = 200.f;
  quitPanelX = (w - quitPanelW) * 0.5f;
  quitPanelY = (h - quitPanelH) * 0.5f;
  quitYesW = 150.f;
  quitYesH = 42.f;
  quitNoW = 150.f;
  quitNoH = 42.f;
  {
    const float gap = 24.f;
    const float total = quitYesW + gap + quitNoW;
    quitYesX = quitPanelX + (quitPanelW - total) * 0.5f;
    quitNoX = quitYesX + quitYesW + gap;
    quitYesY = quitPanelY + quitPanelH - 62.f;
    quitNoY = quitYesY;
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
  glDisable(GL_TEXTURE_2D);
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
}

void MainMenu::drawTexturedRect(float x, float y, float w, float h, GLuint tex) {
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  glColor4f(1.f, 1.f, 1.f, 1.f);
  glBegin(GL_QUADS);
  // Image is top-left origin in file; ortho is top-left (y down) — flip V
  glTexCoord2f(0.f, 1.f); glVertex2f(x, y);
  glTexCoord2f(1.f, 1.f); glVertex2f(x + w, y);
  glTexCoord2f(1.f, 0.f); glVertex2f(x + w, y + h);
  glTexCoord2f(0.f, 0.f); glVertex2f(x, y + h);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
}

void MainMenu::ensureFbo(int w, int h) {
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  if (fbo_ && fboW_ == w && fboH_ == h) return;
  destroyFbo();
  fboW_ = w;
  fboH_ = h;
  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glGenTextures(1, &fboColor_);
  glBindTexture(GL_TEXTURE_2D, fboColor_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboColor_, 0);
  glGenRenderbuffers(1, &fboDepth_);
  glBindRenderbuffer(GL_RENDERBUFFER, fboDepth_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fboDepth_);
  GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (st != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "[MainMenu] FBO incomplete: 0x" << std::hex << st << std::dec << "\n";
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void MainMenu::updateParallax(float dt) {
  motionT_ += dt;
  // Normalised pan in [-1, 1]-ish; converted to pixels in draw/project.
  camPanX_ = std::sin(motionT_ * 0.22f);
  camPanY_ = std::cos(motionT_ * 0.17f);
  // Unused by 2.5D path; kept for future true-3D experiments.
  planeTiltX_ = 0.025f * std::sin(motionT_ * 0.19f + 0.4f);
  planeTiltY_ = 0.030f * std::cos(motionT_ * 0.15f);
}

void MainMenu::effectHueRgb(int hue, float& r, float& g, float& b) const {
  if (hue == EFFECT_PURPLE) {
    r = 0.72f; g = 0.22f; b = 1.f; // team purple
  } else {
    r = 1.f; g = 0.18f; b = 0.22f; // team red
  }
}

int MainMenu::pickBalancedEffectHue(float salt) const {
  // More red active → higher chance of purple (and vice versa). Soft bias.
  const float imbalance = (float)(effectRedCount_ - effectPurpleCount_);
  const float denom = (float)std::max(effectCount_, 1);
  float pPurple = 0.5f + 0.55f * (imbalance / denom);
  if (pPurple < 0.12f) pPurple = 0.12f;
  if (pPurple > 0.88f) pPurple = 0.88f;
  // Deterministic sample from time + counts + per-spawn salt
  float u = std::fmod(motionT_ * 17.13f + salt * 97.1f
                      + (float)effectRedCount_ * 0.37f
                      + (float)effectPurpleCount_ * 0.91f, 1.f);
  if (u < 0.f) u += 1.f;
  return (u < pPurple) ? EFFECT_PURPLE : EFFECT_RED;
}

void MainMenu::assignEffectHue(EffectParticle& p, int hue) {
  if (p.hue == EFFECT_RED) --effectRedCount_;
  else if (p.hue == EFFECT_PURPLE) --effectPurpleCount_;
  p.hue = (hue == EFFECT_PURPLE) ? EFFECT_PURPLE : EFFECT_RED;
  if (p.hue == EFFECT_PURPLE) ++effectPurpleCount_;
  else ++effectRedCount_;
}

void MainMenu::respawnEffectParticle(EffectParticle& p, int w, int h, int exitEdge) {
  // Destroy particle that left the plane and birth a new one near the
  // opposite edge, moving inward — continuous flow, no edge pile-up.
  // exitEdge: 0 top, 1 left, 2 right, 3 bottom
  // Re-hash seed so each life looks different (deterministic, no rand).
  p.seed = std::fmod(p.seed * 1.6180339887f + 0.371f + motionT_ * 0.07f, 1.f);
  if (p.seed < 0.f) p.seed += 1.f;

  // Colour: rebalance red vs purple (destroyed hue may flip on birth).
  assignEffectHue(p, pickBalancedEffectHue(p.seed));

  p.size = 2.8f + std::fmod(p.seed * 17.3f, 3.2f); // ball core radius
  p.alpha = 0.75f + std::fmod(p.seed * 9.1f, 0.25f);

  // Spawn just inside the opposite edge so the orb is immediately visible
  // and already travelling into the plane (not stuck outside the pad zone).
  const float inset = 4.f + p.size;
  const float speedIn = 40.f + std::fmod(p.seed * 41.f, 50.f);
  const float drift = -20.f + std::fmod(p.seed * 53.f, 40.f); // lateral scatter

  // opposite of exit
  const int spawnEdge = (exitEdge + 2) % 4;
  switch (spawnEdge) {
    case 0: // enter from top (came off bottom)
      p.x = std::fmod(p.seed * 997.f, (float)std::max(w, 1));
      p.y = inset;
      p.vx = drift;
      p.vy = speedIn; // down (y-down ortho)
      break;
    case 1: // enter from left (came off right)
      p.x = inset;
      p.y = std::fmod(p.seed * 773.f, (float)std::max(h, 1));
      p.vx = speedIn;
      p.vy = drift;
      break;
    case 2: // enter from right (came off left)
      p.x = (float)w - inset;
      p.y = std::fmod(p.seed * 661.f, (float)std::max(h, 1));
      p.vx = -speedIn;
      p.vy = drift;
      break;
    default: // enter from bottom (came off top)
      p.x = std::fmod(p.seed * 883.f, (float)std::max(w, 1));
      p.y = (float)h - inset;
      p.vx = drift;
      p.vy = -speedIn; // up
      break;
  }
}

void MainMenu::seedEffectParticles(int w, int h) {
  if (w < 1) w = 1280;
  if (h < 1) h = 720;
  effectCount_ = kMaxEffectParticles;
  effectRedCount_ = 0;
  effectPurpleCount_ = 0;
  for (int i = 0; i < effectCount_; ++i) {
    EffectParticle& p = effectParts_[i];
    // Deterministic pseudo-random from index (no rand() dependency).
    p.seed = std::fmod(0.173f * (float)(i + 1) + 0.6180339887f * (float)i, 1.f);
    if (p.seed < 0.f) p.seed += 1.f;
    p.size = 2.8f + std::fmod(p.seed * 17.3f, 3.2f);
    p.alpha = 0.75f + std::fmod(p.seed * 9.1f, 0.25f);
    p.x = std::fmod(p.seed * 1301.f + (float)i * 17.f, (float)w);
    p.y = std::fmod(p.seed * 907.f + (float)i * 29.f, (float)h);
    p.vx = 15.f + std::fmod(p.seed * 53.f, 50.f);
    p.vy = 12.f + std::fmod(p.seed * 37.f, 35.f);
    // Start ~even: alternate, then let balance logic maintain it.
    p.hue = -1; // assignEffectHue treats unknown as no decrement
    assignEffectHue(p, (i & 1) ? EFFECT_PURPLE : EFFECT_RED);
  }
  effectSeeded_ = true;
}

void MainMenu::updateEffectLayer(float dt, int w, int h) {
  if (w < 1 || h < 1) return;
  if (!effectSeeded_ || effectCount_ <= 0)
    seedEffectParticles(w, h);

  // Clamp dt so a hitch does not teleport flakes.
  if (dt < 0.f) dt = 0.f;
  if (dt > 0.05f) dt = 0.05f;

  // Global uneven wind field (pixels / s²-ish via integrated accel).
  const float t = motionT_;
  const float windBaseX = 55.f + 40.f * std::sin(t * 0.31f) + 22.f * std::sin(t * 0.73f + 1.2f);
  const float windBaseY = 35.f + 28.f * std::cos(t * 0.27f) + 18.f * std::sin(t * 0.61f + 0.4f);

  // Cursor repulsion
  const float repelRadius = 110.f;
  const float repelRadiusSq = repelRadius * repelRadius;
  const float repelStrength = 2200.f; // accel scale
  const float drag = 0.92f;          // velocity damping per ~frame at 60Hz-ish
  const float dragPow = std::pow(drag, dt * 60.f);

  for (int i = 0; i < effectCount_; ++i) {
    EffectParticle& p = effectParts_[i];

    // Local wind gust (spatial + per-particle phase)
    const float gx = 0.012f * p.x + p.seed * 6.28f;
    const float gy = 0.015f * p.y + p.seed * 4.1f;
    float windX = windBaseX
      + 30.f * std::sin(t * 0.9f + gx)
      + 18.f * std::cos(t * 1.4f + gy + p.seed * 3.f);
    float windY = windBaseY
      + 24.f * std::cos(t * 0.8f + gy)
      + 14.f * std::sin(t * 1.1f + gx);

    // Soft spring toward wind velocity (terminal velocity tracking)
    const float kTrack = 1.8f;
    p.vx += (windX - p.vx) * kTrack * dt;
    p.vy += (windY - p.vy) * kTrack * dt;

    if (cursorKnown_) {
      float dx = p.x - cursorX_;
      float dy = p.y - cursorY_;
      float d2 = dx * dx + dy * dy;
      if (d2 < repelRadiusSq && d2 > 1.f) {
        float d = std::sqrt(d2);
        float falloff = 1.f - (d / repelRadius);
        falloff *= falloff; // stronger near cursor
        float inv = (repelStrength * falloff) / d;
        p.vx += dx * inv * dt;
        p.vy += dy * inv * dt;
      }
    }

    p.vx *= dragPow;
    p.vy *= dragPow;
    p.x += p.vx * dt;
    p.y += p.vy * dt;

    // Past plane bounds → destroy & spawn on the opposite end (no pile-up).
    // Use particle half-size so the flake is fully off-screen before recycle.
    const float pad = p.size + 2.f;
    if (p.x > (float)w + pad) {
      respawnEffectParticle(p, w, h, 2); // exited right → birth on left
    } else if (p.x < -pad) {
      respawnEffectParticle(p, w, h, 1); // exited left → birth on right
    } else if (p.y > (float)h + pad) {
      respawnEffectParticle(p, w, h, 3); // exited bottom → birth on top
    } else if (p.y < -pad) {
      respawnEffectParticle(p, w, h, 0); // exited top → birth on bottom
    }
  }
}

void MainMenu::drawEffectLayer() {
  if (effectCount_ <= 0) return;
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);

  auto disc = [](float cx, float cy, float rad, float r, float g, float b, float a,
                 int segs) {
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, a);
    glVertex2f(cx, cy);
    for (int s = 0; s <= segs; ++s) {
      float ang = (float)s / (float)segs * 2.f * (float)M_PI;
      // Soft falloff: rim more transparent
      float t = (float)s / (float)segs;
      (void)t;
      glColor4f(r, g, b, a * 0.15f);
      glVertex2f(cx + std::cos(ang) * rad, cy + std::sin(ang) * rad);
    }
    glEnd();
  };

  // Additive glow pass — light bloom / emission
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  for (int i = 0; i < effectCount_; ++i) {
    const EffectParticle& p = effectParts_[i];
    float hr, hg, hb;
    effectHueRgb(p.hue, hr, hg, hb);
    // Outer halo
    disc(p.x, p.y, p.size * 3.6f, hr, hg, hb, 0.18f * p.alpha, 14);
    // Mid glow
    disc(p.x, p.y, p.size * 2.2f, hr, hg, hb, 0.35f * p.alpha, 12);
  }

  // Crackle bolts (additive thin lines)
  glLineWidth(1.2f);
  glBegin(GL_LINES);
  for (int i = 0; i < effectCount_; ++i) {
    const EffectParticle& p = effectParts_[i];
    float hr, hg, hb;
    effectHueRgb(p.hue, hr, hg, hb);
    // 3 candidate sparks; flicker via time + seed
    for (int b = 0; b < 3; ++b) {
      float phase = motionT_ * 22.f + p.seed * 40.f + (float)b * 2.7f + (float)i * 0.13f;
      float flicker = std::sin(phase) * std::sin(phase * 1.7f + p.seed);
      if (flicker < 0.25f) continue; // mostly off — crackle, not solid rays
      float ang = p.seed * 6.28f + (float)b * 2.094f
                 + 0.55f * std::sin(phase * 3.1f);
      float len = p.size * (1.8f + 1.4f * flicker + std::fmod(p.seed * 3.f + b, 1.2f));
      // Jagged mid-point
      float mx = p.x + std::cos(ang) * len * 0.45f
                 + std::sin(phase * 5.f) * p.size * 0.35f;
      float my = p.y + std::sin(ang) * len * 0.45f
                 + std::cos(phase * 4.3f) * p.size * 0.35f;
      float ex = p.x + std::cos(ang) * len;
      float ey = p.y + std::sin(ang) * len;
      float aBolt = 0.55f * p.alpha * flicker;
      glColor4f(1.f, 0.92f, 1.f, aBolt); // hot white core of arc
      glVertex2f(p.x, p.y);
      glVertex2f(mx, my);
      glColor4f(hr, hg, hb, aBolt * 0.85f);
      glVertex2f(mx, my);
      glVertex2f(ex, ey);
    }
  }
  glEnd();

  // Solid cores (normal alpha blend for readable orbs)
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (int i = 0; i < effectCount_; ++i) {
    const EffectParticle& p = effectParts_[i];
    float hr, hg, hb;
    effectHueRgb(p.hue, hr, hg, hb);
    // Soft colour body
    disc(p.x, p.y, p.size * 1.15f, hr, hg, hb, 0.85f * p.alpha, 12);
    // Hot centre
    disc(p.x, p.y, p.size * 0.45f, 1.f, 0.95f, 1.f, 0.95f * p.alpha, 10);
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(1.f, 1.f, 1.f, 1.f);
  glLineWidth(1.f);
}

void MainMenu::drawParallaxScene(int screenW, int screenH) {
  // 2.5D orthographic layers (NOT perspective 3D).
  // Prior true-3D paths mis-framed because shader lookAt/proj matrices are not
  // valid for glLoadMatrixf, and even glFrustum variants left the FBO plane
  // off-centre while hit-testing stayed 1:1 (menu upper-right, clicks centre).
  // Layers share the same y-down ortho as the UI FBO, so framing always matches.
  ncaResetPipelineState();
  glViewport(0, 0, screenW, screenH);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.02f, 0.03f, 0.06f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, screenW, screenH, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Far art drifts more than the near UI plane (parallax depth cue).
  const float bgAmpX = 28.f;
  const float bgAmpY = 18.f;
  const float uiAmpX = 10.f;
  const float uiAmpY = 7.f;
  const float bgPanX = camPanX_ * bgAmpX;
  const float bgPanY = camPanY_ * bgAmpY;
  const float uiPanX = camPanX_ * uiAmpX;
  const float uiPanY = camPanY_ * uiAmpY;
  // Remember for mouse → UI (same frame values live in members too).
  uiLayerPanX_ = uiPanX;
  uiLayerPanY_ = uiPanY;

  // Far background: slightly oversize so pan never shows edges.
  {
    const float bgScale = 1.12f;
    const float bw = (float)screenW * bgScale;
    const float bh = (float)screenH * bgScale;
    const float bx = ((float)screenW - bw) * 0.5f + bgPanX;
    const float by = ((float)screenH - bh) * 0.5f + bgPanY;
    if (bgLoaded && bgTex) {
      drawTexturedRect(bx, by, bw, bh, bgTex);
    } else {
      drawRect(0, 0, (float)screenW, (float)screenH, 0.05f, 0.07f, 0.12f, 1.f);
    }
    // No full-screen darken on Main Menu — hero art should stay bright.
    // Settings / dialogs apply their own dim when focus is on the panel.
  }

  // Mid: effect plane (particles) — behind menu UI, in front of art.
  updateEffectLayer(0.016f, screenW, screenH);
  drawEffectLayer();

  // Near UI: FBO drawn 1:1 screen size, small pan for depth (hits compensate).
  if (fboColor_) {
    drawTexturedRect(uiPanX, uiPanY, (float)screenW, (float)screenH, fboColor_);
  }

  ncaResetPipelineState();
  glDisable(GL_DEPTH_TEST);
}

bool MainMenu::projectMouseToUi(float mx, float my, float& uiX, float& uiY) const {
  // UI layer is drawn at (uiLayerPanX_, uiLayerPanY_); invert that offset.
  if (lastW < 1 || lastH < 1) return false;
  uiX = mx - uiLayerPanX_;
  uiY = my - uiLayerPanY_;
  return true;
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
  // Purple-tinted fill (project palette)
  float br = hover ? 0.18f : 0.10f;
  float bg = hover ? 0.10f : 0.06f;
  float bb = hover ? 0.26f : 0.16f;
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
  // Red border (default); Create keeps green cue
  glColor4f(1.f, 0.28f, 0.30f, hover ? 1.f : 0.80f);
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
  drawRect(x, y, w, h, focused ? 0.14f : 0.07f, focused ? 0.08f : 0.05f,
           focused ? 0.22f : 0.12f, 1.f);
  glColor4f(focused ? 1.f : 0.85f, focused ? 0.35f : 0.28f, focused ? 0.38f : 0.32f, 1.f);
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

bool MainMenu::hitQuitYes(float mx, float my) const {
  return mx >= quitYesX && mx <= quitYesX + quitYesW &&
         my >= quitYesY && my <= quitYesY + quitYesH;
}

bool MainMenu::hitQuitNo(float mx, float my) const {
  return mx >= quitNoX && mx <= quitNoX + quitNoW &&
         my >= quitNoY && my <= quitNoY + quitNoH;
}

void MainMenu::drawQuitConfirm() {
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

  bool yesHov = hoverQuitBtn == 0;
  bool noHov = hoverQuitBtn == 1;
  drawRect(quitYesX, quitYesY, quitYesW, quitYesH,
           yesHov ? 0.55f : 0.45f, 0.15f, 0.15f, 1.f);
  glColor4f(1.f, 0.5f, 0.45f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(quitYesX, quitYesY);
  glVertex2f(quitYesX + quitYesW, quitYesY);
  glVertex2f(quitYesX + quitYesW, quitYesY + quitYesH);
  glVertex2f(quitYesX, quitYesY + quitYesH);
  glEnd();
  drawText(quitYesX + 50.f, quitYesY + 12.f, "Yes", 1.f, 0.9f, 0.9f, 1.6f);

  drawRect(quitNoX, quitNoY, quitNoW, quitNoH,
           0.12f, noHov ? 0.35f : 0.28f, 0.2f, 1.f);
  glColor4f(0.4f, 1.f, 0.65f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(quitNoX, quitNoY);
  glVertex2f(quitNoX + quitNoW, quitNoY);
  glVertex2f(quitNoX + quitNoW, quitNoY + quitNoH);
  glVertex2f(quitNoX, quitNoY + quitNoH);
  glEnd();
  drawText(quitNoX + 55.f, quitNoY + 12.f, "No", 0.9f, 1.f, 0.95f, 1.6f);
}

void MainMenu::drawHostDialog() {
  // Dim multiplayer panel further
  drawRect(0, 0, (float)lastW, (float)lastH, 0.0f, 0.0f, 0.0f, 0.45f);

  drawRect(dlgX, dlgY, dlgW, dlgH, 0.09f, 0.05f, 0.13f, 0.98f);
  glColor4f(1.f, 0.28f, 0.30f, 0.95f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(dlgX, dlgY);
  glVertex2f(dlgX + dlgW, dlgY);
  glVertex2f(dlgX + dlgW, dlgY + dlgH);
  glVertex2f(dlgX, dlgY + dlgH);
  glEnd();

  drawText(dlgX + 28, dlgY + 24, "Host Online Room", 0.85f, 0.55f, 1.f, 1.8f);
  drawText(dlgX + 28, dlgY + 54, "Choose a name others will see in the list.",
           0.7f, 0.75f, 0.82f, 1.15f);

  drawText(hnX, hnY - 16, "Room name", 0.75f, 0.8f, 0.9f, 1.15f);
  drawField(hnX, hnY, hnW, hnH, hostName, focusField == 1, "Stream Match");

  drawText(hpX, hpY - 16, "Password (optional)", 0.75f, 0.8f, 0.9f, 1.15f);
  drawField(hpX, hpY, hpW, hpH, hostPass, focusField == 2, "leave empty for open room");

  for (int i = 0; i < popupButtonCount; ++i)
    drawButton(popupButtons[i], i == hoverPopupBtn);
}

void MainMenu::drawUiContent(int screenW, int screenH) {
  (void)screenW;
  (void)screenH;
  glUseProgram(0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Semi-transparent purple-tinted panel; red border (project palette)
  drawRect(panelX, panelY, panelW, panelH, 0.09f, 0.05f, 0.13f, 0.88f);
  glColor4f(1.f, 0.28f, 0.30f, 0.92f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(panelX, panelY);
  glVertex2f(panelX + panelW, panelY);
  glVertex2f(panelX + panelW, panelY + panelH);
  glVertex2f(panelX, panelY + panelH);
  glEnd();

  drawText(panelX + 24, panelY + 22, "vTuber Combat Chess", 0.85f, 0.55f, 1.f, 2.0f);

  if (page_ == PAGE_ROOT) {
    drawText(panelX + 24, panelY + 56, "Main Menu", 0.9f, 0.82f, 0.95f, 1.4f);
    for (int i = 0; i < buttonCount; ++i)
      drawButton(buttons[i], i == hoverBtn);
  } else {
    drawText(panelX + 24, panelY + 52, "Online Multiplayer",
             0.9f, 0.82f, 0.95f, 1.35f);

    drawRect(listX, listY, listW, listH, 0.05f, 0.03f, 0.08f, 1.f);
    glColor4f(1.f, 0.28f, 0.30f, 0.85f);
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
          drawRect(listX + 2, ry, listW - 4, rowH - 2, 0.22f, 0.10f, 0.28f, 1.f);
        else if (hov)
          drawRect(listX + 2, ry, listW - 4, rowH - 2, 0.14f, 0.08f, 0.18f, 1.f);
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

  if (!statusLine.empty() && !hostDialogOpen_ && !quitConfirmOpen_) {
    drawText(panelX + 24, panelY + panelH - 28, statusLine.c_str(),
             1.f, 0.75f, 0.4f, 1.15f);
  }

  if (quitConfirmOpen_)
    drawQuitConfirm();
}

void MainMenu::draw(int screenW, int screenH) {
  if (!visible) return;
  layout(screenW, screenH);
  blinkT += 0.016f;
  updateParallax(0.016f);

  ensureFbo(screenW, screenH);

  // --- Pass 1: UI into transparent FBO ---
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, screenW, screenH);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, screenW, screenH, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  drawUiContent(screenW, screenH);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // --- Pass 2: 3D parallax stage ---
  drawParallaxScene(screenW, screenH);
}

bool MainMenu::onMouseMove(float mx, float my) {
  if (!visible) return false;
  // Screen-space cursor for effect-layer repulsion (before UI pan remap).
  cursorX_ = mx;
  cursorY_ = my;
  cursorKnown_ = true;
  float uiX = mx, uiY = my;
  if (!projectMouseToUi(mx, my, uiX, uiY)) {
    hoverBtn = hoverRoom = hoverPopupBtn = hoverQuitBtn = -1;
    return true;
  }
  mx = uiX;
  my = uiY;
  if (quitConfirmOpen_) {
    hoverQuitBtn = hitQuitYes(mx, my) ? 0 : (hitQuitNo(mx, my) ? 1 : -1);
    hoverBtn = -1;
    hoverRoom = -1;
    hoverPopupBtn = -1;
    return true;
  }
  if (hostDialogOpen_) {
    hoverPopupBtn = hitPopupButton(mx, my);
    hoverBtn = -1;
    hoverRoom = -1;
    return true;
  }
  hoverPopupBtn = -1;
  hoverQuitBtn = -1;
  hoverBtn = hitButton(mx, my);
  hoverRoom = hitRoom(mx, my);
  return true;
}

bool MainMenu::onMouseButton(int button, int action, float mx, float my) {
  if (!visible) return false;
  if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
    return true;

  float uiX = mx, uiY = my;
  if (!projectMouseToUi(mx, my, uiX, uiY))
    return true;
  mx = uiX;
  my = uiY;

  // Quit confirmation captures all clicks
  if (quitConfirmOpen_) {
    if (hitQuitYes(mx, my)) {
      closeQuitConfirm();
      pending = ACTION_QUIT;
      return true;
    }
    if (hitQuitNo(mx, my)) {
      closeQuitConfirm();
      return true;
    }
    return true; // ignore outside clicks
  }

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
  else if (id == (int)ACTION_QUIT) openQuitConfirm();
  return true;
}

bool MainMenu::onKey(int key, int action, int mods) {
  (void)mods;
  if (!visible) return false;
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;

  if (key == GLFW_KEY_ESCAPE) {
    if (quitConfirmOpen_) {
      closeQuitConfirm();
      return true;
    }
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
    // Root: Esc opens quit confirmation (same as Quit button)
    openQuitConfirm();
    return true;
  }

  if (quitConfirmOpen_) {
    if (key == GLFW_KEY_ENTER) {
      closeQuitConfirm();
      pending = ACTION_QUIT;
      return true;
    }
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
