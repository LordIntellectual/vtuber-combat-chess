#define MINIAUDIO_IMPLEMENTATION
#include "../../third_party/miniaudio.h"

#include "AudioEngine.hxx"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>

AudioEngine::AudioEngine()
  : ready(false), musicOn(true), musicFullTrack(false),
    masterVol(0.9f), musicVol(0.45f), sfxVol(0.7f),
    engine(nullptr), musicSound(nullptr), musicFadeOut(nullptr),
    musicGain(1.f), fadeOutGain(0.f), fadeT(0.f), fadeDur(1.6f),
    fading(false), analysisDecoder(nullptr),
    levelSmooth(0.f), bassSmooth(0.f) {}

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

void AudioEngine::destroySound(void*& slot) {
  if (!slot) return;
  ma_sound_stop((ma_sound*)slot);
  ma_sound_uninit((ma_sound*)slot);
  delete (ma_sound*)slot;
  slot = nullptr;
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

  closeMusicAnalysis();
  destroySound(musicFadeOut);
  destroySound(musicSound);
  fading = false;
  currentFile.clear();

  if (engine) {
    ma_engine_uninit((ma_engine*)engine);
    delete (ma_engine*)engine;
    engine = nullptr;
  }
  ready = false;
}

void AudioEngine::closeMusicAnalysis() {
  if (analysisDecoder) {
    ma_decoder_uninit((ma_decoder*)analysisDecoder);
    delete (ma_decoder*)analysisDecoder;
    analysisDecoder = nullptr;
  }
  analysisAbsPath.clear();
  levelSmooth = 0.f;
  bassSmooth = 0.f;
}

void AudioEngine::openMusicAnalysis(const std::string& absPath) {
  if (absPath.empty()) {
    closeMusicAnalysis();
    return;
  }
  if (analysisDecoder && analysisAbsPath == absPath) return;
  closeMusicAnalysis();

  ma_decoder_config cfg = ma_decoder_config_init_default();
  cfg.format = ma_format_f32;
  cfg.channels = 2;
  ma_decoder* dec = new ma_decoder();
  if (ma_decoder_init_file(absPath.c_str(), &cfg, dec) != MA_SUCCESS) {
    delete dec;
    std::cerr << "[Audio] Analysis decoder failed: " << absPath << "\n";
    return;
  }
  analysisDecoder = dec;
  analysisAbsPath = absPath;
}

void AudioEngine::updateMusicAnalysis() {
  if (!musicSound || !analysisDecoder || !musicOn) {
    // Decay toward silence when no music
    levelSmooth *= 0.90f;
    bassSmooth *= 0.90f;
    return;
  }

  ma_uint64 cursor = 0;
  if (ma_sound_get_cursor_in_pcm_frames((ma_sound*)musicSound, &cursor) != MA_SUCCESS)
    return;

  ma_decoder* dec = (ma_decoder*)analysisDecoder;
  // Peek a short window of PCM around the playhead
  const ma_uint64 win = 1024;
  ma_uint64 seek = (cursor > win / 2) ? (cursor - win / 2) : 0;
  if (ma_decoder_seek_to_pcm_frame(dec, seek) != MA_SUCCESS) return;

  float buf[1024 * 2];
  ma_uint64 framesRead = 0;
  if (ma_decoder_read_pcm_frames(dec, buf, win, &framesRead) != MA_SUCCESS || framesRead < 32)
    return;

  // Mono RMS + crude bass (exponential low-pass then RMS of abs)
  double sumSq = 0.0;
  double sumBassSq = 0.0;
  float lp = 0.f;
  const float a = 0.12f; // low-pass coefficient (~bass emphasis)
  for (ma_uint64 i = 0; i < framesRead; ++i) {
    float L = buf[i * 2 + 0];
    float R = buf[i * 2 + 1];
    float m = 0.5f * (L + R);
    sumSq += (double)m * (double)m;
    lp = lp + a * (m - lp);
    sumBassSq += (double)lp * (double)lp;
  }
  float rms = (float)std::sqrt(sumSq / (double)framesRead);
  float bass = (float)std::sqrt(sumBassSq / (double)framesRead);

  // Normalise — full tracks often sit low; boost then clamp
  rms = std::min(1.f, rms * 4.5f);
  bass = std::min(1.f, bass * 6.5f);

  // Attack / release envelope (punchy bass for techno)
  auto env = [](float cur, float target, float attack, float release) {
    float k = (target > cur) ? attack : release;
    return cur + (target - cur) * k;
  };
  levelSmooth = env(levelSmooth, rms, 0.45f, 0.12f);
  bassSmooth = env(bassSmooth, bass, 0.55f, 0.10f);
}

float AudioEngine::musicTargetVolume() const {
  float mv = std::max(0.f, std::min(1.f, musicVol));
  if (musicFullTrack) mv = std::min(1.f, mv * 1.25f);
  return mv;
}

