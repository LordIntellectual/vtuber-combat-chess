#include "../gl_compat.hxx"
#include "PieceEditor.hxx"
#include "../constants.hxx"
#include "../utils/math.hxx"
#include "../utils/GlState.hxx"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>

#include "../../third_party/stb_easy_font.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PieceEditor::PieceEditor()
  : open(false), clickConsumed(false), dragSlider(-1),
    lastW(1280), lastH(720), dropOpen(DROP_NONE),
    editSlider(-1), editLen(0), editBlink(0.f),
    orbiting(false), lastOrbitMx(0), lastOrbitMy(0),
    orbitYaw(-35.f), orbitPitch(32.f), orbitDist(16.f),
    sets(nullptr), store(nullptr), audio(nullptr),
    setIndex(0), pieceIndex(0), dirty(false), previewShadowTex(0) {
  editBuf[0] = '\0';
  // label, value*, min, max, track x/y/w/h, num x/y/w/h
  sliders[0] = {"Pos X", &edit.px, -2.f, 2.f, 0, 0, 0, 0, 0, 0, 0, 0};
  sliders[1] = {"Pos Y", &edit.py, -2.f, 2.f, 0, 0, 0, 0, 0, 0, 0, 0};
  sliders[2] = {"Pos Z", &edit.pz, -1.f, 3.f, 0, 0, 0, 0, 0, 0, 0, 0};
  sliders[3] = {"Rot X", &edit.rx, -180.f, 180.f, 0, 0, 0, 0, 0, 0, 0, 0};
  sliders[4] = {"Rot Y", &edit.ry, -180.f, 180.f, 0, 0, 0, 0, 0, 0, 0, 0};
  sliders[5] = {"Rot Z", &edit.rz, -180.f, 180.f, 0, 0, 0, 0, 0, 0, 0, 0};
  sliders[6] = {"Scale", &edit.scale, 0.25f, 3.f, 0, 0, 0, 0, 0, 0, 0, 0};
}

PieceEditor::~PieceEditor() {
  freePreview();
  if (previewShadowTex) {
    glDeleteTextures(1, &previewShadowTex);
    previewShadowTex = 0;
  }
}

void PieceEditor::setDeps(PieceSetManager* s, PieceTransformStore* st, AudioEngine* a) {
  sets = s;
  store = st;
  audio = a;
}

void PieceEditor::freePreview() {
  PieceSetManager::freePieces(&previewMeshes);
  loadedSetId.clear();
}

void PieceEditor::loadPreviewForSet(int index) {
  if (!sets || sets->sets().empty()) return;
  if (index < 0 || index >= (int)sets->sets().size()) index = 0;
  setIndex = index;
  const auto& info = sets->sets().at((size_t)setIndex);
  if (info.id == loadedSetId && !previewMeshes.empty()) {
    loadEditFromStore();
    return;
  }

  freePreview();
  // Temporarily select set for loadPieces()
  std::string prev = sets->current().id;
  sets->select(info.id);
  try {
    previewMeshes = sets->loadPieces();
    loadedSetId = info.id;
    std::cout << "[PieceEditor] Loaded set " << info.id
              << " (" << previewMeshes.size() << " meshes)\n";
  } catch (const std::exception& e) {
    std::cerr << "[PieceEditor] load failed: " << e.what() << "\n";
  }
  sets->select(prev);
  loadEditFromStore();
}

void PieceEditor::loadEditFromStore() {
  if (!store || !sets || sets->sets().empty()) {
    edit = PieceTransform{};
    dirty = false;
    return;
  }
  const auto& keys = PieceTransformStore::allPieceKeys();
  if (pieceIndex < 0 || pieceIndex >= (int)keys.size()) pieceIndex = 0;
  std::string sid = sets->sets().at((size_t)setIndex).id;
  edit = store->get(sid, keys[(size_t)pieceIndex]);
  if (edit.scale < 1e-4f) edit.scale = 1.f;
  dirty = false;
}

