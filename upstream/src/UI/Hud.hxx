#ifndef NCA_HUD_HXX_
#define NCA_HUD_HXX_

#include <string>
#include <vector>
#include "../gl_compat.hxx"
#include "../Theme/Theme.hxx"

class PieceSetManager;

class Hud {
public:
  bool visible;
  std::string statusLine;
  std::string lastEvent;

  Hud();
  void draw(int width, int height, const ThemeManager& themes, bool aiOn, bool musicOn,
            const PieceSetManager* pieceSets = nullptr);
  void setStatus(const std::string& s) { statusLine = s; }
  void setEvent(const std::string& e) { lastEvent = e; }

private:
  void drawText(float x, float y, const char* text, float r, float g, float b, float scale);
  void drawPanel(float x, float y, float w, float h, float a);
};

#endif
