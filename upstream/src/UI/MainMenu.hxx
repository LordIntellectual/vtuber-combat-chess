#ifndef NCA_MAIN_MENU_HXX_
#define NCA_MAIN_MENU_HXX_

#include <string>
#include <vector>
#include "../gl_compat.hxx"
#include "../Network/LobbyClient.hxx"

/**
 * Post-splash main menu: Single Player, Multiplayer (online rooms), Settings.
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
    ACTION_HOST_ONLINE = 3,   // create room on lobby + relay as host
    ACTION_JOIN_ONLINE = 4,   // join selected room via lobby + relay
    ACTION_QUIT = 5,
    ACTION_REFRESH_ROOMS = 6
  };

  MainMenu();

  void show();
  void hide();
  bool isVisible() const { return visible; }
  Page page() const { return page_; }

  void draw(int screenW, int screenH);

  bool onMouseButton(int button, int action, float mx, float my);
  bool onMouseMove(float mx, float my);
  bool onKey(int key, int action, int mods);
  bool onChar(unsigned int codepoint);

  Action consumeAction();

  /** Host dialog fields */
  const std::string& hostRoomName() const { return hostName; }
  const std::string& hostPassword() const { return hostPass; }
  /** Join: selected room + password field */
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
  Page page_;
  Action pending;
  int lastW, lastH;
  int hoverBtn;
  int hoverRoom;
  int selectedRoom_;
  int focusField; // 0 none, 1 hostName, 2 hostPass, 3 joinPass
  std::string hostName;
  std::string hostPass;
  std::string joinPass;
  std::string statusLine;
  float blinkT;
  std::vector<LobbyRoom> rooms_;
  LobbyClient lobby_;

  float panelX, panelY, panelW, panelH;
  static const int kMaxButtons = 10;
  struct Btn {
    const char* label;
    float x, y, w, h;
    int id;
  };
  Btn buttons[kMaxButtons];
  int buttonCount;

  // Field rects
  float hnX, hnY, hnW, hnH;
  float hpX, hpY, hpW, hpH;
  float jpX, jpY, jpW, jpH;
  float listX, listY, listW, listH;
  float rowH;

  static const int ID_GOTO_MP = -10;
  static const int ID_BACK = -11;
  static const int ID_REFRESH = -12;

  void layout(int w, int h);
  void rebuild();
  int hitButton(float mx, float my) const;
  int hitRoom(float mx, float my) const;
  int hitField(float mx, float my) const;
  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  void drawText(float x, float y, const char* text, float r, float g, float b, float scale);
  void drawButton(const Btn& b, bool hover);
  void drawField(float x, float y, float w, float h, const std::string& text,
                 bool focused, const char* placeholder);
};

#endif
