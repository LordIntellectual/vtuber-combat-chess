#!/usr/bin/env python3
"""Shatter an NCA piece .obj into spatial *_fragments.cobj chunks.

Usage:
  python3 tools/shatter_piece_to_fragments.py share/nca/piece_sets/vtuber_set_1
  python3 tools/shatter_piece_to_fragments.py path/to/set --pieces queen rook
"""
from __future__ import print_function
import argparse
import math
import os
import sys
from collections import defaultdict

PIECES = ["king", "queen", "bishop", "knight", "rook", "pawn"]


def parse_obj(path):
    verts, norms, uvs = [], [], []
    faces = []
    with open(path, "r", errors="ignore") as f:
        for line in f:
            if not line.strip() or line[0] == "#":
                continue
            p = line.split()
            if not p:
                continue
            if p[0] == "v" and len(p) >= 4:
                verts.append((float(p[1]), float(p[2]), float(p[3])))
            elif p[0] == "vt" and len(p) >= 3:
                uvs.append((float(p[1]), float(p[2])))
            elif p[0] == "vn" and len(p) >= 4:
                norms.append((float(p[1]), float(p[2]), float(p[3])))
            elif p[0] == "f" and len(p) >= 4:
                corners = []
                for tok in p[1:]:
                    parts = tok.split("/")
                    vi = int(parts[0]) - 1
                    ti = int(parts[1]) - 1 if len(parts) > 1 and parts[1] else -1
                    ni = int(parts[2]) - 1 if len(parts) > 2 and parts[2] else -1
                    corners.append((vi, ti, ni))
                for i in range(1, len(corners) - 1):
                    faces.append([corners[0], corners[i], corners[i + 1]])
    return verts, norms, uvs, faces


def face_centroid(verts, face):
    cx = cy = cz = 0.0
    for vi, _, _ in face:
        x, y, z = verts[vi]
        cx += x
        cy += y
        cz += z
    n = float(len(face))
    return cx / n, cy / n, cz / n


def face_area(verts, face):
    a = verts[face[0][0]]
    b = verts[face[1][0]]
    c = verts[face[2][0]]
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    cx = uy * vz - uz * vy
    cy = uz * vx - ux * vz
    cz = ux * vy - uy * vx
    return 0.5 * math.sqrt(cx * cx + cy * cy + cz * cz)


