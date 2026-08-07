# Piece sets

Each subdirectory is one chess piece set for **vTuber Combat Chess** (Lord Intellectual).

## Layout

```
piece_sets/
  <set_id>/
    set.json          # { "id", "name", "description" }
    transforms.json   # per-piece scale / offset / rotation (see below)
    king.obj
    queen.obj
    bishop.obj
    knight.obj
    rook.obj
    pawn.obj
    boardCell.obj
    *_fragments.cobj  # optional; falls back to starship/classic
    source/           # optional original assets (glb, etc.)
```

## Requirements

- OBJ, Z-up (board plane is XY, height is +Z)
- Object name line: `o piece_name`
- Vertex normals required (`vn` + faces as `f v//vn` or `f v/vt/vn`)
- Base of piece near z=0; typical height ~1.2–2.0 (game draws at 2× scale)
- **Optional texture:** place `pawn.png` next to `pawn.obj` (same basename).
  OBJ must include `vt` UVs and faces as `f v/vt/vn`. Cel-shaded outlines stay;
  the fill samples the albedo map with a light team-colour wash.

## Transforms (`transforms.json`) — scale / pose, not mesh edits

OBJ files from different sources often arrive at different sizes and orientations.
**Do not bake ad-hoc fixes into the mesh for balance.** Use the Piece Editor
(in Settings) to set per-piece **scale, position offset, and rotation**. Those
values live in `transforms.json` next to the set.

### Who owns what

| Role | Location | Purpose |
|------|----------|---------|
| **Master defaults** (Lord’s calibrated game) | Git-tracked `share/nca/piece_sets/<set_id>/transforms.json` | Ships with the project / releases so fresh installs match the stream setup |
| **This install** | The set path the game actually loads (e.g. `local/share/nca/piece_sets/...` when running via `run.sh`) | Runtime load + Piece Editor **Save** writes here |

- **Lord’s local install is the master source of truth for calibration.** After
  tuning pieces in-game and saving, copy that set’s `transforms.json` into
  `share/nca/piece_sets/<set_id>/` and **commit** so the next clean clone /
  Windows package keeps those defaults.
- **Players** may freely change scales/poses on their own machine (Piece Editor
  → Save). That only updates their install’s file; it does not change the
  project defaults until someone commits an updated `share/` copy.
- `local/` is gitignored — never rely on it alone for “default” scale, or
  rebuilds will drop back to 1.0 / missing file.

### Format (example)

```json
{
  "pawn": {"px": 0, "py": 0, "pz": 0, "rx": 0, "ry": 0, "rz": 0, "scale": 0.75}
}
```

Keys: `king`, `queen`, `bishop`, `knight`, `rook`, `pawn`.  
Missing keys → identity transform (scale 1). Some sets may also have a
small built-in code fallback if the file is absent entirely.

## Switching in game

- `P` — cycle piece set
- `H` — hide/show HUD (clean stream view)
- Settings → Piece Editor — adjust transform, **Save** to this install’s
  `transforms.json`
