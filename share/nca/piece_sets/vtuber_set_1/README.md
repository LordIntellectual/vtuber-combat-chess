# vTuber Set 1

Drop piece meshes into **this folder**. The game loads them at startup / when you press **P** to cycle sets.

## Required files (OBJ, Z-up)

| File | Piece |
|------|--------|
| `king.obj` | King |
| `queen.obj` | Queen |
| `bishop.obj` | Bishop |
| `knight.obj` | Knight |
| `rook.obj` | Rook / castle |
| `pawn.obj` | Pawn |
| `boardCell.obj` | Board square (optional — falls back to starship/classic) |

## Optional

| File | Purpose |
|------|---------|
| `pawn.png` (same basename as `.obj`) | Albedo texture (needs UVs: `vt` + `f v/vt/vn`) |
| `king.png`, `queen.png`, … | Same for other pieces |
| `*_fragments.cobj` | Capture break-apart meshes (falls back if missing) |
| `source/` | Keep original GLB/FBX/etc. here for archival |

## Format notes

- **Z-up** (height along +Z), base near z = 0  
- Typical height ~1.2–2.0 (game draws pieces at 2×)  
- Object line: `o piece_name`  
- Normals required: `vn` and faces `f v//vn` or `f v/vt/vn`  
- From GLB:  
  `tools/glb_to_nca_obj.py input.glb pawn.obj --name pawn --height 1.35`

## After adding files

Copy into the install tree (or re-run install):

```bash
cp -a share/nca/piece_sets/vtuber_set_1/. local/share/nca/piece_sets/vtuber_set_1/
```

Then restart the game and press **P** until **vTuber Set 1** is active.
