# Starship Stage — Asset & FX Pass

**Focus:** Board as a crumbling metal sci-fi ship flying too close to a star.

## Generated art (AI image tools)

| File | Purpose |
|------|---------|
| `share/nca/textures/space_scape.png` | Vivid deep-space / nebula backdrop (UV-scrolled for motion) |
| `share/nca/textures/star_surface.png` | Blazing star sphere under the board |
| `share/nca/textures/hull_deck.png` | Weathered ship-deck plating reference (board color pairing) |
| `share/nca/textures/ship_pieces_concept.png` | Concept sheet for 6 ship-piece silhouettes |

Also mirrored under `docs/gallery/` for review.

## Procedural 3D fleet (hard-surface OBJs)

Built with a Python hard-surface generator (boxes + tapered hulls), not Blender:

| Piece | Sci-fi identity | Files |
|-------|-----------------|-------|
| Pawn | Small fighter drone | `assets/starship/pawn.obj` + fragments |
| Rook | Block freighter / turret barge | `rook.obj` |
| Knight | Angular interceptor | `knight.obj` |
| Bishop | Lance / spear frigate | `bishop.obj` |
| Queen | Wide mothership | `queen.obj` |
| King | Command cruiser + bridge | `king.obj` |
| Board cell | Rimmed metal deck plate | `boardCell.obj` |

Fragments (`*_fragments.cobj`) are spatial clusters for Bullet destruction.

Regenerate:

```bash
# (script embedded in tools history — re-run from PORT_LOG or docs)
python3 tools/gen_starship_meshes.py   # if extracted to tools/
```

## Runtime FX (engine)

When theme = **Starship Over a Star** (default):

1. **Scrolling space backdrop** — textured full-screen quad with UV motion  
2. **Star under the board** — textured sphere, pulse radius on corona  
3. **Corona beams** — additive triangles from star toward deck, ~every 4–9s  
4. **Board rattle** — impulse shake on corona + continuous micro-turbulence  
5. **Flight hover** — vertical bob on entire board  
6. **Motion streaks** — recycled line particles flying past (speed sensation)  
7. **Capture sparks** (existing) amplified on High FX  

## How to view

```bash
cd Nightfire_Games/In_Progress/NightfireChessArena   # historical project folder
./run_nca.sh   # launches VTuberCombatChess (vTuber Combat Chess by Lord Intellectual)
# Starts on Starship theme (key 3). Keys 1/2 switch away; 3 returns.
```

Watch for console: `[Starfield] CORONA BURST — board rattled`

## 2026-07-25 fix pass — orientation, lighting, sky sphere

### 1) Board/piece 90° tilt (FIXED)
Original ToonChess assets are **Z-up** (board flat on XY, height +Z).  
First starship generator used **Y-up**; after the game’s yaw rotation the tiles stood on edge → black silhouettes.

**Fix:** regenerate all `assets/starship/*.obj` and fragments with **Z = up**, board cell thin in Z (matches original `boardCell` bounds style).

### 2) Black unlit look (FIXED with orientation + shader)
Edge-on tiles made cel fill invisible (only white outlines). Also hardened lighting:

- `celShadingVS.glsl`: normal transform uses `w=0`; `abs(dot)` for readable hard-surface ships  
- Starship palette brightened for stream contrast  

### 3) Tiling space backdrop (FIXED)
Replaced scrolling full-screen **tiling quad** with an **inward-facing sky sphere** (radius ~90) around the play space, single texture mapped on the inside, slow continuous rotation (`time * 3.5°/s` around world Z).

### Honest limits

- Ship meshes are **greybox hard-surface**, not hand-sculpted studio models  
- Sky is equirect-style spherical UV on one image (poles can pin slightly)  
- Hull texture not yet UV-bound to board cells (vertex colors / theme palette still drive fill)  

