#include "gl_compat.hxx"
#include <exception>
#include <map>
#include <iostream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <string>

#include "mesh/Mesh.hxx"
#include "mesh/meshes.hxx"
#include "shader/ShaderProgram.hxx"
#include "shader/shaderPrograms.hxx"
#include "ColorPicking/ColorPicking.hxx"
#include "ShadowMapping/ShadowMapping.hxx"
#include "SmokeGenerator/SmokeGenerator.hxx"
#include "PhysicsWorld/PhysicsWorld.hxx"
#include "Event/EventStack.hxx"
#include "Event/Event.hxx"
#include "constants.hxx"
#include "Clock/Clock.hxx"
#include "Camera/Camera.hxx"
#include "Camera/ActionCamera.hxx"
#include "DirectionalLight.hxx"
#include "utils/utils.hxx"
#include "utils/GlState.hxx"
#include "utils/math.hxx"
#include "ChessGame/ChessGame.hxx"
#include "Theme/Theme.hxx"
#include "Audio/AudioEngine.hxx"
#include "FX/SparkSystem.hxx"
#include "FX/ScreenShake.hxx"
#include "UI/Hud.hxx"
#include "UI/SettingsMenu.hxx"
#include "UI/PieceEditor.hxx"
#include "UI/VictoryScreen.hxx"
#include "UI/PreAlphaSplash.hxx"
#include "UI/MainMenu.hxx"
#include "Environment/Starfield.hxx"
#include "PieceSet/PieceSet.hxx"
#include "PieceSet/PieceTransform.hxx"
#include "Network/NetSession.hxx"
#include "Network/NetProtocol.hxx"
#include "Network/LobbyClient.hxx"
#include "get_share_path.hxx"

// Globals
bool resizing = false;
int width = 1280;
int height = 720;
bool cameraMoving = false;
int dX = 0, dY = 0;
Vector2i mousePosition;
bool selecting = false;

// Shared systems for key / scroll callbacks
ThemeManager* gThemes = nullptr;
ChessGame* gGame = nullptr;
VictoryScreen* gVictory = nullptr;
AudioEngine* gAudio = nullptr;
Hud* gHud = nullptr;
SettingsMenu* gSettings = nullptr;
PieceEditor* gPieceEditor = nullptr;
PieceTransformStore* gTransforms = nullptr;
SparkSystem* gSparks = nullptr;
Starfield* gStarfield = nullptr;
Camera* gCamera = nullptr;
PieceSetManager* gPieceSets = nullptr;
std::map<int, Mesh*>* gPieces = nullptr;
std::map<int, std::vector<Mesh*>>* gFragmentMeshes = nullptr;
PhysicsWorld* gPhysics = nullptr;
bool gRunning = true;
NetSession* gNet = nullptr;
MainMenu* gMainMenu = nullptr;
/** True while playing (single or multi); false on main menu. */
bool gInGame = false;
/** Settings opened from main menu (return to menu when closed via Esc). */
bool gSettingsFromMenu = false;

static std::string ncaShareRoot() {
  std::string p = get_share_path(); // .../share/toonchess/
  for (size_t i = 0; i < p.size(); ++i)
    if (p[i] == '\\') p[i] = '/';
  auto pos = p.rfind("toonchess");
  if (pos != std::string::npos) p.replace(pos, 9, "nca");
  else p += "../nca/";
  if (!p.empty() && p.back() != '/') p.push_back('/');
  return p;
}

static const char* stateName(int s) {
  switch (s) {
    case USER_TURN: return "White to move";
    case USER_MOVING: return "White moving…";
    case WAITING: return "Waiting…";
    case AI_TURN: return "AI thinking…";
    case AI_MOVING: return "Black (AI) moving…";
    case BLACK_TURN: return "Black to move (human)";
    case BLACK_MOVING: return "Black moving…";
    case GAME_OVER: return "Game over";
    default: return "Unknown";
  }
}

/** Menu theme vs stage (current theme) music. Crossfade for scene changes. */
static void playMenuMusic(bool crossfade) {
  if (!gAudio) return;
  if (crossfade) gAudio->crossfadeMusic(kMenuMusicFile);
  else gAudio->playMusic(kMenuMusicFile);
}

static void playStageMusic(bool crossfade) {
  if (!gAudio || !gThemes) return;
  if (crossfade) gAudio->crossfadeMusic(gThemes->current().musicFile);
  else gAudio->playMusic(gThemes->current().musicFile);
}

void applyThemeAudio() {
  if (!gThemes || !gAudio) return;
  // Theme switch during play → blend into that stage's track
  if (gInGame && !(gSettings && gSettings->isOpen()))
    playStageMusic(true);
  else
    playStageMusic(false);
  gAudio->playSfx("sfx_theme");
  if (gHud) gHud->setEvent(std::string("Theme → ") + gThemes->current().name);
  std::cout << "[VCC] Theme: " << gThemes->current().name << std::endl;
}

/** Leave match and show main menu (e.g. Settings → Quit confirmed while in-game). */
static void returnToMainMenu() {
  if (gVictory) gVictory->hide();
  if (gSettings && gSettings->isOpen()) gSettings->closeMenu();
  if (gNet) gNet->close("return_to_menu");
  if (gGame) {
    gGame->clearNetworkRole();
    gGame->resetBoard();
    gGame->setAiEnabled(true);
  }
  gInGame = false;
  gSettingsFromMenu = false;
  cameraMoving = false;
  selecting = false;
  if (gCamera) gCamera->setDragging(false);
  if (gMainMenu) {
    gMainMenu->show();
    gMainMenu->setStatus("");
  }
  if (gHud) gHud->setEvent("Main Menu");
  if (gAudio) {
    gAudio->playSfx("sfx_select");
    playMenuMusic(true);
  }
  std::cout << "[VCC] Returned to Main Menu\n";
}

/** Open in-game settings: stage music → menu theme. */
static void openInGameSettings() {
  if (!gSettings) return;
  gSettingsFromMenu = false;
  if (!gSettings->isOpen()) {
    gSettings->openMenu();
    playMenuMusic(true);
    if (gHud) gHud->setEvent("Settings");
    if (gAudio) gAudio->playSfx("sfx_select");
  }
}

