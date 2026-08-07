#ifndef NCA_MAIN_MENU_HXX_
#define NCA_MAIN_MENU_HXX_

#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include "../Network/LobbyClient.hxx"

/**
 * Post-splash main menu with 2.5D parallax (root + multiplayer).
 * Layers (back → front), same y-down ortho:
 *   1) Far hero art (oversize, larger pan)
 *   2) Effect plane — simulated particles (wind + cursor repulsion)
 *   3) Near UI FBO (smaller pan; hits invert UI pan)
 * No full-screen darken over hero art (see docs/UI_MENUS.md).
 */
class MainMenu {
public:
  enum Page {
    PAGE_ROOT = 0,
    PAGE_MULTIPLAYER = 1
  };

  enum Action {
    ACTION_NONE = 0,
    ACTION_SINGLE_PLAYER = 1,
    ACTION_OPEN_SETTINGS = 2,
    ACTION_HOST_ONLINE = 3,
    ACTION_JOIN_ONLINE = 4,
    ACTION_QUIT = 5,
    ACTION_REFRESH_ROOMS = 6
  };

  MainMenu();
  ~MainMenu();

  void show();
  void hide();
  bool isVisible() const { return visible; }
  Page page() const { return page_; }
  bool hostDialogOpen() const { return hostDialogOpen_; }

  bool loadBackground(const std::string& pngPath);

  void draw(int screenW, int screenH);

  bool onMouseButton(int button, int action, float mx, float my);
  bool onMouseMove(float mx, float my);
  bool onKey(int key, int action, int mods);
  bool onChar(unsigned int codepoint);

  Action consumeAction();

  const std::string& hostRoomName() const { return hostName; }
  const std::string& hostPassword() const { return hostPass; }
  const std::string& joinPassword() const { return joinPass; }
  const LobbyRoom* selectedRoom() const;
  int selectedRoomIndex() const { return selectedRoom_; }

  void setStatus(const std::string& s) { statusLine = s; }
  const std::string& status() const { return statusLine; }

  LobbyClient& lobby() { return lobby_; }
  void setRooms(const std::vector<LobbyRoom>& rooms);
  void clearSelection() { selectedRoom_ = -1; }

private:
  bool visible;
  bool hostDialogOpen_;
  bool quitConfirmOpen_;
  Page page_;
  Action pending;
  int lastW, lastH;
  int hoverBtn;
  int hoverRoom;
  int hoverPopupBtn;
  int hoverQuitBtn;
  int selectedRoom_;
  int focusField;
  std::string hostName;
  std::string hostPass;
  std::string joinPass;
  std::string statusLine;
  float blinkT;
  std::vector<LobbyRoom> rooms_;
  LobbyClient lobby_;
  GLuint bgTex;
  bool bgLoaded;

  // Parallax / FBO
  GLuint fbo_;
  GLuint fboColor_;
  GLuint fboDepth_;
  int fboW_, fboH_;
  float motionT_;
  float camPanX_, camPanY_;       // normalised parallax phase [-1..1]
  float planeTiltX_, planeTiltY_; // reserved (2.5D path does not use tilt)
  float menuPlaneZ_;              // legacy 3D path (unused by 2.5D)
  float bgPlaneZ_;
  float fovYDeg_;
  float uiLayerPanX_, uiLayerPanY_; // pixel pan applied to UI FBO this frame

  // Effect plane (between art and UI): simple snow-like particles
  static const int kMaxEffectParticles = 140;
  struct EffectParticle {
    float x, y;
    float vx, vy;
    float size;
    float alpha;
    float seed; // per-particle phase for uneven wind
  };
  EffectParticle effectParts_[kMaxEffectParticles];
  int effectCount_;
  bool effectSeeded_;
  float cursorX_, cursorY_;
  bool cursorKnown_;

  float panelX, panelY, panelW, panelH;
  static const int kMaxButtons = 10;
  struct Btn {
    const char* label;
    float x, y, w, h;
    int id;
  };
  Btn buttons[kMaxButtons];
  int buttonCount;
  Btn popupButtons[4];
  int popupButtonCount;

  float jpX, jpY, jpW, jpH;
  float listX, listY, listW, listH;
  float rowH;

  float dlgX, dlgY, dlgW, dlgH;
  float hnX, hnY, hnW, hnH;
  float hpX, hpY, hpW, hpH;

  float quitPanelX, quitPanelY, quitPanelW, quitPanelH;
  float quitYesX, quitYesY, quitYesW, quitYesH;
  float quitNoX, quitNoY, quitNoW, quitNoH;

  static const int ID_GOTO_MP = -10;
  static const int ID_BACK = -11;
  static const int ID_REFRESH = -12;
  static const int ID_OPEN_HOST_DIALOG = -13;
  static const int ID_HOST_CREATE = -14;
  static const int ID_HOST_DIALOG_BACK = -15;

  void layout(int w, int h);
  void rebuild();
  void openHostDialog();
  void closeHostDialog();
  void openQuitConfirm();
  void closeQuitConfirm();
  int hitButton(float mx, float my) const;
  int hitPopupButton(float mx, float my) const;
  int hitRoom(float mx, float my) const;
  int hitField(float mx, float my) const;
  bool hitQuitYes(float mx, float my) const;
  bool hitQuitNo(float mx, float my) const;
  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  void drawTexturedRect(float x, float y, float w, float h, GLuint tex);
  void drawText(float x, float y, const char* text, float r, float g, float b, float scale);
  void drawButton(const Btn& b, bool hover);
  void drawField(float x, float y, float w, float h, const std::string& text,
                 bool focused, const char* placeholder);
  void drawHostDialog();
  void drawQuitConfirm();
  void drawUiContent(int screenW, int screenH);
  void ensureFbo(int w, int h);
  void destroyFbo();
  void updateParallax(float dt);
  void drawParallaxScene(int screenW, int screenH);
  /** Map window mouse → layout UI pixels (invert UI layer pan). */
  bool projectMouseToUi(float mx, float my, float& uiX, float& uiY) const;
  void seedEffectParticles(int w, int h);
  void updateEffectLayer(float dt, int w, int h);
  void drawEffectLayer();
  void respawnEffectParticle(EffectParticle& p, int w, int h, int edge);
};

#endif
