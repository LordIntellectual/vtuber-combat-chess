# vTuber Combat Chess — Design Document

**Product name:** vTuber Combat Chess  
**Author:** Lord Intellectual  
**Former working titles:** Nightfire Chess Arena (NCA)  
**Codename (legacy paths/tags):** NCA / VCC  
**Base:** ToonChess (GPL) — heavily extended for stream entertainment  
**Date started:** 2026-07-25  
**Rename to current title:** 2026-07-27  
**Owner goal:** Chess that is *spectacle* for stream / vTuber combat presentation, not a tournament GUI.

**Development note:** Implementation has been assisted by AI coding agents (including Grok / xAI and others) under Lord Intellectual’s direction. That assistance is intentional and disclosed.

## Vision

Every capture is a show. Every board is a stage. Themes transform lighting, colors, particles, ambient sound, and destruction vibes. The streamer has an always-on control HUD to flip stages, piece sets, AI, and FX intensity without leaving the game.

## Pillars

1. **Spectacle first** — particles, glow, screen-readable motion, destruction physics (Bullet fragments), action camera.  
2. **Theme stages** — at least three fully distinct visual/audio identities.  
3. **Streamer control** — keyboard + on-screen panel: theme, AI, FX intensity, music, settings.  
4. **Stream-ready** — windowed 16:9, readable events on stdout (`[VCC]` tags), rebuild docs.  
5. **vTuber combat identity** — character piece sets, neon/space VFX, celebration captures.

## Themes (v1)

| ID | Name | Visual | Audio mood |
|----|------|--------|------------|
| 0 | **Cyber Neon Lounge** | Magenta/cyan glow, dark floor, neon grid, hot pink AI pieces | Synth pulse, digital blips |
| 1 | **Bioluminescent Jungle** | Deep greens/teals, soft bloom, vine-colored board | Soft drones, organic clicks |
| 2 | **Starship Over a Star** | Orange corona sky, metal greys, ember captures | Engine rumble, metal scrapes |

## Systems

### ThemeManager
Central palette: board dark/light, user/AI piece colors, highlight, clear color, light direction, particle tints, border intensity, emissive strength.

### FX
- Existing smoke + fragment collapse (amplified colors per theme)
- Spark/ember particle bursts on capture; neon trails on space theme
- Move trail particles (themed)
- Screen shake + action camera on dramatic captures
- Explosion force scale (Gameplay settings)

### Audio (miniaudio)
- Music loops (generated or short WAV/MP3)
- SFX: select, acknowledge, move, capture, piece destroyed, illegal, theme, victory
- Master / music / sfx gains

### UI / Controls
| Key | Action |
|-----|--------|
| `1`/`2`/`3` | Theme 0–2 |
| `T` | Cycle theme |
| `A` | Toggle AI (Stockfish) |
| `M` | Toggle music |
| `F` | Cycle FX intensity Low/Med/High |
| `R` | New game / reset board |
| `S` | Settings menu |
| `P` | Cycle piece set |
| `H` | Toggle HUD help |
| `Esc` | Quit |
| RMB drag | Orbit camera |
| LMB | Select / move |

On-screen HUD: product title, theme name, AI on/off, FX level, last event, controls strip.  
Pre-alpha splash before music/control. Victory screen on checkmate/forfeit.

### AI
- AI ON: Stockfish after user move (legal-move / check filtering)
- AI OFF: human plays black
- Endgame: checkmate and forfeit (lone king) handled locally with victory UI

## Non-goals (v1)
- Full online multiplayer
- Photo-realistic PBR meshes per theme (palette + shaders + set meshes instead)
- Full FIDE completeness (castling/en passant still limited where base engine was)

## Build
Same stack as ToonChess: CMake, GLFW (fetched), OpenGL, Bullet 2.87 (local), PNG, Stockfish runtime.  
Additional: miniaudio (header), stb_easy_font (header).  
Executable: `VTuberCombatChess`.

## Licensing
Base ToonChess license (see upstream COPYING). Product extensions and assets under Lord Intellectual’s project; disclose GPL when redistributing.