/** Close settings: if still in a match, menu theme → stage music. */
static void onSettingsClosed() {
  if (gSettingsFromMenu) {
    // Main Menu was left visible under Settings (hero stage); just clear flag.
    gSettingsFromMenu = false;
    return;
  }
  if (gInGame)
    playStageMusic(true);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  (void)scancode;
  // Allow key-repeat while typing into value fields
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

  // Esc: settings back → main menu keys → open settings (in-game) → else ignore
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    if (gSettings && gSettings->isOpen()) {
      bool stillOpen = gSettings->handleBack();
      if (gHud) gHud->setEvent(stillOpen ? "Settings" : "Settings closed");
      // Music restore / main-menu show handled in main loop when settings closes
      return;
    }
    if (gMainMenu && gMainMenu->isVisible()) {
      gMainMenu->onKey(key, action, mods);
      return;
    }
    // In-game: Esc opens Settings (Quit there returns to Main Menu after confirm)
    if (gInGame && gSettings) {
      openInGameSettings();
      return;
    }
    return;
  }

  // Settings / piece editor value fields capture digits while open
  if (gSettings && gSettings->isOpen()) {
    if (gSettings->onKey(key, action, mods)) return;
    if (action != GLFW_PRESS) return;
    // Block gameplay shortcuts while settings open (except H for HUD)
    if (key == GLFW_KEY_H || key == GLFW_KEY_U) {
      if (gHud) gHud->visible = !gHud->visible;
    }
    if (key == GLFW_KEY_S) {
      bool wasOpen = gSettings->isOpen();
      gSettings->toggle();
      if (gHud) gHud->setEvent(gSettings->isOpen() ? "Settings" : "Settings closed");
      if (gAudio) gAudio->playSfx("sfx_select");
      // Opening from main-menu settings path keeps menu music; close → main loop
      if (!wasOpen && gSettings->isOpen() && gInGame)
        playMenuMusic(true);
    }
    return;
  }

  // Main menu captures keys (join address typing, shortcuts)
  if (gMainMenu && gMainMenu->isVisible()) {
    if (gMainMenu->onKey(key, action, mods)) {
      if (gAudio && action == GLFW_PRESS) gAudio->playSfx("sfx_select");
    }
    return;
  }

  if (action != GLFW_PRESS) return;
  if (!gThemes || !gGame || !gAudio || !gHud) return;
  if (!gInGame) return;

  // S: settings menu (in-game)
  if (key == GLFW_KEY_S) {
    if (gSettings) {
      if (gSettings->isOpen()) {
        gSettings->closeMenu();
        gHud->setEvent("Settings closed");
        gAudio->playSfx("sfx_select");
        // Stage music restore in main loop
      } else {
        openInGameSettings();
        gHud->setEvent("Settings — Sound / Video / Gameplay");
      }
    }
    return;
  }

  if (key == GLFW_KEY_1) { gThemes->setTheme(THEME_NEON); applyThemeAudio(); }
  if (key == GLFW_KEY_2) { gThemes->setTheme(THEME_JUNGLE); applyThemeAudio(); }
  if (key == GLFW_KEY_3) { gThemes->setTheme(THEME_STARSHIP); applyThemeAudio(); }
  if (key == GLFW_KEY_T) { gThemes->cycle(); applyThemeAudio(); }
  if (key == GLFW_KEY_A) {
    if (gGame && gGame->isNetworked()) {
      if (gHud) gHud->setEvent("AI locked off during multiplayer");
      return;
    }
    gGame->setAiEnabled(!gGame->isAiEnabled());
    gAudio->playSfx("sfx_select");
    gHud->setEvent(gGame->isAiEnabled() ? "AI enabled" : "AI disabled — human plays black");
    std::cout << "[VCC] AI " << (gGame->isAiEnabled() ? "ON" : "OFF") << std::endl;
  }
  if (key == GLFW_KEY_M) {
    gAudio->toggleMusic();
    if (gAudio->musicEnabled()) {
      // Resume the track for the current context
      if (gMainMenu && gMainMenu->isVisible())
        playMenuMusic(false);
      else if (gSettings && gSettings->isOpen())
        playMenuMusic(false);
      else if (gInGame)
        playStageMusic(false);
      else
        playMenuMusic(false);
    }
    gHud->setEvent(gAudio->musicEnabled() ? "Music ON" : "Music OFF");
  }
  if (key == GLFW_KEY_F) {
    gThemes->cycleFx();
    if (gSparks) gSparks->setIntensity(gThemes->fxScale());
    gAudio->playSfx("sfx_select");
    gHud->setEvent(std::string("FX → ") + ThemeManager::fxName(gThemes->fx()));
  }
  if (key == GLFW_KEY_R) {
    // Guest cannot reset the shared board (host authority)
    if (gGame && gGame->networkRole() == ChessGame::NET_CLIENT) {
      if (gHud) gHud->setEvent("Only host can reset the board");
      return;
    }
    gGame->resetBoard();
    if (gNet && gNet->inGame() && gGame->networkRole() == ChessGame::NET_HOST) {
      gNet->send(NetProtocol::makeStart(
        gGame->getFen(), "w", gGame->movePly));
      std::cout << "[VCC-NET] START (reset) broadcast\n";
    }
    gAudio->playSfx("sfx_theme");
    gHud->setEvent("Board reset");
    std::cout << "[VCC] Board reset\n";
  }
  // H or U: hide UI for clean stream view of the board
  if (key == GLFW_KEY_H || key == GLFW_KEY_U) {
    gHud->visible = !gHud->visible;
    gHud->setEvent(gHud->visible ? "HUD shown" : "HUD hidden — press H to show");
    std::cout << "[VCC] HUD " << (gHud->visible ? "ON" : "OFF") << "\n";
  }
  // P: cycle piece set (reload meshes in-place)
  if (key == GLFW_KEY_P) {
    if (!gPieceSets || !gPieces || !gFragmentMeshes || !gPhysics) return;
    try {
      gPhysics->clearFragments();
      PieceSetManager::freePieces(gPieces);
      PieceSetManager::freeFragments(gFragmentMeshes);
      gPieceSets->cycle();
      *gPieces = gPieceSets->loadPieces();
      *gFragmentMeshes = gPieceSets->loadFragments();
      gAudio->playSfx("sfx_theme");
      gHud->setEvent(std::string("Piece set → ") + gPieceSets->current().name);
      std::cout << "[VCC] Piece set: " << gPieceSets->current().name << "\n";
    } catch (const std::exception& e) {
      std::cerr << "[PieceSet] swap failed: " << e.what() << "\n";
      gHud->setEvent(std::string("Piece set error: ") + e.what());
    }
  }
}

void resize_callback(GLFWwindow* window, int new_width, int new_height) {
  (void)window;
  width = new_width;
  height = new_height;
  resizing = true;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
  (void)window; (void)mods;

  // Victory overlay captures clicks (Replay)
  if (gVictory && gVictory->isOpen()) {
    gVictory->onMouseButton(button, action, (float)mousePosition.x, (float)mousePosition.y);
    cameraMoving = false;
    if (gCamera) gCamera->setDragging(false);
    return;
  }

  // Settings / piece editor capture all mouse while open (no board orbit)
  if (gSettings && gSettings->isOpen()) {
    gSettings->onMouseButton(button, action, (float)mousePosition.x, (float)mousePosition.y);
    // Ensure board camera is not left in a dragging state
    cameraMoving = false;
    if (gCamera) gCamera->setDragging(false);
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
      if (gSettings->consumedClick()) gSettings->clearConsumedClick();
    }
    return;
  }

  // Main menu captures clicks
  if (gMainMenu && gMainMenu->isVisible()) {
    gMainMenu->onMouseButton(button, action, (float)mousePosition.x, (float)mousePosition.y);
    cameraMoving = false;
    if (gCamera) gCamera->setDragging(false);
    return;
  }

  if (!gInGame) return;

  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    dX = 0; dY = 0; cameraMoving = true;
    if (gCamera) gCamera->setDragging(true);
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
    cameraMoving = false;
    if (gCamera) gCamera->setDragging(false);
  }
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    selecting = true;
  }
}

void cursor_move_callback(GLFWwindow* window, double xpos, double ypos) {
  (void)window;
  int xposi = (int)xpos;
  int yposi = (int)ypos;
  int dx = xposi - mousePosition.x;
  int dy = yposi - mousePosition.y;
  mousePosition.x = xposi;
  mousePosition.y = yposi;

  // While settings/piece editor is open, do not feed board camera deltas
  if (gSettings && gSettings->isOpen()) {
    gSettings->onMouseMove((float)xposi, (float)yposi);
    dX = 0;
    dY = 0;
    return;
  }

  if (gMainMenu && gMainMenu->isVisible()) {
    gMainMenu->onMouseMove((float)xposi, (float)yposi);
    dX = 0;
    dY = 0;
    return;
  }

  if (!gInGame) {
    dX = 0;
    dY = 0;
    return;
  }

  // Accumulate deltas so multi-event frames are not lost; cleared after move().
  dX += dx;
  dY += dy;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  (void)window;
  (void)xoffset;
  // Piece editor zoom takes priority over board zoom
  if (gPieceEditor && gPieceEditor->isOpen()) {
    gPieceEditor->onScroll((float)mousePosition.x, (float)mousePosition.y, (float)yoffset);
    return;
  }
  if (gSettings && gSettings->isOpen()) return;
  if (gMainMenu && gMainMenu->isVisible()) return;
  if (!gInGame) return;
  if (gCamera) gCamera->zoom((GLfloat)yoffset);
}