void PieceEditor::openEditor() {
  open = true;
  dropOpen = DROP_NONE;
  dragSlider = -1;
  cancelEdit();
  orbiting = false;
  // Default viewing angle (similar to in-game board look)
  orbitYaw = -35.f;
  orbitPitch = 32.f;
  orbitDist = 16.f;
  if (sets && !sets->sets().empty()) {
    setIndex = sets->activeIndex();
    loadPreviewForSet(setIndex);
    loadEditFromStore();
  }
}

void PieceEditor::closeEditor() {
  commitEdit();
  open = false;
  dropOpen = DROP_NONE;
  dragSlider = -1;
  orbiting = false;
}

bool PieceEditor::handleBack() {
  if (!open) return false;
  if (editSlider >= 0) {
    cancelEdit();
    return true;
  }
  if (dropOpen != DROP_NONE) {
    dropOpen = DROP_NONE;
    return true;
  }
  closeEditor();
  return false;
}

std::string PieceEditor::currentSetId() const {
  if (!sets || sets->sets().empty()) return "";
  return sets->sets().at((size_t)setIndex).id;
}

std::string PieceEditor::currentPieceKey() const {
  const auto& keys = PieceTransformStore::allPieceKeys();
  if (pieceIndex < 0 || pieceIndex >= (int)keys.size()) return "king";
  return keys[(size_t)pieceIndex];
}

void PieceEditor::layout(int screenW, int screenH) {
  lastW = screenW;
  lastH = screenH;
  panelW = std::min(920.f, (float)screenW - 40.f);
  panelH = std::min(620.f, (float)screenH - 40.f);
  panelX = ((float)screenW - panelW) * 0.5f;
  panelY = ((float)screenH - panelH) * 0.5f;

  setBoxX = panelX + 20.f;
  setBoxY = panelY + 56.f;
  setBoxW = (panelW - 50.f) * 0.5f;
  setBoxH = 32.f;
  pieceBoxX = setBoxX + setBoxW + 10.f;
  pieceBoxY = setBoxY;
  pieceBoxW = setBoxW;
  pieceBoxH = 32.f;

  viewX = panelX + 20.f;
  viewY = panelY + 100.f;
  viewW = panelW * 0.52f;
  viewH = panelH - 160.f;

  float sx = viewX + viewW + 18.f;
  float sw = panelW - viewW - 50.f;
  float sy = viewY + 8.f;
  const float numW = 78.f;
  const float numH = 20.f;
  for (int i = 0; i < kSliderCount; i++) {
    sliders[i].x = sx;
    sliders[i].y = sy;
    sliders[i].w = sw;
    sliders[i].h = 16.f;
    sliders[i].numW = numW;
    sliders[i].numH = numH;
    sliders[i].numX = sx + sw - numW;
    sliders[i].numY = sy - 20.f;
    sy += 48.f;
  }

  saveW = 120.f;
  saveH = 36.f;
  saveX = sx;
  saveY = panelY + panelH - 52.f;
  backW = 100.f;
  backH = 36.f;
  backX = sx + saveW + 12.f;
  backY = saveY;
}

void PieceEditor::drawRect(float x, float y, float w, float h,
                           float r, float g, float b, float a) {
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f(x, y); glVertex2f(x + w, y);
  glVertex2f(x + w, y + h); glVertex2f(x, y + h);
  glEnd();
}

void PieceEditor::drawText(float x, float y, const char* t,
                           float r, float g, float b, float s) {
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
}

