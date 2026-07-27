# Piece sets

Each subdirectory is one chess piece set for **vTuber Combat Chess** (Lord Intellectual).

## Layout

```
piece_sets/
  <set_id>/
    set.json          # { "id", "name", "description" }
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

## Switching in game

- `P` — cycle piece set
- `H` — hide/show HUD (clean stream view)
