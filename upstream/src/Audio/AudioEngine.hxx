#ifndef NCA_AUDIO_ENGINE_HXX_
#define NCA_AUDIO_ENGINE_HXX_

#include <string>
#include <map>
#include <vector>

// miniaudio is implemented in AudioEngine.cxx
struct ma_engine;
struct ma_sound;

class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();

  bool init(const std::string& audioDir);
  void shutdown();
  /* Call once per frame to free finished one-shot SFX. */
  void update();

  void playSfx(const std::string& name); // e.g. "sfx_move"
  void playMusic(const std::string& filename);
  void stopMusic();
  void setMusicEnabled(bool on);
  bool musicEnabled() const { return musicOn; }
  void toggleMusic();

  void setMasterVolume(float v);
  void setMusicVolume(float v);
  void setSfxVolume(float v);

  float masterVolume() const { return masterVol; }
  float musicVolume() const { return musicVol; }
  float sfxVolume() const { return sfxVol; }

private:
  bool ready;
  bool musicOn;
  bool musicFullTrack; // louder default curve for full-length MP3s
  float masterVol;
  float musicVol;
  float sfxVol;
  std::string dir;
  void* engine; // ma_engine*
  void* musicSound; // ma_sound*
  std::map<std::string, std::string> sfxPaths;
  std::vector<ma_sound*> activeSfx;

  void applyVolumes();
  void pruneSfx();
};

#endif