void char_callback(GLFWwindow* window, unsigned int codepoint) {
  (void)window;
  if (gSettings && gSettings->isOpen()) return;
  if (gMainMenu && gMainMenu->isVisible())
    gMainMenu->onChar(codepoint);
}

void drawSkyGradient(const Theme& th) {
  glDisable(GL_DEPTH_TEST);
  glUseProgram(0);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glBegin(GL_QUADS);
  glColor3f(th.clearTop.x, th.clearTop.y, th.clearTop.z);
  glVertex2f(-1, 1); glVertex2f(1, 1);
  glColor3f(th.clearBottom.x, th.clearBottom.y, th.clearBottom.z);
  glVertex2f(1, -1); glVertex2f(-1, -1);
  glEnd();
  // Decorative horizon band (star corona / neon floor haze)
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glBegin(GL_QUADS);
  glColor4f(th.sparkColor.x, th.sparkColor.y, th.sparkColor.z, 0.25f);
  glVertex2f(-1, -0.15f); glVertex2f(1, -0.15f);
  glColor4f(th.sparkColor.x, th.sparkColor.y, th.sparkColor.z, 0.0f);
  glVertex2f(1, 0.35f); glVertex2f(-1, 0.35f);
  glEnd();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_DEPTH_TEST);
}

void celShadingRender(
  ChessGame* game,
  PhysicsWorld* physicsWorld,
  std::map<int, Mesh*>* pieces,
  std::map<int, ShaderProgram*>* programs,
  ShadowMapping* shadowMapping,
  Camera* camera,
  DirectionalLight* light,
  float elapsedTime,
  const Theme& th,
  Vector3f boardShake = {0,0,0},
  float boardHover = 0.f,
  float outlineFactor = 0.08f,
  PieceTransformStore* transforms = nullptr,
  const std::string* activeSetId = nullptr,
  const Vector3f* neonLightPos = nullptr,
  const Vector3f* neonLightColor = nullptr,
  const float* neonLightIntensity = nullptr,
  int neonLightCount = 0,
  float outlineWhiteR = 0.95f, float outlineWhiteG = 0.12f, float outlineWhiteB = 0.12f,
  float outlineBlackR = 0.62f, float outlineBlackG = 0.18f, float outlineBlackB = 0.92f);

static void printNetUsage() {
  std::cout << "Multiplayer (experimental):\n"
            << "  --host [PORT]          Host a game (default port 7777)\n"
            << "  --join HOST:PORT       Join a host\n"
            << "  --port PORT            Default port for --host\n"
            << "See docs/MULTIPLAYER_DESIGN.md\n";
}

/** Parse multiplayer CLI. Returns false on hard error (bad args). */
static bool parseNetArgs(int argc, char** argv,
                         bool& wantHost, bool& wantJoin,
                         uint16_t& port, std::string& joinTarget) {
  wantHost = false;
  wantJoin = false;
  port = 7777;
  joinTarget.clear();
  if (const char* ep = std::getenv("VCC_PORT")) {
    int p = std::atoi(ep);
    if (p > 0 && p <= 65535) port = (uint16_t)p;
  }
  if (const char* ej = std::getenv("VCC_JOIN")) {
    if (ej[0]) {
      wantJoin = true;
      joinTarget = ej;
    }
  }
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      printNetUsage();
      return false;
    }
    if (a == "--host") {
      wantHost = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        int p = std::atoi(argv[++i]);
        if (p > 0 && p <= 65535) port = (uint16_t)p;
      }
    } else if (a == "--join") {
      wantJoin = true;
      if (i + 1 >= argc) {
        std::cerr << "[VCC-NET] --join requires HOST:PORT\n";
        return false;
      }
      joinTarget = argv[++i];
    } else if (a == "--port") {
      if (i + 1 >= argc) {
        std::cerr << "[VCC-NET] --port requires a number\n";
        return false;
      }
      int p = std::atoi(argv[++i]);
      if (p > 0 && p <= 65535) port = (uint16_t)p;
    }
  }
  if (wantHost && wantJoin) {
    std::cerr << "[VCC-NET] --host and --join are mutually exclusive\n";
    return false;
  }
  return true;
}

static void sendHostStart(NetSession& net, ChessGame* game) {
  if (!game) return;
  net.send(NetProtocol::makeStart(game->getFen(), "w", game->movePly));
  std::cout << "[VCC-NET] START fen ply=" << game->movePly << std::endl;
}

static void handleNetMessages(NetSession& net, ChessGame* game, Hud* hud,
                              AudioEngine* audio) {
  NetProtocol::Message msg;
  while (net.pollMessage(msg)) {
    if (msg.type == "HELLO") {
      // Host already advanced; app sends START
      continue;
    }
    if (msg.type == "WELCOME") {
      std::string you = msg.get("you", "black");
      int side = (you == "white") ? 1 : -1;
      if (game) {
        game->setNetworkRole(ChessGame::NET_CLIENT, side);
        game->resetBoard();
      }
      if (hud) hud->setEvent(std::string("Joined as ") + you);
      continue;
    }
    if (msg.type == "START") {
      if (game) {
        game->resetBoard();
        // Opening only for MVP; FEN load deferred to Phase 2
      }
      if (hud) hud->setEvent("Game start (network)");
      std::cout << "[VCC-NET] START received\n";
      continue;
    }
    if (msg.type == "MOVE_REQ") {
      // Host only
      if (!game || game->networkRole() != ChessGame::NET_HOST) continue;
      std::string uci = msg.get("uci");
      if (!game->tryApplyUciMove(uci, false)) {
        net.send(NetProtocol::makeReject(uci, "illegal"));
        if (hud) hud->setEvent("Rejected guest move " + uci);
        if (audio) audio->playSfx("sfx_illegal");
        std::cout << "[VCC-NET] REJECT " << uci << std::endl;
      } else {
        net.send(NetProtocol::makeMove(uci, game->movePly, "b"));
        std::cout << "[VCC-NET] MOVE " << uci << " ply=" << game->movePly
                  << std::endl;
      }
      continue;
    }
    if (msg.type == "MOVE") {
      // Client applies host-authoritative move (or host ignores own echo)
      if (!game) continue;
      if (game->networkRole() == ChessGame::NET_HOST) continue;
      std::string uci = msg.get("uci");
      if (!game->tryApplyUciMove(uci, true)) {
        std::cerr << "[VCC-NET] failed to apply MOVE " << uci << std::endl;
        if (hud) hud->setEvent("Desync on MOVE " + uci);
      } else {
        std::cout << "[VCC-NET] MOVE applied " << uci << std::endl;
      }
      continue;
    }
    if (msg.type == "REJECT") {
      if (hud) hud->setEvent("Move rejected: " + msg.get("reason", "?"));
      if (audio) audio->playSfx("sfx_illegal");
      continue;
    }
    if (msg.type == "STATE") {
      // Phase 2: full FEN load. MVP: log only.
      std::cout << "[VCC-NET] STATE ply=" << msg.get("ply")
                << " turn=" << msg.get("turn") << std::endl;
      continue;
    }
  }
}