void AudioEngine::applyVolumes() {
  if (!ready || !engine) return;
  ma_engine_set_volume((ma_engine*)engine, std::max(0.f, std::min(1.f, masterVol)));
  const float base = musicTargetVolume();
  if (musicSound)
    ma_sound_set_volume((ma_sound*)musicSound, base * musicGain);
  if (musicFadeOut)
    ma_sound_set_volume((ma_sound*)musicFadeOut, base * fadeOutGain);
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

void AudioEngine::update(float dt) {
  if (!ready) return;
  pruneSfx();

  if (fading) {
    if (dt < 0.f) dt = 0.f;
    if (dt > 0.1f) dt = 0.1f;
    fadeT += dt;
    float t = (fadeDur > 1e-4f) ? (fadeT / fadeDur) : 1.f;
    if (t >= 1.f) {
      t = 1.f;
      musicGain = 1.f;
      fadeOutGain = 0.f;
      destroySound(musicFadeOut);
      fading = false;
    } else {
      // Smoothstep for a less linear crossfade
      float s = t * t * (3.f - 2.f * t);
      musicGain = s;
      fadeOutGain = 1.f - s;
    }
    applyVolumes();
  }

  updateMusicAnalysis();
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

bool AudioEngine::loadMusicSound(const std::string& filename, void*& outSlot,
                                 std::string& resolvedPath) {
  outSlot = nullptr;
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
        return false;
      }
    } else {
      return false;
    }
  }
  ma_sound_set_looping(snd, MA_TRUE);
  outSlot = snd;
  resolvedPath = path;
  return true;
}

void AudioEngine::playMusic(const std::string& filename) {
  if (!ready || !engine) return;
  stopMusic();
  if (!musicOn) return;
  if (filename.empty()) return;

  std::string resolved;
  if (!loadMusicSound(filename, musicSound, resolved)) return;

  musicFullTrack =
    filename.find("music/") != std::string::npos ||
    (filename.size() >= 4 &&
     (filename.compare(filename.size() - 4, 4, ".mp3") == 0 ||
      filename.compare(filename.size() - 4, 4, ".MP3") == 0));
  currentFile = filename;
  musicGain = 1.f;
  fadeOutGain = 0.f;
  fading = false;
  applyVolumes();
  ma_sound_start((ma_sound*)musicSound);
  openMusicAnalysis(resolved);
  std::cout << "[Audio] Music: " << resolved << " (looping)\n";
}

void AudioEngine::crossfadeMusic(const std::string& filename, float durationSec) {
  if (!ready || !engine) return;
  if (!musicOn) {
    // Music muted — remember selection but stay silent
    currentFile = filename;
    return;
  }
  if (filename.empty()) return;

  // Already on this track (and not mid-fade-out to something else): keep it
  if (filename == currentFile && musicSound && !fading) return;

  // If already fading toward the same file, leave it
  if (filename == currentFile && fading && musicSound) return;

  // Drop any previous fade-out
  destroySound(musicFadeOut);

  // Move current primary into fade-out slot
  musicFadeOut = musicSound;
  musicSound = nullptr;
  fadeOutGain = musicGain;
  if (fadeOutGain < 0.01f && musicFadeOut) {
    // practically silent — just kill
    destroySound(musicFadeOut);
    fadeOutGain = 0.f;
  }

  std::string resolved;
  if (!loadMusicSound(filename, musicSound, resolved)) {
    // Restore if we still have old track
    if (musicFadeOut) {
      musicSound = musicFadeOut;
      musicFadeOut = nullptr;
      musicGain = fadeOutGain > 0.f ? fadeOutGain : 1.f;
      fadeOutGain = 0.f;
      fading = false;
      applyVolumes();
    }
    return;
  }

  musicFullTrack =
    filename.find("music/") != std::string::npos ||
    (filename.size() >= 4 &&
     (filename.compare(filename.size() - 4, 4, ".mp3") == 0 ||
      filename.compare(filename.size() - 4, 4, ".MP3") == 0));
  currentFile = filename;
  musicGain = 0.f;
  fadeT = 0.f;
  fadeDur = (durationSec > 0.05f) ? durationSec : 0.05f;
  fading = true;
  applyVolumes();
  ma_sound_start((ma_sound*)musicSound);
  openMusicAnalysis(resolved);
  std::cout << "[Audio] Crossfade → " << resolved
            << " (" << fadeDur << "s)\n";
}

void AudioEngine::stopMusic() {
  closeMusicAnalysis();
  destroySound(musicFadeOut);
  destroySound(musicSound);
  musicFullTrack = false;
  musicGain = 1.f;
  fadeOutGain = 0.f;
  fading = false;
  currentFile.clear();
}

void AudioEngine::setMusicEnabled(bool on) {
  musicOn = on;
  if (!on) {
    stopMusic();
  }
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