def shatter(path, out_path, target_chunks=30):
    verts, norms, uvs, faces = parse_obj(path)
    if not faces:
        print("  no faces in", path)
        return 0
    cents = [face_centroid(verts, f) for f in faces]
    xs = [c[0] for c in cents]
    ys = [c[1] for c in cents]
    zs = [c[2] for c in cents]
    minx, maxx = min(xs), max(xs)
    miny, maxy = min(ys), max(ys)
    minz, maxz = min(zs), max(zs)
    dx = max(maxx - minx, 1e-4)
    dy = max(maxy - miny, 1e-4)
    dz = max(maxz - minz, 1e-4)
    vol = dx * dy * dz
    cell = (vol / float(target_chunks)) ** (1.0 / 3.0)
    nx = max(2, int(round(dx / cell)))
    ny = max(2, int(round(dy / cell)))
    nz = max(2, int(round(dz / cell)))
    while nx * ny * nz > target_chunks * 1.8:
        if max(nx, ny, nz) == nx and nx > 2:
            nx -= 1
        elif max(nx, ny, nz) == ny and ny > 2:
            ny -= 1
        elif nz > 2:
            nz -= 1
        else:
            break

    bins = defaultdict(list)
    for i, c in enumerate(cents):
        ix = min(nx - 1, max(0, int((c[0] - minx) / dx * nx)))
        iy = min(ny - 1, max(0, int((c[1] - miny) / dy * ny)))
        iz = min(nz - 1, max(0, int((c[2] - minz) / dz * nz)))
        bins[(ix, iy, iz)].append(i)

    min_faces = max(8, len(faces) // (target_chunks * 4))
    items = list(bins.items())
    bins = {k: v for k, v in items if len(v) >= min_faces}
    leftovers = [(k, v) for k, v in items if len(v) < min_faces]
    bin_cent = {}
    for k, idxs in bins.items():
        sx = sy = sz = 0.0
        for i in idxs:
            sx += cents[i][0]
            sy += cents[i][1]
            sz += cents[i][2]
        n = float(len(idxs))
        bin_cent[k] = (sx / n, sy / n, sz / n)
    for k, idxs in leftovers:
        if not bin_cent:
            bins[k] = idxs
            continue
        cx = sum(cents[i][0] for i in idxs) / len(idxs)
        cy = sum(cents[i][1] for i in idxs) / len(idxs)
        cz = sum(cents[i][2] for i in idxs) / len(idxs)
        best = min(
            bin_cent.keys(),
            key=lambda kk: (bin_cent[kk][0] - cx) ** 2
            + (bin_cent[kk][1] - cy) ** 2
            + (bin_cent[kk][2] - cz) ** 2,
        )
        bins[best].extend(idxs)

    chunks = list(bins.values())
    lines = [
        "# NCA shatter fragments from " + os.path.basename(path),
        "# grid %dx%dx%d → %d chunks, %d faces" % (nx, ny, nz, len(chunks), len(faces)),
    ]
    for ci, idxs in enumerate(chunks):
        comx = comy = comz = 0.0
        area_sum = 0.0
        for fi in idxs:
            a = face_area(verts, faces[fi]) + 1e-9
            c = cents[fi]
            comx += c[0] * a
            comy += c[1] * a
            comz += c[2] * a
            area_sum += a
        comx /= area_sum
        comy /= area_sum
        comz /= area_sum
        mass = max(0.2, area_sum * 0.35)

        lines.append("o frag_%d" % ci)
        lines.append("mass %.4f" % mass)
        lines.append("origin %.6f %.6f %.6f" % (comx, comy, comz))

        vmap = {}
        local_v, local_vt, local_vn, local_f = [], [], [], []
        has_uv = bool(uvs)
        has_n = bool(norms)
        for fi in idxs:
            face_local = []
            for vi, ti, ni in faces[fi]:
                key = (vi, ti, ni)
                if key not in vmap:
                    vmap[key] = len(local_v)
                    x, y, z = verts[vi]
                    local_v.append((x - comx, y - comy, z - comz))
                    if has_uv and 0 <= ti < len(uvs):
                        local_vt.append(uvs[ti])
                    else:
                        local_vt.append((0.0, 0.0))
                    if has_n and 0 <= ni < len(norms):
                        local_vn.append(norms[ni])
                    else:
                        a = verts[faces[fi][0][0]]
                        b = verts[faces[fi][1][0]]
                        c = verts[faces[fi][2][0]]
                        ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
                        vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
                        nx_, ny_, nz_ = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
                        L = math.sqrt(nx_ * nx_ + ny_ * ny_ + nz_ * nz_) + 1e-9
                        local_vn.append((nx_ / L, ny_ / L, nz_ / L))
                face_local.append(vmap[key] + 1)
            local_f.append(face_local)

        for x, y, z in local_v:
            lines.append("v %.6f %.6f %.6f" % (x, y, z))
        for u, v in local_vt:
            lines.append("vt %.6f %.6f" % (u, v))
        for x, y, z in local_vn:
            lines.append("vn %.6f %.6f %.6f" % (x, y, z))
        for a, b, c in local_f:
            lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (a, a, a, b, b, b, c, c, c))

    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(
        "  %s → %s: %d chunks (grid %dx%dx%d)"
        % (os.path.basename(path), os.path.basename(out_path), len(chunks), nx, ny, nz)
    )
    return len(chunks)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("set_dir", help="Piece set directory containing king.obj etc.")
    ap.add_argument("--pieces", nargs="*", default=PIECES)
    ap.add_argument("--chunks", type=int, default=30, help="Target chunk count")
    args = ap.parse_args()
    set_dir = args.set_dir
    if not os.path.isdir(set_dir):
        print("Not a directory:", set_dir, file=sys.stderr)
        return 1
    for p in args.pieces:
        src = os.path.join(set_dir, "%s.obj" % p)
        dst = os.path.join(set_dir, "%s_fragments.cobj" % p)
        if not os.path.isfile(src):
            print("missing", src)
            continue
        shatter(src, dst, target_chunks=args.chunks)
    return 0


if __name__ == "__main__":
    sys.exit(main())
