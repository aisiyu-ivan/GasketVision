#!/usr/bin/env python3
"""Diagnose whether NG rate comes from sample images or inspection algorithm."""

from __future__ import annotations

import json
import math
from collections import defaultdict
from pathlib import Path

import cv2
import numpy as np

SAMPLES = Path(__file__).resolve().parent.parent
MANIFEST = SAMPLES / "manifest.json"
CONFIG = SAMPLES / "vision_engine.json"


def load_config() -> dict:
    cfg = json.loads(CONFIG.read_text(encoding="utf-8"))
    spec = cfg["spec"]
    return {
        "pxPerMm": cfg["pxPerMm"],
        "center": tuple(cfg["imageCenterPx"]),
        "matchScoreMin": cfg["matchScoreMin"],
        "templatePath": SAMPLES / cfg["templatePath"],
        "outerDiameterMm": spec["outerDiameterMm"],
        "innerDiameterMm": spec["innerDiameterMm"],
        "outerTolMm": spec["outerTolMm"],
        "innerTolMm": spec["innerTolMm"],
        "offsetTolMm": spec["offsetTolMm"],
    }


def measure_ring(gray: np.ndarray, cfg: dict) -> tuple[bool, dict]:
    px = cfg["pxPerMm"]
    cx, cy = cfg["center"]
    od_nom = cfg["outerDiameterMm"]
    id_nom = cfg["innerDiameterMm"]
    expected_od_px = od_nom * px
    expected_id_px = id_nom * px
    roi_half = int(expected_od_px * 0.5 + 24)

    x0 = max(0, int(cx) - roi_half)
    y0 = max(0, int(cy) - roi_half)
    x1 = min(gray.shape[1], x0 + roi_half * 2)
    y1 = min(gray.shape[0], y0 + roi_half * 2)
    patch = gray[y0:y1, x0:x1].copy()
    if patch.shape[0] < 40 or patch.shape[1] < 40:
        return False, {"reason": "roi too small"}

    patch = cv2.GaussianBlur(patch, (5, 5), 1.0)
    _, bright = cv2.threshold(patch, 120, 255, cv2.THRESH_BINARY)
    bright_pixels = int(cv2.countNonZero(bright))
    if bright_pixels < int(expected_od_px * expected_od_px * 0.04):
        return False, {"reason": "too few bright pixels", "brightPixels": bright_pixels}

    m = cv2.moments(bright, True)
    if m["m00"] < 1:
        return False, {"reason": "no bright centroid"}

    centroid = (m["m10"] / m["m00"], m["m01"] / m["m00"])
    origin = (cx - x0, cy - y0)
    center = (centroid[0] + x0, centroid[1] + y0)

    min_inner_r = expected_id_px * 0.35
    max_outer_r = expected_od_px * 0.65
    inner_radii: list[float] = []
    outer_radii: list[float] = []
    for deg in range(0, 360, 2):
        rad = math.radians(deg)
        dx, dy = math.cos(rad), math.sin(rad)
        first_bright = -1
        last_bright = -1
        for step in range(1, int(max_outer_r + 8)):
            x = int(origin[0] + dx * step)
            y = int(origin[1] + dy * step)
            if x < 0 or y < 0 or x >= bright.shape[1] or y >= bright.shape[0]:
                break
            if bright[y, x] > 0:
                if first_bright < 0:
                    first_bright = step
                last_bright = step
        if first_bright < 0 or last_bright <= first_bright:
            continue
        if first_bright >= min_inner_r and last_bright <= max_outer_r and last_bright > first_bright + 2:
            inner_radii.append(first_bright)
            outer_radii.append(last_bright)

    if len(inner_radii) < 24:
        return False, {"reason": "insufficient rays", "rays": len(inner_radii)}

    inner_r = sorted(inner_radii)[len(inner_radii) // 2]
    outer_r = sorted(outer_radii)[len(outer_radii) // 2]
    od_mm = round(outer_r * 2 / px * 100) / 100
    id_mm = round(inner_r * 2 / px * 100) / 100
    off_x = (center[0] - cx) / px
    off_y = (center[1] - cy) / px
    return True, {
        "odMm": round(od_mm, 3),
        "idMm": round(id_mm, 3),
        "offX": round(off_x, 3),
        "offY": round(off_y, 3),
        "hasInnerContour": True,
    }


def match_score(gray: np.ndarray, template_path: Path) -> float:
    tpl = cv2.imread(str(template_path), cv2.IMREAD_GRAYSCALE)
    if tpl is None or gray.shape[0] < tpl.shape[0] or gray.shape[1] < tpl.shape[1]:
        return 0.0
    res = cv2.matchTemplate(gray, tpl, cv2.TM_CCOEFF_NORMED)
    return float(res.max())


def judge(meas: dict, cfg: dict) -> tuple[bool, str]:
    od_err = abs(meas["odMm"] - cfg["outerDiameterMm"])
    id_err = abs(meas["idMm"] - cfg["innerDiameterMm"])
    off_err = math.hypot(meas["offX"], meas["offY"])
    if od_err > cfg["outerTolMm"]:
        return False, "尺寸超差(OD)"
    if id_err > cfg["innerTolMm"]:
        return False, "尺寸超差(ID)"
    if off_err > cfg["offsetTolMm"]:
        return False, "偏心偏移"
    return True, ""


def pixel_geometry(path: Path) -> dict:
    """Ground-truth-ish diameter from bright ring pixels (independent of algo)."""
    gray = cv2.imread(str(path), cv2.IMREAD_GRAYSCALE)
    ys, xs = np.where(gray >= 180)
    if len(xs) < 100:
        return {"brightCount": len(xs)}
    cx, cy = xs.mean(), ys.mean()
    dists = np.hypot(xs - cx, ys - cy)
    return {
        "brightCount": len(xs),
        "estOdPx": round(dists.max() * 2, 1),
        "estIdPx": round(dists.min() * 2, 1),
        "estOdMm": round(dists.max() * 2 / 10, 3),
        "estIdMm": round(dists.min() * 2 / 10, 3),
    }


def main() -> int:
    cfg = load_config()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    stats = defaultdict(lambda: {"total": 0, "algo_ok": 0, "expected_ok": 0, "match_fail": 0, "measure_fail": 0})
    mismatches = []

    for entry in manifest["entries"]:
        case = entry["case"]
        rel = entry["imagePath"]
        path = SAMPLES / rel.replace("/", "\\").replace("station1\\", "station1\\")
        if not path.exists():
            path = SAMPLES / Path(rel).name
        gray = cv2.imread(str(path), cv2.IMREAD_GRAYSCALE)
        if gray is None:
            continue

        stats[case]["total"] += 1
        if entry["expectedOk"]:
            stats[case]["expected_ok"] += 1

        score = match_score(gray, cfg["templatePath"])
        if score < cfg["matchScoreMin"]:
            stats[case]["match_fail"] += 1
            if entry["expectedOk"]:
                mismatches.append((rel, "缺件(模板)", f"score={score:.3f}"))
            continue

        ok_meas, meas = measure_ring(gray, cfg)
        if not ok_meas:
            stats[case]["measure_fail"] += 1
            if entry["expectedOk"]:
                mismatches.append((rel, "缺件(测量)", str(meas)))
            continue

        algo_ok, defect = judge(meas, cfg)
        if algo_ok:
            stats[case]["algo_ok"] += 1

        expected_ok = entry["expectedOk"]
        if algo_ok != expected_ok:
            geo = pixel_geometry(path)
            mismatches.append(
                (
                    rel,
                    f"期望{'OK' if expected_ok else 'NG'}→算法{'OK' if algo_ok else 'NG'}({defect or 'OK'})",
                    f"manifest OD={entry.get('outerDiameterMm')} ID={entry.get('innerDiameterMm')} | "
                    f"algo OD={meas['odMm']} ID={meas['idMm']} off=({meas['offX']},{meas['offY']}) | "
                    f"pixel OD={geo.get('estOdMm')} ID={geo.get('estIdMm')} inner={meas['hasInnerContour']}",
                )
            )

    total = sum(s["total"] for s in stats.values())
    algo_ok_total = sum(s["algo_ok"] for s in stats.values())
    expected_ok_total = sum(s["expected_ok"] for s in stats.values())

    print("=== 诊断结论 ===")
    print(f"样本设计: OK={expected_ok_total}/{total} ({100*expected_ok_total/total:.1f}%)")
    print(f"算法判定: OK={algo_ok_total}/{total} ({100*algo_ok_total/total:.1f}%)")
    print()
    print("按类别:")
    for case in ("ok", "od_oversize", "id_undersize", "eccentric", "missing"):
        s = stats.get(case)
        if not s or s["total"] == 0:
            continue
        print(
            f"  {case:14} n={s['total']:4}  期望OK={s['expected_ok']:4}  "
            f"算法OK={s['algo_ok']:4}  模板失败={s['match_fail']:3}  测量失败={s['measure_fail']:3}"
        )

    false_ng = [m for m in mismatches if "期望OK" in m[1]]
    false_ok = [m for m in mismatches if "期望NG" in m[1] and "→算法OK" in m[1]]
    print()
    print(f"误判: 好件判NG={len(false_ng)}  坏件判OK={len(false_ok)}  总不一致={len(mismatches)}")
    print()
    print("前 12 条不一致样例:")
    for row in mismatches[:12]:
        print(f"  {row[0]}")
        print(f"    {row[1]}")
        print(f"    {row[2]}")

    # Root cause hint
    print()
    ok_stats = stats.get("ok", {})
    if ok_stats.get("measure_fail", 0) > ok_stats.get("total", 1) * 0.1:
        print("→ 主要问题在算法: OK 样本大量测量失败")
    elif len(false_ng) > len(false_ok) and false_ng:
        print("→ 主要问题在算法: 好件被尺寸/偏心容差判成 NG（测量偏差或容差过严）")
    elif len(false_ok) > len(false_ng):
        print("→ 主要问题在算法: 缺陷样本未检出（测量不准或容差过宽）")
    elif expected_ok_total / total < 0.5:
        print("→ 样本比例本身 NG 占比高（当前设计约 40% NG）")
    else:
        print("→ 样本与算法基本一致；若界面 NG 仍高，检查 build 目录是否混有旧图或播放顺序")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