int main(int argc, char** argv) {
  bool wantHost = false, wantJoin = false;
  uint16_t netPort = 7777;
  std::string joinTarget;
  if (!parseNetArgs(argc, argv, wantHost, wantJoin, netPort, joinTarget)) {
    // --help or bad args
    if (argc > 1 && (std::string(argv[1]) == "--help" ||
                     std::string(argv[1]) == "-h"))
      return 0;
    return 1;
  }

  if (!glfwInit()) return 1;
  glfwWindowHint(GLFW_SAMPLES, ANTIALIASING_HIGH);
  // Compatibility profile required for fixed-function HUD (stb_easy_font)
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

  GLFWwindow* window = glfwCreateWindow(width, height, "vTuber Combat Chess", NULL, NULL);
  if (!window) { glfwTerminate(); return 1; }
  glfwMakeContextCurrent(window);
  if (!vccInitGL()) {
    std::cerr << "[GL] Failed to load OpenGL entry points (GLAD)\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  std::cout << "=== vTuber Combat Chess ===\n";
  std::cout << "Author: Lord Intellectual\n";
  std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

  // Audio ready for SFX; no music until splash is accepted
  ThemeManager themes;
  AudioEngine audio;
  std::string nca = ncaShareRoot();
  audio.init(nca + "audio/");
  gThemes = &themes;
  gAudio = &audio;

  // Pre-alpha disclaimer first (SFX ok, music silent)
  {
    PreAlphaSplash splash;
    PreAlphaSplash::Result sr = splash.run(window);
    if (sr != PreAlphaSplash::RESULT_UNDERSTAND) {
      gThemes = nullptr;
      gAudio = nullptr;
      glfwDestroyWindow(window);
      glfwTerminate();
      return 0;
    }
  }
  // Accept → Main Menu theme starts immediately
  playMenuMusic(false);

  Hud hud;
  SettingsMenu settings;
  PieceEditor pieceEditor;
  VictoryScreen victory;
  MainMenu mainMenu;
  PieceTransformStore transforms;
  gHud = &hud;
  gSettings = &settings;
  gPieceEditor = &pieceEditor;
  gVictory = &victory;
  gMainMenu = &mainMenu;
  gTransforms = &transforms;
  settings.setAudio(&audio);
  settings.setPieceEditor(&pieceEditor);
  mainMenu.loadBackground(nca + "ui/menu_background.png");

  std::map<int, ShaderProgram*> programs;
  try {
    programs = initPrograms();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  ChessGame* game = new ChessGame();
  gGame = game;
  try {
    game->start();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "[VCC] Continuing without Stockfish (AI will fail if left ON)\n";
  }

  NetSession netSession;
  gNet = &netSession;
  // CLI host/join skips main menu and enters multiplayer directly
  gInGame = false;
  if (wantHost) {
    std::string err;
    if (!netSession.startHost(netPort, &err)) {
      std::cerr << "[VCC-NET] host failed: " << err << std::endl;
      hud.setEvent("Host failed: " + err);
      mainMenu.show();
      mainMenu.setStatus("Host failed: " + err);
    } else {
      game->setNetworkRole(ChessGame::NET_HOST, /*white*/ 1);
      game->resetBoard();
      gInGame = true;
      mainMenu.hide();
      hud.setEvent("Hosting on port " + std::to_string(netPort));
    }
  } else if (wantJoin) {
    std::string host;
    uint16_t jp = netPort;
    if (!TcpSocket::parseHostPort(joinTarget, host, jp)) {
      std::cerr << "[VCC-NET] invalid --join target (use HOST:PORT)\n";
      hud.setEvent("Invalid join address");
      mainMenu.show();
      mainMenu.setStatus("Invalid join address");
    } else {
      std::string err;
      if (!netSession.startClient(host, jp, "Guest", &err)) {
        std::cerr << "[VCC-NET] join failed: " << err << std::endl;
        hud.setEvent("Join failed: " + err);
        mainMenu.show();
        mainMenu.setStatus("Join failed: " + err);
      } else {
        gInGame = true;
        mainMenu.hide();
        hud.setEvent("Connecting to " + joinTarget);
      }
    }
  } else {
    mainMenu.show();
    hud.setEvent("Main Menu");
  }
  // CLI host/join skipped the menu — go straight to stage music
  if (gInGame)
    playStageMusic(false);

  SmokeGenerator* smokeGenerator = new SmokeGenerator();
  smokeGenerator->initBuffers();

  SparkSystem sparks;
  sparks.init();
  sparks.setIntensity(themes.fxScale());
  gSparks = &sparks;

  Starfield starfield;
  try {
    starfield.init(nca);
  } catch (const std::exception& e) {
    std::cerr << "[Starfield] " << e.what() << "\n";
  }
  gStarfield = &starfield;

  PieceSetManager pieceSets;
  pieceSets.scan(nca + "piece_sets/");
  gPieceSets = &pieceSets;
  // Load saved per-piece placement for every set (and again when cycling)
  for (const auto& s : pieceSets.sets())
    transforms.loadForSet(s.id, s.path);
  pieceEditor.setDeps(&pieceSets, &transforms, &audio);

  std::map<int, Mesh*> pieces;
  std::map<int, std::vector<Mesh*>> fragmentMeshes;
  try {
    pieces = pieceSets.loadPieces();
    fragmentMeshes = pieceSets.loadFragments();
  } catch (const std::exception& e) {
    std::cerr << "[PieceSet] load failed, legacy starship fallback: " << e.what() << "\n";
    pieces = initPieces();
    fragmentMeshes = initFragmentMeshes();
  }
  gPieces = &pieces;
  gFragmentMeshes = &fragmentMeshes;

  PhysicsWorld* physicsWorld = new PhysicsWorld(&fragmentMeshes, game);
  gPhysics = physicsWorld;
  ColorPicking* colorPicking = new ColorPicking(width, height);
  colorPicking->initBuffers();
  ShadowMapping* shadowMapping = new ShadowMapping();
  shadowMapping->initBuffers();
  Clock mainClock;
  Camera* camera = new Camera((double)width / height);
  gCamera = camera;
  ActionCamera actionCam;
  ScreenShake captureShake;

  DirectionalLight light;
  light.projectionMatrix = getOrthoProjMatrix(-25, 25, -20, 20, 1, 50);

  glfwSetFramebufferSizeCallback(window, resize_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetCursorPosCallback(window, cursor_move_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetCharCallback(window, char_callback);

  std::cout << "[VCC] Main Menu after splash | Esc in-game = Main Menu | S settings\n";
  std::cout << "[VCC] In-game: RMB orbit | wheel zoom | P pieces | H hide UI | 1/2/3 themes | A AI | M music\n";
  if (wantHost || wantJoin)
    std::cout << "[VCC-NET] CLI multiplayer active — see docs/MULTIPLAYER_DESIGN.md\n";
  else
    std::cout << "[VCC-NET] Multiplayer: Main Menu → Multiplayer → Host / Join\n";

  Clock frameClock;
  bool netWasInGame = false;
  while (gRunning && !glfwWindowShouldClose(window)) {
    float elapsedTime = (float)mainClock.getElapsedTime();
    float dt = (float)frameClock.getElapsedTime();
    frameClock.restart();
    starfield.update(dt);
    captureShake.update(dt);
    audio.update(dt);
    glfwPollEvents();

    // Network pump (non-blocking) — only while in a match
    if (gInGame && netSession.active()) {
      netSession.pump();
      if (netSession.consumePeerJustJoined()) {
        // Host: peer finished HELLO — reset + START
        game->resetBoard();
        sendHostStart(netSession, game);
        hud.setEvent("Guest joined — you are White");
        audio.playSfx("sfx_theme");
      }
      handleNetMessages(netSession, game, &hud, &audio);

      // Client MOVE_REQ
      if (game->hasPendingMoveReq()) {
        std::string uci = game->consumePendingMoveReq();
        int ply = game->movePly + 1;
        netSession.send(NetProtocol::makeMoveReq(uci, ply));
        std::cout << "[VCC-NET] MOVE_REQ " << uci << std::endl;
        hud.setEvent("Sent move " + uci);
      }
      // Host local MOVE broadcast
      if (game->hasLocalMoveBroadcast()) {
        std::string uci = game->consumeLocalMoveBroadcast();
        netSession.send(NetProtocol::makeMove(
          uci, game->movePly, game->localSideSign() > 0 ? "w" : "b"));
        std::cout << "[VCC-NET] MOVE " << uci << " ply=" << game->movePly
                  << std::endl;
      }

      if (netSession.inGame()) {
        netWasInGame = true;
        // Status overlay via event occasionally
        static double lastStatus = 0;
        double t = mainClock.getElapsedTime();
        if (t - lastStatus > 2.0) {
          lastStatus = t;
          // lightweight: status line includes net
        }
      } else if (netWasInGame && !netSession.active()) {
        hud.setEvent(netSession.statusLine());
        game->clearNetworkRole();
        netWasInGame = false;
      } else if (!netSession.active() && netSession.phase() == NetSession::PHASE_CLOSED) {
        hud.setEvent(netSession.statusLine());
      }
    }

    // Settings → Quit confirmed: leave match → Main Menu; from Main Menu → exit app
    if (settings.consumeQuitRequest()) {
      if (gInGame) {
        returnToMainMenu();
      } else {
        gRunning = false;
        glfwSetWindowShouldClose(window, 1);
        break;
      }
    }

    // Main menu actions
    if (mainMenu.isVisible() && !settings.isOpen()) {
      MainMenu::Action act = mainMenu.consumeAction();
      if (act == MainMenu::ACTION_QUIT) {
        gRunning = false;
        glfwSetWindowShouldClose(window, 1);
        break;
      }
      if (act == MainMenu::ACTION_OPEN_SETTINGS) {
        // Keep Main Menu visible so hero art + effect particles stay behind
        // Settings (same idea as Multiplayer sub-page). In-game Esc still
        // overlays Settings on the live board only.
        gSettingsFromMenu = true;
        settings.openMenu();
        audio.playSfx("sfx_select");
        hud.setEvent("Settings");
        // Stay on menu music (already playing)
      }
      if (act == MainMenu::ACTION_SINGLE_PLAYER) {
        if (netSession.active()) netSession.close("single_player");
        game->clearNetworkRole();
        game->resetBoard();
        game->setAiEnabled(true);
        victory.hide();
        mainMenu.hide();
        gInGame = true;
        audio.playSfx("sfx_theme");
        playStageMusic(true); // menu theme → stage theme
        hud.setEvent(std::string("Single Player — Set: ") + pieceSets.current().name);
        std::cout << "[VCC] Single Player\n";
      }
      if (act == MainMenu::ACTION_REFRESH_ROOMS) {
        std::vector<LobbyRoom> rooms;
        std::string err;
        if (!mainMenu.lobby().listRooms(rooms, &err)) {
          mainMenu.setStatus("Lobby unreachable: " + err);
          audio.playSfx("sfx_illegal");
        } else {
          mainMenu.setRooms(rooms);
          mainMenu.setStatus(std::string("Rooms: ") + std::to_string(rooms.size()) +
                             " @ " + mainMenu.lobby().host());
          audio.playSfx("sfx_select");
        }
      }
      if (act == MainMenu::ACTION_HOST_ONLINE) {
        std::string err;
        LobbySession sess = mainMenu.lobby().createRoom(
          mainMenu.hostRoomName(), mainMenu.hostPassword(), &err);
        if (!sess.ok || sess.token.empty()) {
          mainMenu.setStatus("Host failed: " + (err.empty() ? sess.error : err));
          audio.playSfx("sfx_illegal");
        } else {
          if (netSession.active()) netSession.close("rehost");
          std::string rerr;
          if (!netSession.startRelay(mainMenu.lobby().host(), sess.relayPort,
                                     sess.token, "host", &rerr)) {
            mainMenu.setStatus("Relay failed: " + rerr);
            audio.playSfx("sfx_illegal");
          } else {
            game->setNetworkRole(ChessGame::NET_HOST, 1);
            game->resetBoard();
            victory.hide();
            mainMenu.hide();
            gInGame = true;
            audio.playSfx("sfx_theme");
            playStageMusic(true);
            hud.setEvent("Online room hosted — waiting for guest");
            std::cout << "[VCC-NET] online host room=" << sess.roomId << "\n";
          }
        }
      }
      if (act == MainMenu::ACTION_JOIN_ONLINE) {
        const LobbyRoom* room = mainMenu.selectedRoom();
        if (!room) {
          mainMenu.setStatus("Select a room from the list first");
          audio.playSfx("sfx_illegal");
        } else if (room->full) {
          mainMenu.setStatus("Room is full");
          audio.playSfx("sfx_illegal");
        } else {
          std::string err;
          LobbySession sess = mainMenu.lobby().joinRoom(
            room->id, mainMenu.joinPassword(), &err);
          if (!sess.ok || sess.token.empty()) {
            mainMenu.setStatus("Join failed: " + (err.empty() ? sess.error : err));
            audio.playSfx("sfx_illegal");
          } else {
            if (netSession.active()) netSession.close("rejoin");
            std::string rerr;
            if (!netSession.startRelay(mainMenu.lobby().host(), sess.relayPort,
                                       sess.token, "guest", &rerr)) {
              mainMenu.setStatus("Relay failed: " + rerr);
              audio.playSfx("sfx_illegal");
            } else {
              victory.hide();
              mainMenu.hide();
              gInGame = true;
              audio.playSfx("sfx_theme");
              playStageMusic(true);
              hud.setEvent("Joining " + room->name + "…");
              std::cout << "[VCC-NET] online join room=" << room->id << "\n";
            }
          }
        }
      }
    }

    // Settings just closed (Esc / Return / S) — music + menu restore
    {
      static bool settingsWasOpen = false;
      bool nowOpen = settings.isOpen();
      if (settingsWasOpen && !nowOpen)
        onSettingsClosed();
      settingsWasOpen = nowOpen;
    }

    if (resizing) {
      colorPicking->resizeBuffers(width, height);
      camera->updatePerspective((double)width / height);
      resizing = false;
    }
    // Apply each mouse delta once, then clear — reusing last dY every frame
    // made pitch (tilt) jump while yaw happened to feel smoother.
    if (gInGame && cameraMoving && (dX != 0 || dY != 0)) {
      camera->move((GLfloat)dX, (GLfloat)dY, (GLfloat)width / (GLfloat)height);
      dX = 0;
      dY = 0;
    } else {
      dX = 0;
      dY = 0;
    }
    if (selecting) {
      selecting = false;
      // No board selection during menu, victory or settings
      if (gInGame && !mainMenu.isVisible() && !victory.isOpen() && !settings.isOpen()) {
        // Suppress suggested-move pulse in pick buffer when hints are off
        Vector2i sugS = game->suggestedUserMoveStartPosition;
        Vector2i sugE = game->suggestedUserMoveEndPosition;
        if (!settings.suggestedMovesEnabled()) {
          game->suggestedUserMoveStartPosition = {-1, -1};
          game->suggestedUserMoveEndPosition = {-1, -1};
        }
        auto pos = colorPicking->getClickedPiecePosition(
          {mousePosition.x, height - mousePosition.y},
          game, &pieces, &programs, camera, elapsedTime);
        game->suggestedUserMoveStartPosition = sugS;
        game->suggestedUserMoveEndPosition = sugE;
        game->setNewSelectedPiecePosition(pos);
        if (game->getState() == USER_TURN || game->getState() == BLACK_TURN) {
          audio.playSfx("sfx_select");
        }
      }
    }

    int prevState = game->getState();
    if (gInGame && !mainMenu.isVisible()) {
      try {
        game->perform();
      } catch (const std::exception& e) {
        std::cerr << "[VCC] perform: " << e.what() << std::endl;
        hud.setEvent(e.what());
        audio.playSfx("sfx_illegal");
        // recover: don't kill session on bad AI move
        if (game->getState() == AI_TURN) game->setAiEnabled(false);
      } catch (...) {
        std::cerr << "[VCC] perform: unknown exception\n";
        hud.setEvent("Internal error (move)");
      }
    }
    int st = game->getState();
    {
      std::string status = gInGame ? stateName(st) : "Main Menu";
      if (gInGame && (netSession.active() || netSession.phase() == NetSession::PHASE_CLOSED)) {
        status += " | ";
        status += netSession.statusLine();
      }
      hud.setStatus(status);
    }
    // Network moves start in tryApplyUciMove (before perform), so rely on
    // ChessGame's one-shot flag rather than only state-diff across perform().
    bool startedMoving = game->consumeAnimationJustStarted();
    if (!startedMoving) {
      startedMoving =
        (prevState == USER_TURN && st == USER_MOVING) ||
        (prevState == BLACK_TURN && st == BLACK_MOVING) ||
        (prevState == AI_TURN && st == AI_MOVING);
    }
    // Acknowledge: piece ordered to move (occupied tile or empty)
    if (startedMoving) {
      audio.playSfx("sfx_acknowledge");
      if (st == USER_MOVING || st == BLACK_MOVING)
        std::cout << "[VCC] Move " << game->getLastUserMove() << "\n";
      if (st == AI_MOVING)
        hud.setEvent("AI moved");
    }
    // Capture-only action camera (Settings → Video) — all sides, including net
    if (!settings.actionCameraEnabled() && actionCam.isActive()) {
      actionCam.cancel(camera);
    }
    actionCam.setEnabled(settings.actionCameraEnabled());
    if (startedMoving && game->movingIsCapture && settings.actionCameraEnabled()) {
      actionCam.beginCapture(camera, game);
      hud.setEvent("ACTION CAM");
    }

    // Victory celebration: explode remaining enemy non-king pieces once
    if (game->victoryFxPending) {
      try {
        game->victoryFxPending = false;
        const Theme& thV = themes.current();
        float fxV = themes.fxScale();
        float force = settings.explosionForceScale();
        for (int x = 0; x < 8; x++) {
          for (int y = 0; y < 8; y++) {
            int p = game->board[x][y];
            if (p == EMPTY) continue;
            if (std::abs(p) == KING) continue;
            bool isWhite = p > 0;
            // Explode loser's army (keep king)
            if (game->whiteWon && isWhite) continue;
            if (!game->whiteWon && !isWhite) continue;
            Vector2i pos = {x, y};
            physicsWorld->collapsePiece(p, pos, force);
            game->board[x][y] = EMPTY;
            Vector3f origin = {
              x * 4.f - 14.f, y * 4.f - 14.f, 1.5f
            };
            sparks.burst(origin, thV.sparkColor, (int)(60 * fxV), 1.2f * fxV);
            smokeGenerator->generate(origin, (int)(15 * fxV), thV.sparkColor, 1.0f);
          }
        }
        captureShake.trigger(2.2f, 1.25f);
        audio.playSfx("sfx_victory");
        audio.playSfx("sfx_piece_destroyed");
        victory.show(game->endReason, game->whiteWon);
        hud.setEvent(game->endReason == END_FORFEIT ? "FORFEIT!" : "CHECKMATE!");
        std::cout << "[VCC] Victory screen — "
                  << (game->endReason == END_FORFEIT ? "FORFEIT" : "CHECKMATE")
                  << " (" << (game->whiteWon ? "White" : "Black") << ")\n"
                  << std::flush;
      } catch (const std::exception& e) {
        std::cerr << "[VCC] victory FX: " << e.what() << "\n";
        game->victoryFxPending = false;
        victory.show(game->endReason, game->whiteWon);
      }
    }

    // Replay from victory overlay
    if (victory.consumeReplay()) {
      if (actionCam.isActive()) actionCam.cancel(camera);
      game->resetBoard();
      physicsWorld->resyncBoard(game);
      victory.hide();
      hud.setEvent("New game");
      audio.playSfx("sfx_theme");
      std::cout << "[VCC] Replay — board reset\n";
    }

    physicsWorld->simulate();
    sparks.update();

    Event gameEvent;
    while (EventStack::pollEvent(&gameEvent)) {
      const Theme& th = themes.current();
      float fx = themes.fxScale();

      if (gameEvent.type == Event::FragmentDisappearsEvent) {
        smokeGenerator->generate(
          gameEvent.fragment.position,
          (int)round(gameEvent.fragment.volume * fx),
          gameEvent.fragment.piece > 0 ? th.smokeUser : th.smokeAI,
          0.7f);
      }
      if (gameEvent.type == Event::PieceTakenEvent) {
        // Chunk explosion + radial blast force (static board pieces stay put)
        physicsWorld->collapsePiece(
          gameEvent.piece.piece, gameEvent.piece.position,
          settings.explosionForceScale());
        actionCam.onPieceTaken();
        Vector3f origin = {
          gameEvent.piece.position.x * 4.f - 14.f,
          gameEvent.piece.position.y * 4.f - 14.f,
          1.5f
        };
        // Flash of sparks + smoke at the blast
        sparks.burst(origin, th.sparkColor, (int)(140 * fx), 2.0f * fx);
        smokeGenerator->generate(origin, (int)(40 * fx), th.sparkColor, 1.6f);
        audio.playSfx("sfx_piece_destroyed");
        // Extreme screen shake ~2s
        captureShake.trigger(2.0f, 1.15f);
        hud.setEvent("EXPLOSION!");
        std::cout << "[VCC] CAPTURE EXPLOSION at "
                  << gameEvent.piece.position.x << ","
                  << gameEvent.piece.position.y << "\n";
      }
      if (gameEvent.type == Event::PieceMovingEvent) {
        physicsWorld->updatePiecePosition(
          gameEvent.movingPiece.startPosition,
          gameEvent.movingPiece.currentPosition);
        // Slight height so trails sit around the piece body, not under the board
        Vector3f p = {
          gameEvent.movingPiece.currentPosition.x * 4.f - 14.f,
          gameEvent.movingPiece.currentPosition.y * 4.f - 14.f,
          0.9f
        };
        // Per-theme move trail: smoke sprites vs neon energy cloud (space)
        if (th.moveParticleStyle == MOVE_PARTICLE_NEON) {
          // Side colours from theme (starship: white=red, black=purple)
          Vector3f neonCol = (game->movingPiece > 0)
            ? th.neonTrailUser : th.neonTrailAI;
          // Cloud each frame of the move lerp (billboard discs)
          int n = (int)std::max(2.f, 3.f * fx);
          sparks.emitNeonCloud(p, neonCol, n);
        } else {
          smokeGenerator->generate(p, (int)(1 * fx), th.moveTrail, 0.25f);
          if (themes.fx() == FX_HIGH) {
            sparks.burst(p, th.moveTrail, 2, 0.3f);
          }
        }
      }
      if (gameEvent.type == Event::PieceStopsEvent) {
        physicsWorld->movePiece(
          gameEvent.movingPiece.startPosition,
          gameEvent.movingPiece.endPosition);
      }
    }

    // Light from theme
    const Theme& th = themes.current();
    light.direction = th.lightDir;
    Vector3f lightPosition = {
      -20.f * light.direction.x,
      -20.f * light.direction.y,
      -20.f * light.direction.z
    };
    light.viewMatrix = getLookAtMatrix(lightPosition, {0, 0, 0}, {0, 0, 1});

    float outlineF = settings.outlineFactor();
    const Theme& thRender = th;

    if (!mainMenu.isVisible()) {
      camera->update();
      // Action cam follows after user orbit so RMB/zoom stay relative to the piece
      actionCam.update(camera, game, physicsWorld, dt);

      // Gate engine-suggested move pulse (Gameplay setting; live mid-game toggle)
      Vector2i sugSaveS = game->suggestedUserMoveStartPosition;
      Vector2i sugSaveE = game->suggestedUserMoveEndPosition;
      if (!settings.suggestedMovesEnabled()) {
        game->suggestedUserMoveStartPosition = {-1, -1};
        game->suggestedUserMoveEndPosition = {-1, -1};
      }

      shadowMapping->renderShadowMap(game, &pieces, &programs, &light, elapsedTime);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      // Shadow / previous frame smoke may leave program, multitex, instancing dirty.
      ncaResetPipelineState();

      glClearColor(thRender.clearBottom.x, thRender.clearBottom.y, thRender.clearBottom.z, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, width, height);

      // Starship: full sky-sphere universe + star. Other themes: simple gradient.
      if (themes.activeId() == THEME_STARSHIP && starfield.ready()) {
        starfield.drawWorld(camera);
      } else {
        drawSkyGradient(thRender);
      }

      Vector3f shake = (themes.activeId() == THEME_STARSHIP && starfield.ready())
        ? starfield.boardShake() : Vector3f(0,0,0);
      // Capture explosion shake (all themes) — stacked on ambient starship rattle
      Vector3f capShake = captureShake.offset();
      shake.x += capShake.x;
      shake.y += capShake.y;
      shake.z += capShake.z;
      float hover = (themes.activeId() == THEME_STARSHIP && starfield.ready())
        ? starfield.boardHoverZ() : 0.f;

      // Pieces need a clean program/texture unit state after sky GLSL env pass.
      ncaResetPipelineState();
      glEnable(GL_CULL_FACE);
      glEnable(GL_DEPTH_TEST);

      std::string activeSet = pieceSets.current().id;

      // Neon move particles act as dynamic point lights on board/pieces
      Vector3f neonPos[SparkSystem::MAX_POINT_LIGHTS];
      Vector3f neonCol[SparkSystem::MAX_POINT_LIGHTS];
      float neonInt[SparkSystem::MAX_POINT_LIGHTS];
      int neonCount = 0;
      if (thRender.moveParticleStyle == MOVE_PARTICLE_NEON) {
        sparks.gatherNeonLights(
          neonPos, neonCol, neonInt, SparkSystem::MAX_POINT_LIGHTS, &neonCount);
      }

      celShadingRender(
        game, physicsWorld, &pieces, &programs, shadowMapping, camera, &light,
        elapsedTime, thRender, shake, hover, outlineF, &transforms, &activeSet,
        neonPos, neonCol, neonInt, neonCount,
        settings.outlineWhiteR(), settings.outlineWhiteG(), settings.outlineWhiteB(),
        settings.outlineBlackR(), settings.outlineBlackG(), settings.outlineBlackB());

      // Restore suggestion coords so next AI move still has data when re-enabled
      game->suggestedUserMoveStartPosition = sugSaveS;
      game->suggestedUserMoveEndPosition = sugSaveE;

      ncaResetPipelineState();
      glEnable(GL_DEPTH_TEST);

      // Smoke only for non-neon themes / fragment debris; sparks always (incl. neon)
      smokeGenerator->draw(camera);
      sparks.draw(camera);

      ncaResetPipelineState();

      // Screen flare after 3D (epilepsy-safe swell), then HUD on top
      if (themes.activeId() == THEME_STARSHIP && starfield.ready()) {
        starfield.drawScreenFlare(width, height);
      }

      // HUD only during play (menu has its own chrome)
      if (gInGame) {
        hud.draw(width, height, themes, game->isAiEnabled(), audio.musicEnabled(),
                 &pieceSets);
      }
    }

    mainMenu.draw(width, height);
    settings.draw(width, height);
    if (pieceEditor.isOpen())
      pieceEditor.draw(width, height, &programs, thRender, outlineF);
    if (gInGame) victory.draw(width, height);

    glfwSwapBuffers(window);
  }

  gMainMenu = nullptr;
  gNet = nullptr;
  glfwTerminate();
  PieceSetManager::freePieces(&pieces);
  PieceSetManager::freeFragments(&fragmentMeshes);
  deletePrograms(&programs);
  delete colorPicking;
  delete shadowMapping;
  delete smokeGenerator;
  delete game;
  delete physicsWorld;
  delete camera;
  audio.shutdown();
  return 0;
}

void celShadingRender(
    ChessGame* game,
    PhysicsWorld* physicsWorld,
    std::map<int, Mesh*>* pieces,
    std::map<int, ShaderProgram*>* programs,
    ShadowMapping* shadowMapping,
    Camera* camera,
    DirectionalLight* light,
    float elapsedTime,
    const Theme& th,
    Vector3f boardShake,
    float boardHover,
    float outlineFactor,
    PieceTransformStore* transforms,
    const std::string* activeSetId,
    const Vector3f* neonLightPos,
    const Vector3f* neonLightColor,
    const float* neonLightIntensity,
    int neonLightCount,
    float outlineWhiteR,
    float outlineWhiteG,
    float outlineWhiteB,
    float outlineBlackR,
    float outlineBlackG,
    float outlineBlackB) {
  std::vector<GLfloat> movementMatrix;
  Vector3f translation;
  Vector3f rotation = {0, 0, 1};
  // User asked to double piece size (2×). Board cells stay full cell size.
  const float PIECE_SCALE = 2.0f;
  auto boardT = [&](float x, float y, float z) -> Vector3f {
    return {
      x + boardShake.x,
      y + boardShake.y,
      z + boardShake.z + boardHover
    };
  };
  auto pieceMatrix = [&](int pieceSigned, float bx, float by, float bz) {
    PieceTransform t;
    if (transforms && activeSetId) {
      t = transforms->get(*activeSetId,
        PieceTransformStore::pieceKeyFromType(std::abs(pieceSigned)));
    }
    return PieceTransformStore::buildPieceMatrix(
      pieceSigned, bx, by, bz, PIECE_SCALE, t);
  };

  ShaderProgram* blackBorderProgram = programs->at(BLACK_BORDER);
  ShaderProgram* celShadingProgram = programs->at(CEL_SHADING);

  glUseProgram(blackBorderProgram->id);
  glCullFace(GL_FRONT);
  blackBorderProgram->setViewMatrix(&camera->viewMatrix);
  blackBorderProgram->setProjectionMatrix(&camera->projectionMatrix);
  // Normal-based outline thickness (Video settings). 0 disables silhouette shell.
  blackBorderProgram->setFloat("outlineFactor", outlineFactor);

  auto setOutlineSide = [&](int pieceSigned) {
    // White / user (positive) vs black / AI (negative). Board uses black.
    if (pieceSigned > 0)
      blackBorderProgram->setVector4f(
        "outlineColor", outlineWhiteR, outlineWhiteG, outlineWhiteB, 1.f);
    else if (pieceSigned < 0)
      blackBorderProgram->setVector4f(
        "outlineColor", outlineBlackR, outlineBlackG, outlineBlackB, 1.f);
    else
      blackBorderProgram->setVector4f("outlineColor", 0.f, 0.f, 0.f, 1.f);
  };

  for (unsigned int i = 0; i < physicsWorld->fragmentPool.size(); i++) {
    Fragment* fragment = physicsWorld->fragmentPool.at(i).second;
    movementMatrix = fragment->getMoveMatrix();
    blackBorderProgram->setMoveMatrix(&movementMatrix);
    std::vector<GLfloat> normalMatrix = inverse(&movementMatrix);
    normalMatrix = transpose(&normalMatrix);
    blackBorderProgram->setNormalMatrix(&normalMatrix);
    setOutlineSide(physicsWorld->fragmentPool.at(i).first);
    fragment->mesh->draw();
  }

  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      int piece = game->board[x][y];
      float zLift = 0.f;
      // Suggested-move tiles and/or checked king (same bob; see isHighlightPulseSquare)
      if (game->isHighlightPulseSquare(x, y)) {
        zLift = 0.5f + 0.5f * sin(2 * elapsedTime - M_PI / 2.0);
      }
      translation = boardT((float)(x * 4.0 - 14.0), (float)(y * 4.0 - 14.0), zLift);

      // Board cell: identity rotation (Z-up flat), no piece scale
      movementMatrix = getIdentityMatrix();
      movementMatrix = translate(&movementMatrix, translation);
      blackBorderProgram->setMoveMatrix(&movementMatrix);
      setOutlineSide(0); // board: black outline
      pieces->at(BOARDCELL)->draw();

      if (piece != EMPTY) {
        int pk = std::abs(piece);
        if (pieces->count(pk)) {
          movementMatrix = pieceMatrix(
            piece, translation.x, translation.y, translation.z);
          blackBorderProgram->setMoveMatrix(&movementMatrix);
          setOutlineSide(piece);
          pieces->at(pk)->draw();
        }
      }
    }
  }

  if (game->movingPiece != EMPTY) {
    int mk = std::abs(game->movingPiece);
    if (pieces->count(mk)) {
      translation = boardT(
        (float)(game->movingPiecePosition.x * 4.0 - 14.0),
        (float)(game->movingPiecePosition.y * 4.0 - 14.0), 0.0
      );
      movementMatrix = pieceMatrix(
        game->movingPiece, translation.x, translation.y, translation.z);
      blackBorderProgram->setMoveMatrix(&movementMatrix);
      setOutlineSide(game->movingPiece);
      pieces->at(mk)->draw();
    }
  }

  glUseProgram(celShadingProgram->id);
  glCullFace(GL_BACK);
  celShadingProgram->setViewMatrix(&camera->viewMatrix);
  celShadingProgram->setProjectionMatrix(&camera->projectionMatrix);
  celShadingProgram->setMatrix4fv("LMatrix", &light->viewMatrix);
  celShadingProgram->setMatrix4fv("PLMatrix", &light->projectionMatrix);
  celShadingProgram->bindTexture(0, GL_TEXTURE0, "shadowMap", shadowMapping->getShadowMap());
  celShadingProgram->setInt("shadowMapResolution", shadowMapping->resolution);
  celShadingProgram->setVector3f(
    "lightDirection", light->direction.x, light->direction.y, light->direction.z);
  celShadingProgram->setFloat("emissiveBoost", th.emissiveBoost);
  celShadingProgram->setFloat("time", elapsedTime);
  celShadingProgram->setInt("useDiffuseMap", 0);

  // Dynamic neon particle lights (space theme). Zero when unused.
  int nLights = neonLightCount;
  if (nLights < 0) nLights = 0;
  if (nLights > SparkSystem::MAX_POINT_LIGHTS) nLights = SparkSystem::MAX_POINT_LIGHTS;
  if (!neonLightPos || !neonLightColor || !neonLightIntensity) nLights = 0;
  celShadingProgram->setInt("numPointLights", (GLfloat)nLights);
  for (int i = 0; i < SparkSystem::MAX_POINT_LIGHTS; i++) {
    char posName[32], colName[32], intName[32];
    std::snprintf(posName, sizeof(posName), "pointLightPos[%d]", i);
    std::snprintf(colName, sizeof(colName), "pointLightColor[%d]", i);
    std::snprintf(intName, sizeof(intName), "pointLightIntensity[%d]", i);
    if (i < nLights) {
      celShadingProgram->setVector3f(
        posName, neonLightPos[i].x, neonLightPos[i].y, neonLightPos[i].z);
      celShadingProgram->setVector3f(
        colName, neonLightColor[i].x, neonLightColor[i].y, neonLightColor[i].z);
      celShadingProgram->setFloat(intName, neonLightIntensity[i]);
    } else {
      celShadingProgram->setVector3f(posName, 0.f, 0.f, 0.f);
      celShadingProgram->setVector3f(colName, 0.f, 0.f, 0.f);
      celShadingProgram->setFloat(intName, 0.f);
    }
  }

  auto setC = [&](const Vector4f& c) {
    celShadingProgram->setVector4f("color", c.x, c.y, c.z, c.w);
  };

  // Cel fill: solid theme colour, or piece albedo texture (keeps black outline pass)
  auto drawCelMesh = [&](Mesh* mesh) {
    if (mesh->hasDiffuseTexture()) {
      celShadingProgram->bindTexture(
        1, GL_TEXTURE1, "diffuseMap", mesh->diffuseTextureId);
      celShadingProgram->setInt("useDiffuseMap", 1);
    } else {
      celShadingProgram->setInt("useDiffuseMap", 0);
    }
    mesh->draw();
  };

  for (unsigned int i = 0; i < physicsWorld->fragmentPool.size(); i++) {
    Fragment* fragment = physicsWorld->fragmentPool.at(i).second;
    movementMatrix = fragment->getMoveMatrix();
    celShadingProgram->setMoveMatrix(&movementMatrix);
    std::vector<GLfloat> normalMatrix = inverse(&movementMatrix);
    normalMatrix = transpose(&normalMatrix);
    celShadingProgram->setNormalMatrix(&normalMatrix);
    // Exploded chunks: textured when the set provides albedo (recognisable model)
    if (physicsWorld->fragmentPool.at(i).first > 0)
      setC(th.pieceUser);
    else
      setC(th.pieceAI);
    drawCelMesh(fragment->mesh);
  }

  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      int piece = game->board[x][y];

      float zLift2 = 0.f;
      if (game->isHighlightPulseSquare(x, y)) {
        zLift2 = 0.5f + 0.5f * sin(2 * elapsedTime - M_PI / 2.0);
      }
      translation = boardT((float)(x * 4.0 - 14.0), (float)(y * 4.0 - 14.0), zLift2);

      (x + y) % 2 == 0 ? setC(th.boardDark) : setC(th.boardLight);
      if ((game->selectedPiecePosition.x == x && game->selectedPiecePosition.y == y) ||
          game->allowedNextPositions[x][y]) {
        setC(th.boardHighlight);
      }

      movementMatrix = getIdentityMatrix();
      movementMatrix = translate(&movementMatrix, translation);
      celShadingProgram->setMoveMatrix(&movementMatrix);
      std::vector<GLfloat> normalMatrix = inverse(&movementMatrix);
      normalMatrix = transpose(&normalMatrix);
      celShadingProgram->setNormalMatrix(&normalMatrix);
      drawCelMesh(pieces->at(BOARDCELL));

      if (piece != EMPTY) {
        movementMatrix = pieceMatrix(
          piece, translation.x, translation.y, translation.z);
        celShadingProgram->setMoveMatrix(&movementMatrix);
        normalMatrix = inverse(&movementMatrix);
        normalMatrix = transpose(&normalMatrix);
        celShadingProgram->setNormalMatrix(&normalMatrix);
        int pk = std::abs(piece);
        if (pieces->count(pk)) {
          piece > 0 ? setC(th.pieceUser) : setC(th.pieceAI);
          drawCelMesh(pieces->at(pk));
        }
      }
    }
  }

  if (game->movingPiece != EMPTY) {
    int mk = std::abs(game->movingPiece);
    if (pieces->count(mk)) {
      translation = boardT(
        (float)(game->movingPiecePosition.x * 4.0 - 14.0),
        (float)(game->movingPiecePosition.y * 4.0 - 14.0), 0.0
      );
      movementMatrix = pieceMatrix(
        game->movingPiece, translation.x, translation.y, translation.z);
      celShadingProgram->setMoveMatrix(&movementMatrix);
      std::vector<GLfloat> normalMatrix = inverse(&movementMatrix);
      normalMatrix = transpose(&normalMatrix);
      celShadingProgram->setNormalMatrix(&normalMatrix);
      game->movingPiece > 0 ? setC(th.pieceUser) : setC(th.pieceAI);
      drawCelMesh(pieces->at(mk));
    }
  }
}
