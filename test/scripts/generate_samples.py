#!/usr/bin/env python3
"""Generate synthetic gasket inspection images and manifest.json (stdlib only)."""

from __future__ import annotations

import argparse
import json
import math
import struct
import zlib
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent.parent

PX_PER_MM = 10.0
IMG_W, IMG_H = 640, 480
CENTER = (IMG_W // 2, IMG_H // 2)
FIDUCIAL_ORIGIN = (30, 30)
NOMINAL_OD = 12.0
NOMINAL_ID = 8.0

CASES = {
    "ok": {"expectedOk": True, "od": 12.01, "id": 8.01, "dx": 0.02, "dy": -0.01, "draw_ring": True},
    "od_oversize": {"expectedOk": False, "od": 12.25, "id": 8.00, "dx": 0.01, "dy": 0.01, "draw_ring": True},
    "id_undersize": {"expectedOk": False, "od": 12.00, "id": 7.75, "dx": 0.01, "dy": 0.01, "draw_ring": True},
    "eccentric": {"expectedOk": False, "od": 12.00, "id": 8.00, "dx": 0.15, "dy": 0.12, "draw_ring": True},
    "missing": {"expectedOk": False, "od": 0.0, "id": 0.0, "dx": 0.0, "dy": 0.0, "draw_ring": False},
}

# 默认 OK 占 60%，其余缺陷均分 40%（避免演示 NG 率过高）
CASE_WEIGHTS = {
    "ok": 6,
    "od_oversize": 1,
    "id_undersize": 1,
    "eccentric": 1,
    "missing": 1,
}


def mm_to_px(mm: float) -> float:
    return mm * PX_PER_MM


class GrayImage:
    def __init__(self, w: int, h: int, fill: int = 0) -> None:
        self.w = w
        self.h = h
        self.pixels = bytearray([fill] * (w * h))

    def set_px(self, x: int, y: int, v: int) -> None:
        if 0 <= x < self.w and 0 <= y < self.h:
            self.pixels[y * self.w + x] = max(0, min(255, v))

    def fill_rect(self, x0: int, y0: int, x1: int, y1: int, v: int) -> None:
        for y in range(y0, y1):
            for x in range(x0, x1):
                self.set_px(x, y, v)

    def draw_circle(self, cx: int, cy: int, r: int, v: int, filled: bool = False) -> None:
        r2 = r * r
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                d2 = (x - cx) ** 2 + (y - cy) ** 2
                if filled and d2 <= r2:
                    self.set_px(x, y, v)
                elif not filled and abs(d2 - r2) <= max(1, r * 2):
                    self.set_px(x, y, v)

    def to_png_bytes(self) -> bytes:
        raw = bytearray()
        for y in range(self.h):
            raw.append(0)
            start = y * self.w
            raw.extend(self.pixels[start : start + self.w])
        compressed = zlib.compress(bytes(raw), 9)

        def chunk(tag: bytes, data: bytes) -> bytes:
            crc = zlib.crc32(tag + data) & 0xFFFFFFFF
            return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

        ihdr = struct.pack(">IIBBBBB", self.w, self.h, 8, 0, 0, 0, 0)
        return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", compressed) + chunk(b"IEND", b"")


def draw_fiducial(img: GrayImage) -> None:
    ox, oy = FIDUCIAL_ORIGIN
    img.fill_rect(ox, oy, ox + 24, oy + 4, 220)
    img.fill_rect(ox, oy, ox + 4, oy + 24, 220)


def draw_fixture(img: GrayImage) -> None:
    img.pixels[:] = bytes([38] * (img.w * img.h))
    draw_fiducial(img)
    img.draw_circle(CENTER[0], CENTER[1], int(mm_to_px(NOMINAL_OD) / 2 + 6), 55, filled=False)


def draw_ring(img: GrayImage, od_mm: float, id_mm: float, dx_mm: float, dy_mm: float) -> None:
    cx = CENTER[0] + mm_to_px(dx_mm)
    cy = CENTER[1] + mm_to_px(dy_mm)
    inner_r = mm_to_px(id_mm) / 2.0
    outer_r = mm_to_px(od_mm) / 2.0
    x0 = max(0, int(cx - outer_r - 2))
    x1 = min(img.w, int(cx + outer_r + 3))
    y0 = max(0, int(cy - outer_r - 2))
    y1 = min(img.h, int(cy + outer_r + 3))
    for y in range(y0, y1):
        for x in range(x0, x1):
            dist = math.hypot(x - cx, y - cy)
            if (inner_r - 0.5) <= dist <= (outer_r + 0.5):
                img.set_px(x, y, 205)


def render_image(case_key: str, seed: int) -> tuple[GrayImage, dict]:
    spec = CASES[case_key]
    img = GrayImage(IMG_W, IMG_H, 0)
    draw_fixture(img)
    od = spec["od"] + (seed % 3) * 0.005
    idv = spec["id"] - (seed % 2) * 0.004
    dx = spec["dx"] + (seed % 5) * 0.002
    dy = spec["dy"] - (seed % 4) * 0.002
    if spec["draw_ring"]:
        draw_ring(img, od, idv, dx, dy)
    meta = {
        "expectedOk": spec["expectedOk"],
        "case": case_key,
        "outerDiameterMm": round(od, 3) if spec["draw_ring"] else None,
        "innerDiameterMm": round(idv, 3) if spec["draw_ring"] else None,
    }
    return img, meta


def save_fiducial_template(out_dir: Path) -> str:
    full = GrayImage(IMG_W, IMG_H, 0)
    draw_fiducial(full)
    x, y = FIDUCIAL_ORIGIN
    crop = GrayImage(32, 32, 0)
    for cy in range(32):
        for cx in range(32):
            crop.set_px(cx, cy, full.pixels[(y + cy) * IMG_W + (x + cx)])
    tpl_dir = out_dir / "templates"
    tpl_dir.mkdir(parents=True, exist_ok=True)
    rel = "templates/fiducial_L.png"
    (out_dir / rel).write_bytes(crop.to_png_bytes())
    return rel


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=TEST_DIR, help="Output directory (default: test/)")
    parser.add_argument("--per-case", type=int, default=5, help="Images per defect case")
    parser.add_argument("--total", type=int, default=None, help="Total images, split evenly across cases")
    args = parser.parse_args()

    case_keys = list(CASES.keys())
    case_count = len(case_keys)
    per_case_map: dict[str, int] = {}
    if args.total is not None:
        if args.total < case_count:
            raise SystemExit(f"--total must be at least {case_count}")
        weight_sum = sum(CASE_WEIGHTS.get(k, 1) for k in case_keys)
        assigned = 0
        for i, case_key in enumerate(case_keys):
            weight = CASE_WEIGHTS.get(case_key, 1)
            if i == len(case_keys) - 1:
                per_case_map[case_key] = args.total - assigned
            else:
                n = args.total * weight // weight_sum
                per_case_map[case_key] = n
                assigned += n
    else:
        for case_key in case_keys:
            per_case_map[case_key] = args.per_case

    out_dir = args.out.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    template_rel = save_fiducial_template(out_dir)
    manifest: dict = {
        "pxPerMm": PX_PER_MM,
        "templatePath": template_rel,
        "entries": [],
    }

    station_dir = out_dir / "station1"
    station_dir.mkdir(parents=True, exist_ok=True)
    for old in station_dir.glob("*.png"):
        old.unlink()

    max_per_case = max(per_case_map.values()) if per_case_map else args.per_case
    index_width = max(2, len(str(max_per_case - 1)))
    for case_key in case_keys:
        per_case = per_case_map[case_key]
        for i in range(per_case):
            img, meta = render_image(case_key, seed=100 + i)
            filename = f"{case_key}_{i:0{index_width}d}.png"
            rel_path = f"station1/{filename}"
            (station_dir / filename).write_bytes(img.to_png_bytes())
            manifest["entries"].append({"stationId": 1, "imagePath": rel_path.replace("\\", "/"), **meta})

    (out_dir / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    ok_n = per_case_map.get("ok", 0)
    total_n = len(manifest["entries"])
    print(f"Wrote {total_n} images to {out_dir} (OK={ok_n}, NG≈{total_n - ok_n})")


if __name__ == "__main__":
    main()
