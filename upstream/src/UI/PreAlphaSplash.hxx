#ifndef NCA_PRE_ALPHA_SPLASH_HXX_
#define NCA_PRE_ALPHA_SPLASH_HXX_

#include "../gl_compat.hxx"

/**
 * Blocking pre-alpha disclaimer shown at game start, before board input.
 * Music may already be playing (started in main before this dialog).
 */
class PreAlphaSplash {
public:
  enum Result {
    RESULT_NONE = 0,
    RESULT_UNDERSTAND = 1,
    RESULT_QUIT = 2
  };

  PreAlphaSplash();

  /**
   * Run a modal loop on the given window until Understand or Quit.
   * Does not own audio; only draws the dialog and polls input.
   */
  Result run(GLFWwindow* window);

private:
  int lastW, lastH;
  float panelX, panelY, panelW, panelH;
  float okX, okY, okW, okH;
  float quitX, quitY, quitW, quitH;
  Result result;

  void layout(int w, int h);
  void draw();
  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  void drawText(float x, float y, const char* text, float r, float g, float b, float scale);
  bool hit(float mx, float my, float x, float y, float w, float h) const;
};

#endif
