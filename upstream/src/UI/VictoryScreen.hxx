#ifndef NCA_VICTORY_SCREEN_HXX_
#define NCA_VICTORY_SCREEN_HXX_

#include "../constants.hxx"

/**
 * Full-screen victory overlay for checkmate or forfeit.
 * Replay button returns control to main via consumeReplay().
 */
class VictoryScreen {
public:
  VictoryScreen();

  void show(int endReason, bool whiteWon);
  void hide();
  bool isOpen() const { return open; }

  void draw(int screenW, int screenH);

  /** LMB handling; returns true if click was consumed. */
  bool onMouseButton(int button, int action, float mx, float my);

  /** True once after user clicks Replay (cleared by caller). */
  bool consumeReplay();

private:
  bool open;
  int reason; // END_CHECKMATE / END_FORFEIT
  bool whiteWon;
  bool replayRequested;

  float btnX, btnY, btnW, btnH;
  int lastW, lastH;

  void layout(int screenW, int screenH);
  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  void drawText(float x, float y, const char* text, float r, float g, float b, float scale);
};

#endif
