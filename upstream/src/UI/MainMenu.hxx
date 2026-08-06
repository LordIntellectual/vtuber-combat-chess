#ifndef NCA_MAIN_MENU_HXX_
#define NCA_MAIN_MENU_HXX_

#include <string>
#include "../gl_compat.hxx"

/**
 * Post-splash main menu: Single Player, Multiplayer (host/join), Settings.
 * Drawn as a full-screen overlay; gameplay is frozen while visible.
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
    ACTION_HOST = 3,
    ACTION_JOIN = 4,
    ACTION_QUIT = 5
  };

  MainMenu();

  void show();
  void hide();
  bool isVisible() const { return visible; }
  Page page() const { return page_; }

  void draw(int screenW, int screenH);

  /** Returns true if click was consumed. */
  bool onMouseButton(int button, int action, float mx, float my);
  bool onMouseMove(float mx, float my);
  /** Returns true if key was consumed. */
  bool onKey(int key, int action, int mods);
  /** Text input for join address field. */
  bool onChar(unsigned int codepoint);

  /** Pop one-shot action (call each frame while visible). */
  Action consumeAction();

  /** Join target e.g. "127.0.0.1:7777" */
  const std::string& joinAddress() const { return joinAddr; }
  /** Host listen port (default 7777). */
  unsigned short hostPort() const { return hostPortNum; }

  /** Status / error line under multiplayer options. */
  void setStatus(const std::string& s) { statusLine = s; }
  const std::string& status() const { return statusLine; }

private:
  bool visible;
  Page page_;
  Action pending;
  int lastW, lastH;
  int hoverBtn; // -1 none
  bool joinFieldFocused;
  std::string joinAddr;
  unsigned short hostPortNum;
  std::string statusLine;
  float blinkT;

  // Layout rects (screen pixels, top-left origin like other UI)
  float panelX, panelY, panelW, panelH;
  static const int kMaxButtons = 8;
  struct Btn {
    const char* label;
    float x, y, w, h;
    int id; // action or page nav codes
  };
  Btn buttons[kMaxButtons];
  int buttonCount;
  float fieldX, fieldY, fieldW, fieldH;

  // Button ids: positive = Action enum, negative = internal
  static const int ID_GOTO_MP = -10;
  static const int ID_BACK = -11;
  static const int ID_JOIN_FIELD = -12;

  void layout(int w, int h);
  void rebuild();
  int hitButton(float mx, float my) const;
  bool hitField(float mx, float my) const;
  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  void drawText(float x, float y, const char* text, float r, float g, float b, float scale);
  void drawButton(const Btn& b, bool hover);
};

#endif
