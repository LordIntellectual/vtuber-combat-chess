#ifndef NCA_SETTINGS_MENU_HXX_
#define NCA_SETTINGS_MENU_HXX_

#include <string>
#include <vector>
#include <algorithm>

class AudioEngine;
class PieceEditor;

/* Settings overlay with Sound / Video / Gameplay / Piece editor submenus. */
class SettingsMenu {
public:
  enum Page {
    PAGE_ROOT = 0,
    PAGE_SOUND = 1,
    PAGE_VIDEO = 2,
    PAGE_GAMEPLAY = 3,
    PAGE_PIECE_EDITOR = 4
  };

  SettingsMenu();

  void setAudio(AudioEngine* audio) { audioEngine = audio; syncFromAudio(); }
  void setPieceEditor(PieceEditor* ed) { pieceEditor = ed; }

  bool isOpen() const { return open || (pieceEditorOpen()); }
  void openMenu();
  void closeMenu();
  void toggle();
  /** Esc: go back one page, or close if on root. Returns true if still open. */
  bool handleBack();

  void draw(int screenW, int screenH);

  bool onMouseButton(int button, int action, float mx, float my);
  bool onMouseMove(float mx, float my);
  /** Keyboard while menu open. Returns true if key was consumed (e.g. value edit). */
  bool onKey(int key, int action, int mods);
  bool isEditingValue() const { return editSlider >= 0; }

  bool isDragging() const { return dragIndex >= 0; }
  bool consumedClick() const { return clickConsumed; }
  void clearConsumedClick() { clickConsumed = false; }

  /** True once after user confirms Quit (caller should exit the game). */
  bool consumeQuitRequest();

  /** 0..1 UI value; maps to outline factor in model units for the border shader. */
  float outlineThickness() const { return outline; }
  /** Shader outlineFactor from UI (0 → 0, 1 → max outline). */
  float outlineFactor() const;

  /** White (positive) side outline RGB in 0..1. Defaults red. */
  float outlineWhiteR() const { return outWhiteR; }
  float outlineWhiteG() const { return outWhiteG; }
  float outlineWhiteB() const { return outWhiteB; }
  /** Black (negative) side outline RGB in 0..1. Defaults purple. */
  float outlineBlackR() const { return outBlackR; }
  float outlineBlackG() const { return outBlackG; }
  float outlineBlackB() const { return outBlackB; }

  /** Capture cinematic camera (default on). */
  bool actionCameraEnabled() const { return actionCamera; }
  void setActionCameraEnabled(bool v) { actionCamera = v; }

  /** Engine-suggested move pulse on board (default off). Live mid-game toggle. */
  bool suggestedMovesEnabled() const { return suggestedMoves; }
  void setSuggestedMovesEnabled(bool v) { suggestedMoves = v; }

  /**
   * Capture explosion force scale. UI 0..1 maps to 0..2× default blast
   * (0.5 = current default). Applies to the next capture immediately.
   */
  float explosionForce() const { return explosionForceUI; }
  /** Physics multiplier: 0..2 (1.0 at 50% slider). */
  float explosionForceScale() const { return explosionForceUI * 2.f; }

  /**
   * Main Menu beat-pulse strength (0..1). How hard the panel reacts to
   * music energy (full-band). Default ~0.6. Sound settings.
   */
  float menuPulseSensitivity() const { return menuPulseSens; }
  void setMenuPulseSensitivity(float v) {
    menuPulseSens = std::max(0.f, std::min(1.f, v));
  }

  /** Main Menu electric border / crackle FX (default on). Sound settings. */
  bool menuElectricBordersEnabled() const { return menuElectric; }
  void setMenuElectricBordersEnabled(bool v) { menuElectric = v; }

  bool pieceEditorOpen() const;

private:
  struct Slider {
    const char* label;
    float* value;       // 0..1
    float x, y, w, h;   // track
    float numX, numY, numW, numH; // clickable value box
    bool percent;       // display as 0–100%
    int minInt;         // inclusive (percent display units)
    int maxInt;         // inclusive
  };
  struct Button {
    const char* label;
    float x, y, w, h;
    int action; // Page to open, -1 = back, -2 = action cam, -3 = suggested moves
                // -4 = reset outline colours, -5 = menu electric toggle
                // -10 = Return (close menu), -11 = Quit (confirm popup)
  };

  bool open;
  bool clickConsumed;
  int dragIndex;
  int lastW, lastH;
  Page page;

  // Quit confirmation overlay (on root settings)
  bool quitConfirmOpen;
  bool quitRequested;
  float quitPanelX, quitPanelY, quitPanelW, quitPanelH;
  float quitYesX, quitYesY, quitYesW, quitYesH;
  float quitNoX, quitNoY, quitNoW, quitNoH;

  // Inline number editing
  int editSlider;          // -1 = not editing
  char editBuf[16];
  int editLen;
  float editBlink;

  float master;
  float music;
  float sfx;
  float menuPulseSens; // 0..1 Main Menu music-pulse strength
  bool menuElectric;   // Main Menu electric border FX
  float outline; // 0..1 thickness control
  // Per-side outline colours (0..1 RGB)
  float outWhiteR, outWhiteG, outWhiteB;
  float outBlackR, outBlackG, outBlackB;
  bool actionCamera;   // capture close-up cam (default on)
  bool suggestedMoves; // pulse suggested tiles (default off)
  float explosionForceUI; // 0..1 (0.5 → 1× default blast)

  AudioEngine* audioEngine;
  PieceEditor* pieceEditor;

  static const int kMaxSliders = 12;
  static const int kMaxButtons = 8;
  int sliderCount;
  int buttonCount;
  Slider sliders[kMaxSliders];
  Button buttons[kMaxButtons];

  void syncFromAudio();
  void applyToAudio();
  void rebuildPage();
  void layout(int screenW, int screenH);
  float panelHeightForPage() const;
  int hitSlider(float mx, float my) const;
  int hitValueBox(float mx, float my) const;
  int hitButton(float mx, float my) const;
  void setSliderFromX(int index, float mx);
  void beginEdit(int index);
  void cancelEdit();
  void commitEdit();
  void applySliderSideEffects(int index);
  void drawText(float x, float y, const char* text, float r, float g, float b, float scale);
  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  void layoutQuitConfirm(int screenW, int screenH);
  void drawQuitConfirm();
  bool hitQuitYes(float mx, float my) const;
  bool hitQuitNo(float mx, float my) const;
};

#endif