bool PieceEditor::hitRect(float mx, float my, float x, float y, float w, float h) const {
  return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

int PieceEditor::hitSlider(float mx, float my) const {
  for (int i = 0; i < kSliderCount; i++) {
    const auto& s = sliders[i];
    if (mx >= s.x - 4 && mx <= s.x + s.w + 4 &&
        my >= s.y - 10 && my <= s.y + s.h + 10)
      return i;
  }
  return -1;
}

int PieceEditor::hitValueBox(float mx, float my) const {
  for (int i = 0; i < kSliderCount; i++) {
    const auto& s = sliders[i];
    if (mx >= s.numX && mx <= s.numX + s.numW &&
        my >= s.numY && my <= s.numY + s.numH)
      return i;
  }
  return -1;
}

void PieceEditor::setSliderFromX(int idx, float mx) {
  if (idx < 0 || idx >= kSliderCount) return;
  auto& s = sliders[idx];
  if (s.w <= 0.f) return;
  float t = (mx - s.x) / s.w;
  t = std::max(0.f, std::min(1.f, t));
  *s.value = s.minV + t * (s.maxV - s.minV);
  dirty = true;
}

void PieceEditor::beginEdit(int idx) {
  if (idx < 0 || idx >= kSliderCount) return;
  commitEdit();
  editSlider = idx;
  editBlink = 0.f;
  const auto& s = sliders[idx];
  editLen = snprintf(editBuf, sizeof(editBuf), "%.2f", *s.value);
  if (editLen < 0) editLen = 0;
  if (editLen >= (int)sizeof(editBuf)) editLen = (int)sizeof(editBuf) - 1;
}

void PieceEditor::cancelEdit() {
  editSlider = -1;
  editLen = 0;
  editBuf[0] = '\0';
}

void PieceEditor::commitEdit() {
  if (editSlider < 0 || editSlider >= kSliderCount) {
    cancelEdit();
    return;
  }
  auto& s = sliders[editSlider];
  if (editLen <= 0) {
    cancelEdit();
    return;
  }
  char* end = nullptr;
  float n = std::strtof(editBuf, &end);
  if (end == editBuf) {
    cancelEdit();
    return;
  }
  if (n < s.minV) n = s.minV;
  if (n > s.maxV) n = s.maxV;
  *s.value = n;
  dirty = true;
  cancelEdit();
}

void PieceEditor::draw3DPreview(std::map<int, ShaderProgram*>* programs,
                                const Theme& theme, float outlineFactor) {
  if (!programs || previewMeshes.empty()) return;

  // One-time 1×1 white texture so cel shadow sampling never hits unbound unit
  if (!previewShadowTex) {
    unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &previewShadowTex);
    glBindTexture(GL_TEXTURE_2D, previewShadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);

  // Pixel viewport (OpenGL Y is bottom-up; UI Y is top-down)
  int vx = (int)std::floor(viewX);
  int vy = lastH - (int)std::floor(viewY + viewH);
  int vw = (int)std::floor(viewW);
  int vh = (int)std::floor(viewH);
  if (vw < 8 || vh < 8) return;

  glEnable(GL_SCISSOR_TEST);
  glScissor(vx, vy, vw, vh);
  glViewport(vx, vy, vw, vh);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  // Slight sky tint so empty isn't pure black if something fails
  glClearColor(
    theme.clearBottom.x * 0.35f + 0.08f,
    theme.clearBottom.y * 0.35f + 0.1f,
    theme.clearBottom.z * 0.35f + 0.16f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  float aspect = (float)vw / (float)std::max(vh, 1);
  auto proj = getPerspectiveProjMatrix(38.f, aspect, 0.3f, 100.f);
  // Interactive orbit camera around tile centre (Z-up)
  Vector3f center = {0.f, 0.f, 1.2f};
  float yawR = orbitYaw * (float)M_PI / 180.f;
  float pitchR = orbitPitch * (float)M_PI / 180.f;
  float cp = std::cos(pitchR);
  Vector3f eye = {
    center.x + orbitDist * cp * std::sin(yawR),
    center.y - orbitDist * cp * std::cos(yawR),
    center.z + orbitDist * std::sin(pitchR)
  };
  Vector3f up = {0.f, 0.f, 1.f};
  auto view = getLookAtMatrix(eye, center, up);

  ShaderProgram* border = programs->at(BLACK_BORDER);
  ShaderProgram* cel = programs->at(CEL_SHADING);

  int ptype = PieceTransformStore::pieceTypeFromKey(currentPieceKey());
  Mesh* cell = previewMeshes.count(BOARDCELL) ? previewMeshes[BOARDCELL] : nullptr;
  Mesh* piece = previewMeshes.count(ptype) ? previewMeshes[ptype] : nullptr;

  Vector3f lightDir = theme.lightDir;
  float len = std::sqrt(lightDir.x*lightDir.x + lightDir.y*lightDir.y + lightDir.z*lightDir.z) + 1e-5f;
  lightDir.x /= len; lightDir.y /= len; lightDir.z /= len;

  auto drawMeshBorder = [&](Mesh* m, std::vector<GLfloat>& mm, bool whiteSide) {
    if (!m) return;
    glUseProgram(border->id);
    glCullFace(GL_FRONT);
    border->setViewMatrix(&view);
    border->setProjectionMatrix(&proj);
    border->setFloat("outlineFactor", outlineFactor);
    // Preview: white-side red, black-side purple (match game defaults)
    if (whiteSide)
      border->setVector4f("outlineColor", 0.95f, 0.12f, 0.12f, 1.f);
    else
      border->setVector4f("outlineColor", 0.62f, 0.18f, 0.92f, 1.f);
    border->setMoveMatrix(&mm);
    auto nm = inverse(&mm);
    nm = transpose(&nm);
    border->setNormalMatrix(&nm);
    m->draw();
  };

  auto drawMeshCel = [&](Mesh* m, std::vector<GLfloat>& mm, const Vector4f& col) {
    if (!m) return;
    glUseProgram(cel->id);
    glCullFace(GL_BACK);
    cel->setViewMatrix(&view);
    cel->setProjectionMatrix(&proj);
    // Light from above the preview camera — identity light matrices + white
    // "shadow map" so the cel shader never samples an unbound unit.
    auto ident = getIdentityMatrix();
    cel->setMatrix4fv("LMatrix", &ident);
    cel->setMatrix4fv("PLMatrix", &ident);
    cel->setInt("shadowMapResolution", 4);
    cel->bindTexture(0, GL_TEXTURE0, "shadowMap", previewShadowTex);
    cel->setInt("useDiffuseMap", 0);
    if (m->hasDiffuseTexture()) {
      cel->bindTexture(1, GL_TEXTURE1, "diffuseMap", m->diffuseTextureId);
      cel->setInt("useDiffuseMap", 1);
    }
    cel->setVector3f("lightDirection", lightDir.x, lightDir.y, lightDir.z);
    cel->setFloat("emissiveBoost", theme.emissiveBoost + 0.35f);
    cel->setFloat("time", 0.f);
    cel->setVector4f("color", col.x, col.y, col.z, col.w);
    cel->setMoveMatrix(&mm);
    auto nm = inverse(&mm);
    nm = transpose(&nm);
    cel->setNormalMatrix(&nm);
    m->draw();
  };

  // Board cell at origin (black outline)
  if (cell) {
    auto mm = getIdentityMatrix();
    glUseProgram(border->id);
    glCullFace(GL_FRONT);
    border->setViewMatrix(&view);
    border->setProjectionMatrix(&proj);
    border->setFloat("outlineFactor", outlineFactor);
    border->setVector4f("outlineColor", 0.f, 0.f, 0.f, 1.f);
    border->setMoveMatrix(&mm);
    {
      auto nm = inverse(&mm);
      nm = transpose(&nm);
      border->setNormalMatrix(&nm);
    }
    cell->draw();
    drawMeshCel(cell, mm, theme.boardLight);
  }

  // Piece with edit transform (white team) — red outline by default
  if (piece) {
    auto mm = PieceTransformStore::buildPieceMatrix(
      +ptype, 0.f, 0.f, 0.f, 2.0f, edit);
    drawMeshBorder(piece, mm, true);
    drawMeshCel(piece, mm, theme.pieceUser);
  }

  glDisable(GL_SCISSOR_TEST);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  ncaResetPipelineState();
  glUseProgram(0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void PieceEditor::drawUI() {
  // Dim outside panel only (do not cover the 3D view hole later)
  drawRect(0, 0, (float)lastW, (float)lastH, 0.f, 0.f, 0.f, 0.55f);
  drawRect(panelX, panelY, panelW, panelH, 0.05f, 0.06f, 0.1f, 0.96f);
  glColor4f(0.3f, 0.85f, 1.f, 0.9f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(panelX, panelY);
  glVertex2f(panelX + panelW, panelY);
  glVertex2f(panelX + panelW, panelY + panelH);
  glVertex2f(panelX, panelY + panelH);
  glEnd();

  drawText(panelX + 20, panelY + 14, "PIECE EDITOR", 0.4f, 1.f, 1.f, 2.0f);
  char status[128];
  snprintf(status, sizeof(status), "%s", dirty ? "* unsaved changes" : "saved / clean");
  drawText(panelX + 220, panelY + 20, status,
           dirty ? 1.f : 0.5f, dirty ? 0.7f : 0.8f, dirty ? 0.3f : 0.6f, 1.2f);

  // Set dropdown
  drawText(setBoxX, setBoxY - 18.f, "Set", 0.7f, 0.75f, 0.85f, 1.15f);
  drawRect(setBoxX, setBoxY, setBoxW, setBoxH, 0.12f, 0.14f, 0.2f, 1.f);
  std::string setLabel = "(no sets)";
  if (sets && !sets->sets().empty())
    setLabel = sets->sets().at((size_t)setIndex).name + "  v";
  drawText(setBoxX + 8, setBoxY + 8, setLabel.c_str(), 0.95f, 0.95f, 1.f, 1.25f);

  // Piece dropdown
  drawText(pieceBoxX, pieceBoxY - 18.f, "Piece", 0.7f, 0.75f, 0.85f, 1.15f);
  drawRect(pieceBoxX, pieceBoxY, pieceBoxW, pieceBoxH, 0.12f, 0.14f, 0.2f, 1.f);
  std::string pieceLabel = currentPieceKey() + "  v";
  drawText(pieceBoxX + 8, pieceBoxY + 8, pieceLabel.c_str(), 0.95f, 0.95f, 1.f, 1.25f);

  // Preview frame border only (hollow) — 3D is drawn into the hole after this UI pass
  glColor4f(0.35f, 0.55f, 0.75f, 1.f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(viewX - 2, viewY - 2);
  glVertex2f(viewX + viewW + 2, viewY - 2);
  glVertex2f(viewX + viewW + 2, viewY + viewH + 2);
  glVertex2f(viewX - 2, viewY + viewH + 2);
  glEnd();
  drawText(viewX + 6, viewY + viewH - 18,
           "Drag in view to orbit  |  Wheel zoom",
           0.75f, 0.8f, 0.9f, 1.05f);

  // Placeholder message if meshes failed to load (3D pass may still clear)
  if (previewMeshes.empty()) {
    drawRect(viewX, viewY, viewW, viewH, 0.08f, 0.1f, 0.14f, 1.f);
    drawText(viewX + 24, viewY + viewH * 0.5f - 8,
             "No mesh loaded for this set", 1.f, 0.5f, 0.4f, 1.3f);
  }

  // Sliders + clickable number boxes
  editBlink += 0.05f;
  char line[64];
  for (int i = 0; i < kSliderCount; i++) {
    const auto& s = sliders[i];
    float v = *s.value;
    float t = (v - s.minV) / (s.maxV - s.minV + 1e-6f);
    t = std::max(0.f, std::min(1.f, t));

    drawText(s.x, s.y - 18.f, s.label, 0.9f, 0.92f, 1.f, 1.2f);

    bool editing = (editSlider == i);
    if (editing)
      drawRect(s.numX, s.numY, s.numW, s.numH, 0.18f, 0.28f, 0.38f, 1.f);
    else
      drawRect(s.numX, s.numY, s.numW, s.numH, 0.12f, 0.16f, 0.22f, 1.f);
    glColor4f(editing ? 1.f : 0.4f, editing ? 0.9f : 0.85f, editing ? 0.3f : 1.f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(s.numX, s.numY);
    glVertex2f(s.numX + s.numW, s.numY);
    glVertex2f(s.numX + s.numW, s.numY + s.numH);
    glVertex2f(s.numX, s.numY + s.numH);
    glEnd();

    if (editing) {
      bool cursorOn = (std::fmod(editBlink, 1.2f) < 0.7f);
      snprintf(line, sizeof(line), "%s%s", editBuf, cursorOn ? "|" : "");
    } else {
      snprintf(line, sizeof(line), "%.2f", v);
    }
    float tw = (float)std::strlen(line) * 7.f * 1.15f;
    float tx = s.numX + std::max(3.f, (s.numW - tw) * 0.5f);
    drawText(tx, s.numY + 4.f, line, 1.f, 0.95f, 0.55f, 1.15f);

    drawRect(s.x, s.y, s.w, s.h, 0.12f, 0.14f, 0.2f, 1.f);
    drawRect(s.x, s.y, s.w * t, s.h, 0.25f, 0.75f, 0.95f, 1.f);
    float kx = s.x + s.w * t - 5.f;
    if (kx < s.x) kx = s.x;
    drawRect(kx, s.y - 3.f, 10.f, s.h + 6.f, 1.f, 0.9f, 0.4f, 1.f);
  }

  // Save / Back
  drawRect(saveX, saveY, saveW, saveH, 0.15f, 0.45f, 0.28f, 1.f);
  drawText(saveX + 28, saveY + 10, "Save", 0.9f, 1.f, 0.9f, 1.5f);
  drawRect(backX, backY, backW, backH, 0.25f, 0.2f, 0.2f, 1.f);
  drawText(backX + 22, backY + 10, "Back", 1.f, 0.9f, 0.9f, 1.5f);

  drawText(panelX + 20, panelY + panelH - 22,
           "Transforms apply in-game for this set after Save.",
           0.55f, 0.6f, 0.7f, 1.1f);

  // Expanded dropdown lists on top
  if (dropOpen == DROP_SET && sets) {
    float ly = setBoxY + setBoxH;
    float itemH = 28.f;
    int n = (int)sets->sets().size();
    drawRect(setBoxX, ly, setBoxW, itemH * n + 4.f, 0.08f, 0.09f, 0.14f, 0.98f);
    for (int i = 0; i < n; i++) {
      float iy = ly + 2.f + i * itemH;
      if (i == setIndex)
        drawRect(setBoxX + 2, iy, setBoxW - 4, itemH - 2, 0.15f, 0.35f, 0.45f, 1.f);
      drawText(setBoxX + 8, iy + 6, sets->sets().at((size_t)i).name.c_str(),
               0.95f, 0.95f, 1.f, 1.2f);
    }
  }
  if (dropOpen == DROP_PIECE) {
    float ly = pieceBoxY + pieceBoxH;
    float itemH = 28.f;
    const auto& keys = PieceTransformStore::allPieceKeys();
    int n = (int)keys.size();
    drawRect(pieceBoxX, ly, pieceBoxW, itemH * n + 4.f, 0.08f, 0.09f, 0.14f, 0.98f);
    for (int i = 0; i < n; i++) {
      float iy = ly + 2.f + i * itemH;
      if (i == pieceIndex)
        drawRect(pieceBoxX + 2, iy, pieceBoxW - 4, itemH - 2, 0.15f, 0.35f, 0.45f, 1.f);
      drawText(pieceBoxX + 8, iy + 6, keys[(size_t)i].c_str(), 0.95f, 0.95f, 1.f, 1.2f);
    }
  }
}

void PieceEditor::draw(int screenW, int screenH,
                       std::map<int, ShaderProgram*>* programs,
                       const Theme& theme,
                       float outlineFactor) {
  if (!open) return;
  layout(screenW, screenH);

  // 2D chrome first (panel leaves a hole for the 3D view — no fill over view)
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

  drawUI();

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  // 3D into the view rectangle (drawn after panel so it is not covered)
  draw3DPreview(programs, theme, outlineFactor);

  // Dropdown lists must sit above the 3D view — redraw only open lists
  if (dropOpen != DROP_NONE) {
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screenW, screenH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    if (dropOpen == DROP_SET && sets) {
      float ly = setBoxY + setBoxH;
      float itemH = 28.f;
      int n = (int)sets->sets().size();
      drawRect(setBoxX, ly, setBoxW, itemH * n + 4.f, 0.08f, 0.09f, 0.14f, 0.98f);
      for (int i = 0; i < n; i++) {
        float iy = ly + 2.f + i * itemH;
        if (i == setIndex)
          drawRect(setBoxX + 2, iy, setBoxW - 4, itemH - 2, 0.15f, 0.35f, 0.45f, 1.f);
        drawText(setBoxX + 8, iy + 6, sets->sets().at((size_t)i).name.c_str(),
                 0.95f, 0.95f, 1.f, 1.2f);
      }
    }
    if (dropOpen == DROP_PIECE) {
      float ly = pieceBoxY + pieceBoxH;
      float itemH = 28.f;
      const auto& keys = PieceTransformStore::allPieceKeys();
      int n = (int)keys.size();
      drawRect(pieceBoxX, ly, pieceBoxW, itemH * n + 4.f, 0.08f, 0.09f, 0.14f, 0.98f);
      for (int i = 0; i < n; i++) {
        float iy = ly + 2.f + i * itemH;
        if (i == pieceIndex)
          drawRect(pieceBoxX + 2, iy, pieceBoxW - 4, itemH - 2, 0.15f, 0.35f, 0.45f, 1.f);
        drawText(pieceBoxX + 8, iy + 6, keys[(size_t)i].c_str(), 0.95f, 0.95f, 1.f, 1.2f);
      }
    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
  }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

bool PieceEditor::save() {
  if (!store || !sets || sets->sets().empty()) return false;
  const auto& info = sets->sets().at((size_t)setIndex);
  store->set(info.id, currentPieceKey(), edit);
  bool ok = store->saveForSet(info.id, info.path);
  if (ok) dirty = false;
  if (audio) audio->playSfx(ok ? "sfx_theme" : "sfx_illegal");
  return ok;
}

bool PieceEditor::onScroll(float mx, float my, float yoffset) {
  if (!open) return false;
  layout(lastW, lastH);
  if (!hitRect(mx, my, viewX, viewY, viewW, viewH)) return true; // still consume while open
  // Scroll up = zoom in
  if (yoffset > 0.f) orbitDist *= 0.90f;
  else orbitDist *= 1.11f;
  orbitDist = std::max(5.f, std::min(40.f, orbitDist));
  return true;
}

bool PieceEditor::onMouseButton(int button, int action, float mx, float my) {
  if (!open) return false;
  layout(lastW, lastH);

  // LMB or RMB drag inside the 3D view = orbit preview camera
  if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) &&
      action == GLFW_PRESS &&
      dropOpen == DROP_NONE &&
      hitRect(mx, my, viewX, viewY, viewW, viewH)) {
    // Don't start orbit if clicking UI chrome that overlaps (shouldn't)
    orbiting = true;
    lastOrbitMx = mx;
    lastOrbitMy = my;
    clickConsumed = true;
    return true;
  }
  if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) &&
      action == GLFW_RELEASE && orbiting) {
    orbiting = false;
    clickConsumed = true;
    return true;
  }

  if (button != GLFW_MOUSE_BUTTON_LEFT) {
    // Block board camera / other inputs while editor is open
    clickConsumed = true;
    return true;
  }

  if (action == GLFW_PRESS) {
    // Dropdown list hits first
    if (dropOpen == DROP_SET && sets) {
      float ly = setBoxY + setBoxH;
      float itemH = 28.f;
      int n = (int)sets->sets().size();
      for (int i = 0; i < n; i++) {
        if (hitRect(mx, my, setBoxX, ly + 2.f + i * itemH, setBoxW, itemH - 2)) {
          loadPreviewForSet(i);
          dropOpen = DROP_NONE;
          clickConsumed = true;
          if (audio) audio->playSfx("sfx_select");
          return true;
        }
      }
      dropOpen = DROP_NONE;
      clickConsumed = true;
      return true;
    }
    if (dropOpen == DROP_PIECE) {
      float ly = pieceBoxY + pieceBoxH;
      float itemH = 28.f;
      const auto& keys = PieceTransformStore::allPieceKeys();
      int n = (int)keys.size();
      for (int i = 0; i < n; i++) {
        if (hitRect(mx, my, pieceBoxX, ly + 2.f + i * itemH, pieceBoxW, itemH - 2)) {
          pieceIndex = i;
          loadEditFromStore();
          dropOpen = DROP_NONE;
          clickConsumed = true;
          if (audio) audio->playSfx("sfx_select");
          return true;
        }
      }
      dropOpen = DROP_NONE;
      clickConsumed = true;
      return true;
    }

    if (hitRect(mx, my, setBoxX, setBoxY, setBoxW, setBoxH)) {
      dropOpen = DROP_SET;
      clickConsumed = true;
      return true;
    }
    if (hitRect(mx, my, pieceBoxX, pieceBoxY, pieceBoxW, pieceBoxH)) {
      dropOpen = DROP_PIECE;
      clickConsumed = true;
      return true;
    }
    if (hitRect(mx, my, saveX, saveY, saveW, saveH)) {
      save();
      clickConsumed = true;
      return true;
    }
    if (hitRect(mx, my, backX, backY, backW, backH)) {
      closeEditor();
      clickConsumed = true;
      if (audio) audio->playSfx("sfx_select");
      return true;
    }

    // Click number box → type exact value
    int hv = hitValueBox(mx, my);
    if (hv >= 0) {
      beginEdit(hv);
      if (audio) audio->playSfx("sfx_select");
      clickConsumed = true;
      return true;
    }
    // Click elsewhere while editing → commit
    if (editSlider >= 0) commitEdit();

    int hs = hitSlider(mx, my);
    if (hs >= 0) {
      dragSlider = hs;
      setSliderFromX(hs, mx);
      clickConsumed = true;
      return true;
    }

    clickConsumed = true;
    return true;
  }

  if (action == GLFW_RELEASE) {
    dragSlider = -1;
    clickConsumed = true;
    return true;
  }
  return true;
}

bool PieceEditor::onKey(int key, int action, int mods) {
  (void)mods;
  if (!open || editSlider < 0) return false;
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;

  if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
    commitEdit();
    if (audio) audio->playSfx("sfx_select");
    return true;
  }
  if (key == GLFW_KEY_ESCAPE) {
    cancelEdit();
    return true;
  }
  if (key == GLFW_KEY_BACKSPACE) {
    if (editLen > 0) editBuf[--editLen] = '\0';
    return true;
  }

  // Digits
  int digit = -1;
  if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) digit = key - GLFW_KEY_0;
  if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) digit = key - GLFW_KEY_KP_0;
  if (digit >= 0) {
    if (editLen < 12) {
      editBuf[editLen++] = (char)('0' + digit);
      editBuf[editLen] = '\0';
    }
    return true;
  }
  // Decimal point (one only)
  if (key == GLFW_KEY_PERIOD || key == GLFW_KEY_KP_DECIMAL) {
    bool hasDot = false;
    for (int i = 0; i < editLen; i++) if (editBuf[i] == '.') hasDot = true;
    if (!hasDot && editLen < 12) {
      editBuf[editLen++] = '.';
      editBuf[editLen] = '\0';
    }
    return true;
  }
  // Leading minus
  if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) {
    if (editLen == 0) {
      editBuf[editLen++] = '-';
      editBuf[editLen] = '\0';
    }
    return true;
  }
  return true; // consume while editing
}

bool PieceEditor::onMouseMove(float mx, float my) {
  if (!open) return false;
  layout(lastW, lastH);

  if (orbiting) {
    float dx = mx - lastOrbitMx;
    float dy = my - lastOrbitMy;
    lastOrbitMx = mx;
    lastOrbitMy = my;
    // Horizontal drag → yaw; vertical → pitch
    orbitYaw += dx * 0.35f;
    orbitPitch += dy * 0.30f;
    // Keep above the board a bit; allow almost top-down / low side view
    if (orbitPitch < 8.f) orbitPitch = 8.f;
    if (orbitPitch > 85.f) orbitPitch = 85.f;
    return true;
  }

  if (dragSlider < 0) return true;
  setSliderFromX(dragSlider, mx);
  return true;
}
