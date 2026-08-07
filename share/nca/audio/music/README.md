# Music

Paths are relative to `share/nca/audio/`.

| File | Use |
|------|-----|
| `vtuber_combat_chess_main_theme_v0_0_1.mp3` | **Main Menu** theme (playback; extracted from the `.mp4` source) |
| `vtuber_combat_chess_Main_Theme_v0_0_1.mp4` | Source/master of the main theme (audio-only AAC in MP4) |
| `nova_chase_orbit.mp3` | Starship / space **stage** — **Nova Chase Orbit** |
| (legacy WAV loops) | `../music_neon.wav`, `../music_jungle.wav`, `../music_starship.wav` |

## Behaviour (engine)

1. Launch → SFX only, no music → pre-alpha splash  
2. Accept splash → main theme starts immediately  
3. Enter match (Single Player / online) → crossfade **menu → stage**  
4. Esc / Settings in-game → crossfade **stage → menu**  
5. Resume (close settings while still in match) → crossfade **menu → stage**  
6. Quit to Main Menu → crossfade to menu theme  

Stage tracks come from `Theme.cxx` (`musicFile` per theme).
