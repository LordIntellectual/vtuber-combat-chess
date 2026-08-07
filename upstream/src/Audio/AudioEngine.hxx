#ifndef NCA_AUDIO_ENGINE_HXX_
#define NCA_AUDIO_ENGINE_HXX_

#include <string>
#include <map>
#include <vector>

// miniaudio is implemented in AudioEngine.cxx
struct ma_engine;
struct ma_sound;

/** Main Menu theme (under share/nca/audio/). */
static const char* const kMenuMusicFile =
  "music/vtuber_combat_chess_main_theme_v0_0_1.mp3";

class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();

  bool init(const std::string& audioDir);
  void shutdown();
  /** Call once per frame: free finished SFX and advance music crossfades. */
  void update(float dt = 0.016f);

  void playSfx(const std::string& name); // e.g. "sfx_move"
  /** Hard-cut to track (or start if silent). Prefer crossfadeMusic for scene changes. */
  void playMusic(const std::string& filename);
  /** Smooth blend from current music into filename over durationSec. */
  void crossfadeMusic(const std::string& filename, float durationSec = 1.6f);
  void stopMusic();
  void setMusicEnabled(bool on);
  bool musicEnabled() const { return musicOn; }
  void toggleMusic();

  /** Path of the track we are playing / fading toward (relative to audio dir). */
  const std::string& currentMusicFile() const { return currentFile; }
  bool isPlayingMusic() const { return musicSound != nullptr || musicFadeOut != nullptr; }

  void setMasterVolume(float v);
  void setMusicVolume(float v);
  void setSfxVolume(float v);

  float masterVolume() const { return masterVol; }
  float musicVolume() const { return musicVol; }
  float sfxVolume() const { return sfxVol; }

  /** Smoothed 0..1 levels from the currently playing music (for UI pulse / FX). */
  float musicLevel() const { return levelSmooth; }
  float musicBassLevel() const { return bassSmooth; }

private:
  bool ready;
  bool musicOn;
  bool musicFullTrack; // louder default curve for full-length MP3s
  float masterVol;
  float musicVol;
  float sfxVol;
  std::string dir;
  void* engine; // ma_engine*
  void* musicSound; // ma_sound* — primary / fade-in
  void* musicFadeOut; // ma_sound* — fading out during crossfade
  float musicGain;     // 0..1 scale on primary
  float fadeOutGain;   // 0..1 scale on fade-out slot
  float fadeT;
  float fadeDur;
  bool fading;
  std::string currentFile;
  std::string analysisAbsPath;
  void* analysisDecoder; // ma_decoder* — PCM peek for reactive UI
  float levelSmooth;
  float bassSmooth;
  std::map<std::string, std::string> sfxPaths;
  std::vector<ma_sound*> activeSfx;

  void applyVolumes();
  void pruneSfx();
  void destroySound(void*& slot);
  bool loadMusicSound(const std::string& filename, void*& outSlot, std::string& resolvedPath);
  float musicTargetVolume() const;
  void openMusicAnalysis(const std::string& absPath);
  void closeMusicAnalysis();
  void updateMusicAnalysis();
};

#endif
