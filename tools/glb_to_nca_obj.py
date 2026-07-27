#!/usr/bin/env python3
"""Convert a glTF/GLB mesh to Z-up textured OBJ for vTuber Combat Chess.

Exports:
  - .obj with v / vt / vn and f v/vt/vn
  - .png basecolor (from embedded image)
  - .mtl referencing the png

Normals are recomputed for correct cel-shading + back-face culling.

Usage:
  glb_to_nca_obj.py input.glb output.obj --name pawn --height 1.35
"""
from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


def load_glb(path: Path):
    data = path.read_bytes()
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF":
        raise RuntimeError(f"Not a GLB: {path}")
    off = 12
    js = binchunk = None
    while off < length:
        clen, ctype = struct.unpack_from("<I4s", data, off)
        off += 8
        cdata = data[off : off + clen]
        off += clen
        if ctype == b"JSON":
            js = json.loads(cdata.decode("utf-8").rstrip(" \0"))
        elif ctype.startswith(b"BIN"):
            binchunk = cdata
    if js is None or binchunk is None:
        raise RuntimeError("GLB missing JSON or BIN chunk")
    return js, binchunk


def accessor_raw(js, binchunk, acc_idx):
    acc = js["accessors"][acc_idx]
    bv = js["bufferViews"][acc["bufferView"]]
    off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    ctype = acc["componentType"]
    count = acc["count"]
    typ = acc["type"]
    ncomp = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[typ]
    stride = bv.get("byteStride", None)
    if ctype == 5126:
        esz = 4 * ncomp
        st = stride or esz
        return [
            struct.unpack_from("<" + "f" * ncomp, binchunk, off + i * st)
            for i in range(count)
        ]
    if ctype == 5123:
        st = stride or 2
        return [struct.unpack_from("<H", binchunk, off + i * st)[0] for i in range(count)]
    if ctype == 5125:
        st = stride or 4
        return [struct.unpack_from("<I", binchunk, off + i * st)[0] for i in range(count)]
    raise RuntimeError(f"unsupported componentType {ctype}")


def extract_png(js, binchunk, out_path: Path) -> bool:
    imgs = js.get("images") or []
    if not imgs:
        return False
    im = imgs[0]
    if "bufferView" not in im:
        return False
    bv = js["bufferViews"][im["bufferView"]]
    off = bv.get("byteOffset", 0)
    length = bv["byteLength"]
    out_path.write_bytes(binchunk[off : off + length])
    print(f"  texture {out_path} ({length} bytes)")
    return True


def convert(glb_path: Path, obj_path: Path, object_name: str, target_height: float, max_half_xy: float = 0.85):
    js, binchunk = load_glb(glb_path)
    tex_path = obj_path.with_suffix(".png")
    extract_png(js, binchunk, tex_path)

    prim = js["meshes"][0]["primitives"][0]
    pos = accessor_raw(js, binchunk, prim["attributes"]["POSITION"])
    idx = accessor_raw(js, binchunk, prim["indices"])
    if "TEXCOORD_0" in prim["attributes"]:
        uvs = accessor_raw(js, binchunk, prim["attributes"]["TEXCOORD_0"])
        tex = [(u[0], 1.0 - u[1]) for u in uvs]  # flip V for OpenGL
    else:
        tex = [(0.0, 0.0)] * len(pos)

    # Y-up → Z-up
    verts = [(p[0], -p[2], p[1]) for p in pos]
    zs = [v[2] for v in verts]
    zmin, zmax = min(zs), max(zs)
    scale = target_height / (zmax - zmin) if zmax > zmin else 1.0
    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    cx = 0.5 * (min(xs) + max(xs))
    cy = 0.5 * (min(ys) + max(ys))
    verts = [((x - cx) * scale, (y - cy) * scale, (z - zmin) * scale) for x, y, z in verts]
    max_xy = max(max(abs(v[0]), abs(v[1])) for v in verts) or 1.0
    if max_xy > max_half_xy:
        s2 = max_half_xy / max_xy
        verts = [(x * s2, y * s2, z * s2) for x, y, z in verts]
        zmin2 = min(v[2] for v in verts)
        verts = [(x, y, z - zmin2) for x, y, z in verts]

    tris = [(idx[i], idx[i + 1], idx[i + 2]) for i in range(0, len(idx), 3)]
    acc = [[0.0, 0.0, 0.0] for _ in verts]
    for a, b, c in tris:
        ax, ay, az = verts[a]
        bx, by, bz = verts[b]
        cx_, cy_, cz_ = verts[c]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx_ - ax, cy_ - ay, cz_ - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        for i in (a, b, c):
            acc[i][0] += nx
            acc[i][1] += ny
            acc[i][2] += nz
    norms = []
    for nx, ny, nz in acc:
        L = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
        norms.append((nx / L, ny / L, nz / L))

    cx = sum(v[0] for v in verts) / len(verts)
    cy = sum(v[1] for v in verts) / len(verts)
    cz = sum(v[2] for v in verts) / len(verts)
    out = inn = 0
    step = max(1, len(verts) // 200)
    for i in range(0, len(verts), step):
        v, n = verts[i], norms[i]
        d = (v[0] - cx) * n[0] + (v[1] - cy) * n[1] + (v[2] - cz) * n[2]
        if d >= 0:
            out += 1
        else:
            inn += 1
    if inn > out:
        norms = [(-n[0], -n[1], -n[2]) for n in norms]
        tris = [(a, c, b) for a, b, c in tris]

    lines = [
        f"# From {glb_path.name} via glb_to_nca_obj.py",
        f"o {object_name}",
        f"mtllib {object_name}.mtl",
        f"usemtl {object_name}",
    ]
    for x, y, z in verts:
        lines.append(f"v {x:.6f} {y:.6f} {z:.6f}")
    for u, v in tex:
        lines.append(f"vt {u:.6f} {v:.6f}")
    for nx, ny, nz in norms:
        lines.append(f"vn {nx:.6f} {ny:.6f} {nz:.6f}")
    for a, b, c in tris:
        lines.append(f"f {a+1}/{a+1}/{a+1} {b+1}/{b+1}/{b+1} {c+1}/{c+1}/{c+1}")

    obj_path.parent.mkdir(parents=True, exist_ok=True)
    obj_path.write_text("\n".join(lines) + "\n")
    obj_path.with_suffix(".mtl").write_text(
        f"newmtl {object_name}\nKa 1 1 1\nKd 1 1 1\nmap_Kd {tex_path.name}\n"
    )
    zs = [v[2] for v in verts]
    print(f"Wrote {obj_path}  tris={len(tris)}  h={max(zs)-min(zs):.3f}  tex={tex_path.name}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input_glb", type=Path)
    ap.add_argument("output_obj", type=Path)
    ap.add_argument("--name", default="piece")
    ap.add_argument("--height", type=float, default=1.35)
    ap.add_argument("--max-half-xy", type=float, default=0.85)
    args = ap.parse_args()
    convert(args.input_glb, args.output_obj, args.name, args.height, args.max_half_xy)


if __name__ == "__main__":
    main()
