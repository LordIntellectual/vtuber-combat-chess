#ifndef NCA_PIECE_EDITOR_HXX_
#define NCA_PIECE_EDITOR_HXX_

#include <map>
#include <string>
#include <vector>
#include "../gl_compat.hxx"
#include "../PieceSet/PieceSet.hxx"
#include "../PieceSet/PieceTransform.hxx"
#include "../mesh/Mesh.hxx"
#include "../shader/ShaderProgram.hxx"
#include "../Theme/Theme.hxx"
#include "../Audio/AudioEngine.hxx"

/* In-settings piece placement editor: set/piece dropdowns, 3D tile preview,
   X/Y/Z pos & rot + scale sliders, Save to transforms.json. */
class PieceEditor {
public:
  PieceEditor();
  ~PieceEditor();

  void setDeps(PieceSetManager* sets, PieceTransformStore* store, AudioEngine* audio);

  bool isOpen() const { return open; }
  void openEditor();
  void closeEditor();
  bool handleBack(); // Esc: close dropdowns then editor

  void draw(int screenW, int screenH,
            std::map<int, ShaderProgram*>* programs,
            const Theme& theme,
            float outlineFactor);

  bool onMouseButton(int button, int action, float mx, float my);
  bool onMouseMove(float mx, float my);
  /** Mouse wheel over the preview: zoom camera. Returns true if consumed. */
  bool onScroll(float mx, float my, float yoffset);
  /** Keyboard for number-field editing. Returns true if consumed. */
  bool onKey(int key, int action, int mods);
  bool isEditingValue() const { return editSlider >= 0; }

  bool consumedClick() const { return clickConsumed; }
  void clearConsumedClick() { clickConsumed = false; }

private:
  enum DropKind { DROP_NONE = 0, DROP_SET = 1, DROP_PIECE = 2 };

  bool open;
  bool clickConsumed;
  int dragSlider;
  int lastW, lastH;
  DropKind dropOpen;

  // Inline number edit for slider values
  int editSlider; // -1 = not editing
  char editBuf[24];
  int editLen;
  float editBlink;

  // Preview orbit camera (degrees / distance) — independent of board camera
  bool orbiting;
  float lastOrbitMx, lastOrbitMy;
  float orbitYaw;   // around Z
  float orbitPitch; // elevation
  float orbitDist;

  PieceSetManager* sets;
  PieceTransformStore* store;
  AudioEngine* audio;

  int setIndex;
  int pieceIndex;
  PieceTransform edit;
  bool dirty;

  std::map<int, Mesh*> previewMeshes;
  std::string loadedSetId;
  GLuint previewShadowTex; // 1×1 white — cel shader always samples shadowMap

  // Layout rects
  float panelX, panelY, panelW, panelH;
  float viewX, viewY, viewW, viewH;
  float setBoxX, setBoxY, setBoxW, setBoxH;
  float pieceBoxX, pieceBoxY, pieceBoxW, pieceBoxH;
  float saveX, saveY, saveW, saveH;
  float backX, backY, backW, backH;

  struct Slider {
    const char* label;
    float* value;
    float minV, maxV;
    float x, y, w, h;
    float numX, numY, numW, numH;
  };
  static const int kSliderCount = 7;
  Slider sliders[kSliderCount];

  void freePreview();
  void loadPreviewForSet(int index);
  void loadEditFromStore();
  void layout(int screenW, int screenH);
  void draw3DPreview(std::map<int, ShaderProgram*>* programs, const Theme& theme, float outlineFactor);
  void drawUI();
  void drawText(float x, float y, const char* t, float r, float g, float b, float s);
  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  int hitSlider(float mx, float my) const;
  int hitValueBox(float mx, float my) const;
  bool hitRect(float mx, float my, float x, float y, float w, float h) const;
  void setSliderFromX(int idx, float mx);
  void beginEdit(int idx);
  void cancelEdit();
  void commitEdit();
  bool save();
  std::string currentSetId() const;
  std::string currentPieceKey() const;
};

#endif
