#define MINIAUDIO_IMPLEMENTATION
#include "../../third_party/miniaudio.h"

#include "AudioEngine.hxx"
#include <iostream>
#include <algorithm>
#include <cctype>

AudioEngine::AudioEngine()
  : ready(false), musicOn(true), musicFullTrack(false),
    masterVol(0.9f), musicVol(0.45f), sfxVol(0.7f),
    engine(nullptr), musicSound(nullptr) {}

AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::init(const std::string& audioDir) {
  dir = audioDir;
  for (size_t i = 0; i < dir.size(); ++i)
    if (dir[i] == '\\') dir[i] = '/';
  if (!dir.empty() && dir.back() != '/') dir.push_back('/');

  ma_engine* eng = new ma_engine();
  ma_engine_config cfg = ma_engine_config_init();
  if (ma_engine_init(&cfg, eng) != MA_SUCCESS) {
    delete eng;
    std::cerr << "[Audio] ma_engine_init failed — running silent\n";
    return false;
  }
  engine = eng;
  ready = true;

  // sfx_select        = Select.wav          (piece/tile/menu selection)
  // sfx_move / sfx_acknowledge = Acknowledge.wav (order piece to move)
  // sfx_piece_destroyed / sfx_capture / sfx_explosion = Piece_destroyed.wav
  const char* names[] = {
    "sfx_select", "sfx_move", "sfx_acknowledge",
    "sfx_piece_destroyed", "sfx_capture", "sfx_explosion",
    "sfx_check", "sfx_illegal", "sfx_theme", "sfx_victory"
  };
  for (const char* n : names) {
    sfxPaths[n] = dir + n + ".wav";
  }

  applyVolumes();
  std::cout << "[Audio] Ready (dir=" << dir << ")\n";
  return true;
}

void AudioEngine::shutdown() {
  pruneSfx();
  for (ma_sound* s : activeSfx) {
    if (s) {
      ma_sound_uninit(s);
      delete s;
    }
  }
  activeSfx.clear();

  if (musicSound) {
    ma_sound_uninit((ma_sound*)musicSound);
    delete (ma_sound*)musicSound;
    musicSound = nullptr;
  }
  if (engine) {
    ma_engine_uninit((ma_engine*)engine);
    delete (ma_engine*)engine;
    engine = nullptr;
  }
  ready = false;
}

void AudioEngine::applyVolumes() {
  if (!ready || !engine) return;
  // Engine = master; music/sfx sounds are relative (0..1) multipliers.
  ma_engine_set_volume((ma_engine*)engine, std::max(0.f, std::min(1.f, masterVol)));
  if (musicSound) {
    float mv = std::max(0.f, std::min(1.f, musicVol));
    if (musicFullTrack) mv = std::min(1.f, mv * 1.25f);
    ma_sound_set_volume((ma_sound*)musicSound, mv);
  }
}

void AudioEngine::pruneSfx() {
  if (activeSfx.empty()) return;
  std::vector<ma_sound*> keep;
  keep.reserve(activeSfx.size());
  for (ma_sound* s : activeSfx) {
    if (!s) continue;
    if (ma_sound_is_playing(s) == MA_TRUE) {
      keep.push_back(s);
    } else {
      ma_sound_uninit(s);
      delete s;
    }
  }
  activeSfx.swap(keep);
}

void AudioEngine::update() {
  if (!ready) return;
  pruneSfx();
}

void AudioEngine::playSfx(const std::string& name) {
  if (!ready || !engine) return;
  auto it = sfxPaths.find(name);
  if (it == sfxPaths.end()) return;

  pruneSfx();
  ma_sound* snd = new ma_sound();
  // Decode fully for short WAV one-shots; fire-and-forget via activeSfx list
  if (ma_sound_init_from_file((ma_engine*)engine, it->second.c_str(),
        MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_DECODE, nullptr, nullptr, snd) != MA_SUCCESS) {
    delete snd;
    return;
  }
  ma_sound_set_volume(snd, std::max(0.f, std::min(1.f, sfxVol)));
  ma_sound_start(snd);
  activeSfx.push_back(snd);
}

void AudioEngine::playMusic(const std::string& filename) {
  if (!ready || !engine) return;
  stopMusic();
  if (!musicOn) return;

  std::string path = dir + filename;
  const ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_ASYNC;
  ma_sound* snd = new ma_sound();
  if (ma_sound_init_from_file((ma_engine*)engine, path.c_str(),
        flags, nullptr, nullptr, snd) != MA_SUCCESS) {
    delete snd;
    std::cerr << "[Audio] Failed to load music: " << path << "\n";
    if (filename.rfind("music/", 0) == 0) {
      std::string alt = dir + filename.substr(6);
      snd = new ma_sound();
      if (ma_sound_init_from_file((ma_engine*)engine, alt.c_str(),
            flags, nullptr, nullptr, snd) == MA_SUCCESS) {
        path = alt;
      } else {
        delete snd;
        std::cerr << "[Audio] Fallback also failed: " << alt << "\n";
        return;
      }
    } else {
      return;
    }
  }
  ma_sound_set_looping(snd, MA_TRUE);
  musicFullTrack =
    filename.find("music/") != std::string::npos ||
    (filename.size() >= 4 &&
     (filename.compare(filename.size() - 4, 4, ".mp3") == 0 ||
      filename.compare(filename.size() - 4, 4, ".MP3") == 0));
  musicSound = snd;
  applyVolumes();
  ma_sound_start(snd);
  std::cout << "[Audio] Music: " << path << " (looping)\n";
}

void AudioEngine::stopMusic() {
  if (musicSound) {
    ma_sound_stop((ma_sound*)musicSound);
    ma_sound_uninit((ma_sound*)musicSound);
    delete (ma_sound*)musicSound;
    musicSound = nullptr;
  }
  musicFullTrack = false;
}

void AudioEngine::setMusicEnabled(bool on) {
  musicOn = on;
  if (!on) stopMusic();
}

void AudioEngine::toggleMusic() {
  setMusicEnabled(!musicOn);
}

void AudioEngine::setMasterVolume(float v) {
  masterVol = std::max(0.f, std::min(1.f, v));
  applyVolumes();
}

void AudioEngine::setMusicVolume(float v) {
  musicVol = std::max(0.f, std::min(1.f, v));
  applyVolumes();
}

void AudioEngine::setSfxVolume(float v) {
  sfxVol = std::max(0.f, std::min(1.f, v));
  // Active one-shots keep their start volume; new plays use sfxVol.
}
