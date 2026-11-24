#!/usr/bin/env python3
"""Quickslot Design Studio — 2.5D Heightmap-to-PBR Metallic UI Frame Forge.

Generates cohesive, dark-fantasy RGBA PNG theme packs for Quickslot schema 3:
  - Square, Card, Panel, Diamond, Circle, Hexagon (bg, border, selected, equipped)
  - Segmented Radial Wheel (ring_bg, outer_rim, inner_rim, hub, pointer, spoke)
  - Rule divider, faceted equipped/charge markers, feathered equipment panel
  - Optional external/AI PNG ornament stamp import with automatic symmetry & height extraction

Usage:
  python3 tools/design-studio/forge.py --flagships
  python3 tools/design-studio/forge.py --theme oathvein --out assets/themes/oathvein
  python3 tools/design-studio/forge.py --recipe my_recipe.json --out Data/Quickslot/themes/my-theme
"""

from __future__ import annotations
import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

import numpy as np
from PIL import Image, ImageFilter

STUDIO_DIR = Path(__file__).resolve().parent
if str(STUDIO_DIR) not in sys.path:
    sys.path.insert(0, str(STUDIO_DIR))

from materials import ALL_THEME_RECIPES, FLAGSHIP_RECIPES, MATERIALS, resolve_recipe
from ornaments import radial_ornaments, rect_ornaments
from stamps import import_stamp

REPO_ROOT = STUDIO_DIR.parent.parent


# ---------------------------------------------------------------------------
# Noise & Signed Distance Field Primitives
# ---------------------------------------------------------------------------

def fbm_noise(h: int, w: int, seed: int, octaves=(8, 20, 48, 112)) -> np.ndarray:
    rng = np.random.default_rng(seed)
    acc = np.zeros((h, w), dtype=np.float32)
    wsum = 0.0
    base = min(h, w)
    for i, freq in enumerate(octaves):
        gh = max(2, int(round(h * freq / max(base, 1))))
        gw = max(2, int(round(w * freq / max(base, 1))))
        gh = min(gh, h)
        gw = min(gw, w)
        low = rng.uniform(0.0, 1.0, (gh, gw)).astype(np.float32)
        up = np.asarray(
            Image.fromarray((low * 255.0).astype(np.uint8)).resize((w, h), Image.Resampling.BICUBIC),
            dtype=np.float32,
        ) / 255.0
        wt = 0.52 ** i
        acc += up * wt
        wsum += wt
    return acc / max(wsum, 1e-6)


def brushed_noise(h: int, w: int, seed: int) -> np.ndarray:
    """Anisotropic horizontal + vertical micro-brushed metal streaks."""
    rng = np.random.default_rng(seed)
    raw = rng.uniform(0.0, 1.0, (max(4, h // 2), max(4, w // 12))).astype(np.float32)
    horiz = np.asarray(
        Image.fromarray((raw * 255.0).astype(np.uint8)).resize((w, h), Image.Resampling.BILINEAR),
        dtype=np.float32,
    ) / 255.0
    raw2 = rng.uniform(0.0, 1.0, (max(4, h // 12), max(4, w // 2))).astype(np.float32)
    vert = np.asarray(
        Image.fromarray((raw2 * 255.0).astype(np.uint8)).resize((w, h), Image.Resampling.BILINEAR),
        dtype=np.float32,
    ) / 255.0
    return 0.5 * (horiz + vert)


def chamfer_rect_sdf(w: int, h: int, pad: float, cut: float) -> np.ndarray:
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cx, cy = (w - 1) * 0.5, (h - 1) * 0.5
    hx, hy = cx - pad, cy - pad
    dx, dy = np.abs(xx - cx), np.abs(yy - cy)
    qx, qy = dx - hx, dy - hy
    box = np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0)) + np.minimum(np.maximum(qx, qy), 0.0)
    if cut <= 0.5:
        return box
    chamfer = ((dx - (hx - cut)) + (dy - (hy - cut)) - cut) * 0.70710678
    return np.maximum(box, chamfer)


def circle_sdf(size: int, radius: float) -> np.ndarray:
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    c = (size - 1) * 0.5
    return np.hypot(xx - c, yy - c) - radius


def diamond_sdf(size: int, radius: float) -> np.ndarray:
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    c = (size - 1) * 0.5
    # Exact Euclidean distance to a 45-degree rotated square (diamond) of half-diagonal `radius`
    return (np.abs(xx - c) + np.abs(yy - c) - radius) * 0.70710678


def hexagon_sdf(size: int, radius: float) -> np.ndarray:
    """Exact pointy-top regular hexagon SDF matching Hud.cpp hexVertices (vertex at top)."""
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    c = (size - 1) * 0.5
    # Rotate coordinates 90 deg so standard flat-top formula yields pointy-top matching -pi/2 start
    px = np.abs(yy - c)
    py = np.abs(xx - c)
    # Inradius of regular hexagon with circumradius `radius` is radius * sqrt(3)/2
    r_in = radius * 0.8660254
    k0, k1, k2 = -0.8660254, 0.5, 0.57735027
    dot = np.minimum(k0 * px + k1 * py, 0.0)
    px2 = px - 2.0 * dot * k0
    py2 = py - 2.0 * dot * k1
    px2 = px2 - np.clip(px2, -k2 * r_in, k2 * r_in)
    py2 = py2 - r_in
    return np.hypot(px2, py2) * np.sign(py2)


# ---------------------------------------------------------------------------
# Core 2.5D Heightmap -> PBR Metallic Shading
# ---------------------------------------------------------------------------

def _to_rgba_image(rgb: np.ndarray, alpha: np.ndarray) -> Image.Image:
    rgba = np.zeros((rgb.shape[0], rgb.shape[1], 4), dtype=np.uint8)
    rgba[:, :, :3] = np.clip(rgb * 255.0, 0.0, 255.0).astype(np.uint8)
    rgba[:, :, 3] = np.clip(alpha * 255.0, 0.0, 255.0).astype(np.uint8)
    return Image.fromarray(rgba, mode="RGBA")


def _shade_metal(
    sdf: np.ndarray,
    rim_w: float,
    pad: float,
    recipe: Dict[str, Any],
    orn_h: Optional[np.ndarray] = None,
    orn_acc: Optional[np.ndarray] = None,
    orn_em: Optional[np.ndarray] = None,
    seed_offset: int = 0,
    include_inner_shadow: bool = True,
) -> Tuple[Image.Image, Image.Image, Image.Image, Image.Image]:
    """Return `(bg_png, border_png, selected_png, equipped_png)` for any SDF shape."""
    h, w = sdf.shape
    seed = int(recipe.get("seed", 17)) + seed_offset

    grain = fbm_noise(h, w, seed=seed, octaves=(6, 18, 44, 108))
    fine = fbm_noise(h, w, seed=seed + 97, octaves=(36, 84, 168))
    brush = brushed_noise(h, w, seed=seed + 211)

    grain_amt = float(recipe.get("grain", 0.40))
    wear_amt = float(recipe.get("wear", 0.45))
    rough = float(recipe.get("roughness", 0.34))
    bevel_h = float(recipe.get("bevelHeight", 4.4))

    # Organic broken edge-wear mask (prevents sterile continuous computer lines!)
    wear = np.clip(
        (grain - 0.34) * (1.5 + wear_amt * 1.4) + (fine - 0.5) * 0.75 + (brush - 0.5) * 0.55,
        0.0,
        1.0,
    )

    # Anti-aliased shape masks
    outer_mask = np.clip(0.5 - sdf, 0.0, 1.0)
    inner_hole = np.clip(0.5 - (sdf + rim_w), 0.0, 1.0)
    band = np.clip(outer_mask - inner_hole, 0.0, 1.0)

    # Sculpted 2-tier forged bevel profile across rim width [0=outer, 1=inner]
    t = np.clip(-sdf / max(rim_w, 1e-3), 0.0, 1.0)
    profile = np.maximum(0.0, np.sin(t * math.pi)) ** 0.72
    ridge_main = np.exp(-((t - 0.34) / 0.15) ** 2) * 0.60
    ridge_inner = np.exp(-((t - 0.84) / 0.10) ** 2) * 0.36
    base_height = (profile * 0.62 + ridge_main + ridge_inner) * band

    # Stamp sculpted ornaments directly into heightfield
    if orn_h is None:
        orn_h = np.zeros((h, w), dtype=np.float32)
    if orn_acc is None:
        orn_acc = np.zeros((h, w), dtype=np.float32)
    if orn_em is None:
        orn_em = np.zeros((h, w), dtype=np.float32)

    # Allow ornaments to slightly extend metal mask where they protrude (e.g. Daedric corner tips)
    orn_mask = np.clip(orn_h * 1.4, 0.0, 1.0)
    metal_mask = np.clip(np.maximum(band, orn_mask), 0.0, 1.0)

    height = (
        base_height
        + orn_h * 0.48
        + (grain - 0.5) * (0.16 * grain_amt) * metal_mask
        + (brush - 0.5) * (0.10 * grain_amt) * metal_mask
        + (fine - 0.5) * (0.08 * grain_amt) * metal_mask
    )

    # Sobel/central-difference surface normals
    gy, gx = np.gradient(height * bevel_h)
    nx, ny, nz = -gx, -gy, np.ones_like(gx)
    inv_len = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz + 1e-8)
    nx *= inv_len
    ny *= inv_len
    nz *= inv_len

    # Dual studio key + cool rim lighting (consistent top-left lighting across all shapes!)
    l1 = np.array([-0.54, -0.66, 0.52], dtype=np.float32)
    l1 /= np.linalg.norm(l1)
    l2 = np.array([0.48, 0.54, 0.69], dtype=np.float32)
    l2 /= np.linalg.norm(l2)
    v = np.array([0.0, 0.0, 1.0], dtype=np.float32)
    h1 = (l1 + v) / np.linalg.norm(l1 + v)
    h2 = (l2 + v) / np.linalg.norm(l2 + v)

    ndotl1 = np.clip(nx * l1[0] + ny * l1[1] + nz * l1[2], 0.0, 1.0)
    ndotl2 = np.clip(nx * l2[0] + ny * l2[1] + nz * l2[2], 0.0, 1.0)

    shininess = 14.0 + (1.0 - rough) * 34.0 + (1.0 - wear) * 18.0
    spec1 = np.clip(nx * h1[0] + ny * h1[1] + nz * h1[2], 0.0, 1.0) ** shininess
    spec2 = np.clip(nx * h2[0] + ny * h2[1] + nz * h2[2], 0.0, 1.0) ** (shininess * 0.75)

    # Crevice ambient occlusion along outer/inner grooves
    seam_ao = np.clip(np.sin(t * math.pi) * 1.48, 0.16, 1.0)
    seam_ao = np.maximum(seam_ao, orn_mask * 0.78)

    # Razor-thin 1px etched dark outer & inner forged contour lines
    outer_contour = np.exp(-((sdf + 0.65) / 0.85) ** 2) * band
    inner_contour = np.exp(-((sdf + rim_w - 0.65) / 0.85) ** 2) * band
    contour = np.clip(outer_contour * 0.90 + inner_contour * 0.76, 0.0, 0.94)

    # Broken metallic edge catch-light along raised bevel crest
    bevel_crest = np.exp(-((t - 0.30) / 0.09) ** 2) * band * (0.35 + 0.65 * ndotl1)
    orn_crest = orn_h * (0.30 + 0.70 * ndotl1) * 0.45
    glint = np.clip(
        (spec1 * 0.92 + bevel_crest * 0.56 + orn_crest) * (0.26 + 0.92 * wear) + spec2 * 0.24,
        0.0,
        1.35,
    )

    # Material palette
    base_rgb = np.array(recipe["metalBase"], dtype=np.float32) / 255.0
    hi_rgb = np.array(recipe["metalHi"], dtype=np.float32) / 255.0
    dark_rgb = np.array(recipe["metalDark"], dtype=np.float32) / 255.0
    acc_rgb = np.array(recipe["accentMetal"], dtype=np.float32) / 255.0
    pat_rgb = np.array(recipe["patina"], dtype=np.float32) / 255.0
    em_rgb = np.array(recipe["emissive"], dtype=np.float32) / 255.0
    pat_str = float(recipe.get("patinaStrength", 0.25))
    em_str = float(recipe.get("emissiveStrength", 0.0))

    acc_mix = np.clip(orn_acc * 0.75, 0.0, 0.85)[:, :, None]
    albedo = base_rgb[None, None, :] * (1.0 - acc_mix) + acc_rgb[None, None, :] * acc_mix
    albedo = albedo * (0.80 + (0.38 * grain_amt) * grain[:, :, None] + (0.14 * grain_amt) * brush[:, :, None])

    # Patina settles subtly in low-AO crevices
    crevice = np.clip((1.0 - seam_ao) * pat_str * (0.6 + 0.8 * grain), 0.0, 0.65)[:, :, None]
    albedo = albedo * (1.0 - crevice) + pat_rgb[None, None, :] * crevice

    lit = (
        dark_rgb[None, None, :] * 0.42
        + albedo * (0.28 + 0.96 * ndotl1[:, :, None] + 0.24 * ndotl2[:, :, None]) * seam_ao[:, :, None]
        + hi_rgb[None, None, :] * (glint[:, :, None] * 0.82)
    )
    # Apply dark etched outer/inner contours
    lit = lit * (1.0 - contour[:, :, None]) + dark_rgb[None, None, :] * 0.18 * contour[:, :, None]
    # Add subtle emissive inlay glow (e.g. Daedric ember veins, Dwemer aetherium dots)
    if em_str > 0.01:
        lit = np.clip(lit + em_rgb[None, None, :] * (orn_em[:, :, None] * em_str), 0.0, 1.0)

    # Soft feathered outer drop shadow (on the border layer so it stays grounded even at low bgOpacity)
    sh_op = float(recipe.get("shadowOpacity", 0.60))
    shadow = np.exp(-(np.maximum(sdf, 0.0) / max(pad * 0.42, 2.0)) ** 2) * (1.0 - metal_mask) * sh_op
    # Subtle inner cast shadow right inside the metallic rim
    in_sh = np.zeros_like(shadow)
    if include_inner_shadow:
        in_dist = np.maximum(0.0, -(sdf + rim_w))
        in_sh = np.exp(-(in_dist / max(rim_w * 0.85, 2.5)) ** 2) * inner_hole * 0.42

    border_a = np.clip(metal_mask + (shadow + in_sh) * (1.0 - metal_mask), 0.0, 1.0)
    # Premultiplied composite of dark shadow + lit metal -> clean unpremultiplied RGBA
    pre_rgb = lit * metal_mask[:, :, None]
    safe_a = np.maximum(border_a[:, :, None], 1e-4)
    border_rgb = np.clip(pre_rgb / safe_a, 0.0, 1.0)
    border_img = _to_rgba_image(border_rgb, border_a)

    # -----------------------------------------------------------------------
    # Background Layer (`*_bg.png`): smoked-glass interior with subtle vignette & grain
    # -----------------------------------------------------------------------
    bg_base = np.array(recipe["bgColor"], dtype=np.float32) / 255.0
    bg_op = float(recipe.get("bgOpacity", 0.82))
    in_dist = np.maximum(0.0, -(sdf + rim_w * 0.65))
    vig = np.clip(1.0 - np.exp(-in_dist / max(min(w, h) * 0.16, 8.0)), 0.0, 1.0)
    # Subtle vertical ambient sheen (slightly lighter near top like light catching smoked glass)
    yy = np.linspace(0.0, 1.0, h, dtype=np.float32)[:, None]
    top_sheen = (1.0 - yy) * 0.06
    bg_rgb = bg_base[None, None, :] * (0.76 + 0.34 * grain[:, :, None]) * (0.72 + 0.28 * vig[:, :, None]) + top_sheen[:, :, None]
    bg_mask = np.clip(0.5 - (sdf + rim_w * 0.55), 0.0, 1.0)
    bg_a = np.clip(bg_mask * (bg_op - 0.10 * vig + (grain - 0.5) * 0.04), 0.0, 1.0)
    bg_img = _to_rgba_image(np.clip(bg_rgb, 0.0, 1.0), bg_a)

    # -----------------------------------------------------------------------
    # Luminous Metallic Selected Overlay (`*_selected.png`, tinted by selection/accent)
    # Delicate glowing bevel catch-light + lit corner clasps + feathered inner wash
    # -----------------------------------------------------------------------
    rim_sheen = np.exp(-((t - 0.38) / 0.26) ** 2) * band * (0.58 + 0.42 * wear)
    outer_lip = np.exp(-((sdf + 0.4) / 1.4) ** 2) * 0.45
    inner_wash = np.exp(-(np.maximum(0.0, -(sdf + rim_w)) / max(min(w, h) * 0.12, 8.0)) ** 1.4) * inner_hole * 0.20
    clasp_glow = orn_mask * 0.82
    sel_a = np.clip(rim_sheen * 0.88 + outer_lip * 0.35 + inner_wash + clasp_glow * 0.65, 0.0, 0.92)
    # Neutral white/silver gradient (slightly brighter on beveled crests) so any accent tint looks metallic!
    sel_val = np.clip(0.84 + 0.16 * (spec1 + bevel_crest + orn_h), 0.0, 1.0)
    sel_rgb = np.repeat(sel_val[:, :, None], 3, axis=2)
    selected_img = _to_rgba_image(sel_rgb, sel_a)

    # -----------------------------------------------------------------------
    # Equipped Overlay (`*_equipped.png`, tinted by equipped color)
    # Restrained metallic sheen along beveled rim + corner clasp gleam (zero neon box!)
    # -----------------------------------------------------------------------
    eq_rim = np.exp(-((t - 0.36) / 0.20) ** 2) * band * (0.45 + 0.45 * wear)
    eq_wash = np.exp(-(np.maximum(0.0, -(sdf + rim_w)) / max(min(w, h) * 0.07, 5.0)) ** 1.5) * inner_hole * 0.12
    eq_a = np.clip(eq_rim * 0.66 + orn_mask * 0.52 + eq_wash, 0.0, 0.78)
    eq_val = np.clip(0.86 + 0.14 * (spec1 + bevel_crest), 0.0, 1.0)
    eq_rgb = np.repeat(eq_val[:, :, None], 3, axis=2)
    equipped_img = _to_rgba_image(eq_rgb, eq_a)

    return bg_img, border_img, selected_img, equipped_img


# ---------------------------------------------------------------------------
# Shape Builders (Cohesive Logical-Pixel Scale: 3 source px == 1 logical px)
# ---------------------------------------------------------------------------

PX_PER_LOGICAL = 3.0


def _merge_stamp(
    orn_h: np.ndarray,
    orn_acc: np.ndarray,
    stamp_path: Optional[str],
    symmetry: str,
    strength: float,
) -> Tuple[np.ndarray, np.ndarray]:
    if not stamp_path:
        return orn_h, orn_acc
    p = Path(stamp_path)
    if not p.is_file():
        return orn_h, orn_acc
    h, w = orn_h.shape
    _, st_h = import_stamp(p, w, h, symmetry=symmetry, bg_mode="auto", clear_center_ratio=0.72)
    merged_h = np.clip(np.maximum(orn_h, st_h * strength), 0.0, 1.0)
    merged_acc = np.clip(np.maximum(orn_acc, st_h * strength * 0.8), 0.0, 1.0)
    return merged_h, merged_acc


def forge_rect_family(
    recipe: Dict[str, Any],
    w: int,
    h: int,
    seed_offset: int,
    scale_mul: float = 1.0,
) -> Tuple[Image.Image, Image.Image, Image.Image, Image.Image]:
    rim_w = float(recipe.get("rimPx", 3.4)) * PX_PER_LOGICAL * scale_mul
    cut = float(recipe.get("chamferPx", 7.0)) * PX_PER_LOGICAL * scale_mul
    outset = float(recipe.get("outset", 0.0))
    pad = (10.0 if outset > 0.0 else 6.0) * scale_mul
    sdf = chamfer_rect_sdf(w, h, pad=pad, cut=cut)
    orn_h, orn_acc, orn_em = rect_ornaments(recipe.get("ornament", "oathvein_clasp"), w, h, pad, cut, rim_w)
    orn_h, orn_acc = _merge_stamp(
        orn_h, orn_acc,
        recipe.get("stampPath"),
        recipe.get("stampSymmetry", "quad"),
        float(recipe.get("stampStrength", 0.85)),
    )
    return _shade_metal(sdf, rim_w, pad, recipe, orn_h, orn_acc, orn_em, seed_offset=seed_offset)


def forge_radial_shape(
    recipe: Dict[str, Any],
    kind: str,
    size: int = 256,
    seed_offset: int = 0,
) -> Tuple[Image.Image, Image.Image, Image.Image, Image.Image]:
    rim_w = float(recipe.get("rimPx", 3.4)) * PX_PER_LOGICAL
    outset = float(recipe.get("outset", 0.0))
    pad = 12.0 if outset > 0.0 else 7.0
    radius = size * 0.5 - pad
    if kind == "circle":
        sdf = circle_sdf(size, radius)
        folds = 8
    elif kind == "diamond":
        sdf = diamond_sdf(size, radius)
        folds = 4
    else:
        sdf = hexagon_sdf(size, radius)
        folds = 6
    orn_h, orn_acc, orn_em = radial_ornaments(recipe.get("ornament", "oathvein_clasp"), size, radius, rim_w, n_fold=folds)
    # Constrain radial ornaments cleanly near the actual shape band so they sit flush on diamond/hex vertices
    band_prox = np.clip(1.0 - np.abs(sdf + rim_w * 0.45) / (rim_w * 1.65), 0.0, 1.0)
    orn_h *= band_prox
    orn_acc *= band_prox
    orn_em *= band_prox
    return _shade_metal(sdf, rim_w, pad, recipe, orn_h, orn_acc, orn_em, seed_offset=seed_offset)


def forge_wheel_assets(recipe: Dict[str, Any]) -> Dict[str, Image.Image]:
    """Forge cohesive segmented radial wheel artwork sharing the theme's exact PBR material."""
    seed = int(recipe.get("seed", 17))
    out: Dict[str, Image.Image] = {}

    # 1. Polar annular ring background (256x256: X=angle, Y=inner->outer radial)
    sz = 256
    grain = fbm_noise(sz, sz, seed=seed + 301, octaves=(6, 16, 44))
    yy = np.linspace(0.0, 1.0, sz, dtype=np.float32)[:, None]
    # Subtle depth shading near inner & outer edges of the ring
    edge_shade = np.clip(np.maximum(0.0, np.sin(yy * math.pi)) ** 0.45, 0.35, 1.0)
    bg_c = np.array(recipe["bgColor"], dtype=np.float32) / 255.0
    acc_c = np.array(recipe["accentMetal"], dtype=np.float32) / 255.0
    ring_rgb = bg_c[None, None, :] * (0.70 + 0.35 * grain[:, :, None]) * (0.72 + 0.28 * edge_shade[:, :, None])
    ring_rgb = np.clip(ring_rgb + acc_c[None, None, :] * ((1.0 - edge_shade[:, :, None]) * 0.06), 0.0, 1.0)
    ring_a = np.full((sz, sz), float(recipe.get("bgOpacity", 0.82)) * 0.92, dtype=np.float32)
    out["wheel_ring_bg.png"] = _to_rgba_image(ring_rgb, ring_a)

    # 2. Outer metallic rim (512x512, transparent center)
    osize = 512
    rim_w = float(recipe.get("rimPx", 3.4)) * 3.2
    pad = 14.0
    orad = osize * 0.5 - pad
    sdf_o = circle_sdf(osize, orad)
    oh, oa, oe = radial_ornaments(recipe.get("ornament", "oathvein_clasp"), osize, orad, rim_w, n_fold=8)
    _, outer_border, _, _ = _shade_metal(sdf_o, rim_w, pad, recipe, oh, oa, oe, seed_offset=311, include_inner_shadow=False)
    out["wheel_outer_rim.png"] = outer_border

    # 3. Inner metallic collar (384x384, delicate thinner rim)
    isize = 384
    irim_w = float(recipe.get("rimPx", 3.4)) * 2.4
    ipad = 10.0
    irad = isize * 0.5 - ipad
    sdf_i = circle_sdf(isize, irad)
    ih, ia, ie = radial_ornaments(recipe.get("ornament", "oathvein_clasp"), isize, irad, irim_w, n_fold=8)
    _, inner_border, _, _ = _shade_metal(sdf_i, irim_w, ipad, recipe, ih * 0.65, ia * 0.65, ie * 0.65, seed_offset=319, include_inner_shadow=False)
    out["wheel_inner_rim.png"] = inner_border

    # 4. Hub background & border (256x256)
    hub_bg, hub_border, _, _ = forge_radial_shape(recipe, "circle", size=256, seed_offset=331)
    out["wheel_hub_bg.png"] = hub_bg
    out["wheel_hub_border.png"] = hub_border

    # 5. Subtle hub overlay (delicate forged compass hairline ring at 18% opacity)
    hs = 256
    sdf_ov = np.abs(circle_sdf(hs, hs * 0.36)) - 1.5
    ov_a = np.clip(0.5 - sdf_ov, 0.0, 1.0) * 0.22
    hi_c = np.array(recipe["metalHi"], dtype=np.float32) / 255.0
    ov_rgb = np.broadcast_to(hi_c[None, None, :], (hs, hs, 3)).copy()
    out["wheel_hub_overlay.png"] = _to_rgba_image(ov_rgb, ov_a)

    # 6. Wheel pointer (beveled upward metallic arrowhead, 96x96)
    ps = 96
    yy, xx = np.mgrid[0:ps, 0:ps].astype(np.float32)
    nx = np.abs(xx - (ps - 1) * 0.5) / (ps * 0.5)
    ny = (yy - ps * 0.14) / (ps * 0.72)
    # Tapered chevron arrowhead SDF approximation
    tri = np.maximum(nx - np.clip(ny * 0.72, 0.0, 1.0), np.maximum(-ny, ny - 0.85))
    notch = 0.72 - ny - nx * 0.45
    shape_a = np.clip(-np.maximum(tri, -notch) * 18.0, 0.0, 1.0)
    # Bevel crest down the vertical centerline
    crest = np.clip(1.0 - nx * 2.2, 0.45, 1.0)
    ptr_rgb = np.repeat(crest[:, :, None], 3, axis=2)
    out["wheel_pointer.png"] = _to_rgba_image(ptr_rgb, shape_a)

    # 7. Radial wheel spoke divider (24x256 tapered metallic hairline)
    sw, sh = 24, 256
    yy, xx = np.mgrid[0:sh, 0:sw].astype(np.float32)
    dx = np.abs(xx - (sw - 1) * 0.5)
    taper = np.maximum(0.0, np.sin(np.linspace(0.0, math.pi, sh, dtype=np.float32)))[:, None] ** 0.5
    sp_a = np.clip(1.0 - dx / (1.8 * taper + 0.35), 0.0, 1.0) * 0.65 * taper
    sp_rgb = np.broadcast_to((np.array(recipe["metalHi"], dtype=np.float32) / 255.0)[None, None, :], (sh, sw, 3)).copy()
    out["wheel_spoke.png"] = _to_rgba_image(sp_rgb, sp_a)

    return out


def forge_misc_assets(recipe: Dict[str, Any]) -> Dict[str, Image.Image]:
    """Forge divider rule, faceted markers, and feathered equipment panel."""
    out: Dict[str, Image.Image] = {}
    base_c = np.array(recipe["metalBase"], dtype=np.float32) / 255.0
    hi_c = np.array(recipe["metalHi"], dtype=np.float32) / 255.0
    acc_c = np.array(recipe["accentMetal"], dtype=np.float32) / 255.0
    bg_c = np.array(recipe["bgColor"], dtype=np.float32) / 255.0

    # 1. Divider rule (384x24): tapered forged hairline + centered faceted diamond finial
    rw, rh = 384, 24
    yy, xx = np.mgrid[0:rh, 0:rw].astype(np.float32)
    cx, cy = (rw - 1) * 0.5, (rh - 1) * 0.5
    dx, dy = np.abs(xx - cx), np.abs(yy - cy)
    taper = np.clip(1.0 - (dx / (rw * 0.48)) ** 1.6, 0.0, 1.0)
    line_a = np.clip(1.0 - dy / 1.4, 0.0, 1.0) * taper * 0.72
    dia_d = (dx / 1.45 + dy) - 6.0
    dia_a = np.clip(0.5 - dia_d, 0.0, 1.0)
    rule_a = np.clip(np.maximum(line_a, dia_a), 0.0, 1.0)
    bevel = np.clip(1.0 - (xx - cx) / 14.0 - (yy - cy) / 10.0, 0.4, 1.2)
    rule_rgb = np.clip(
        base_c[None, None, :] * (1.0 - dia_a[:, :, None]) * 0.6
        + hi_c[None, None, :] * (0.55 * line_a[:, :, None] + 0.65 * dia_a[:, :, None] * bevel[:, :, None])
        + acc_c[None, None, :] * (0.45 * dia_a[:, :, None]),
        0.0,
        1.0,
    )
    out["rule.png"] = _to_rgba_image(rule_rgb, rule_a)

    # 2. Faceted diamond markers (equipped & charge pip, 64x64 neutral white so tint roles work)
    for name, aspect in (("marker_equipped.png", 1.0), ("marker_pip.png", 0.85)):
        ms = 64
        yy, xx = np.mgrid[0:ms, 0:ms].astype(np.float32)
        c = (ms - 1) * 0.5
        dx, dy = (xx - c) / aspect, (yy - c)
        dist = (np.abs(dx) + np.abs(dy)) - (ms * 0.36)
        mask = np.clip(0.5 - dist, 0.0, 1.0)
        # 4-facet beveled gemstone shading
        facet = 0.72 + 0.28 * np.where(dx < 0, 1.0, 0.35) * np.where(dy < 0, 1.0, 0.55)
        edge = np.exp(-((dist + 1.2) / 1.4) ** 2) * 0.45
        val = np.clip(facet * 0.85 + edge, 0.0, 1.0)
        rgb = np.repeat(val[:, :, None], 3, axis=2)
        out[name] = _to_rgba_image(rgb, mask)

    # 3. Equipment panel (256x128): delicate feathered horizontal smoked backdrop + subtle left metallic bar
    ew, eh = 256, 128
    grain = fbm_noise(eh, ew, seed=int(recipe.get("seed", 17)) + 409, octaves=(8, 24, 64))
    ux = np.linspace(0.0, 1.0, ew, dtype=np.float32)[None, :]
    uy = np.linspace(0.0, 1.0, eh, dtype=np.float32)[:, None]
    fade_x = np.clip((1.0 - ux) ** 1.35, 0.0, 1.0)
    fade_y = np.clip(np.maximum(0.0, np.sin(uy * math.pi)) ** 0.25, 0.0, 1.0)
    eq_bg_a = fade_x * fade_y * float(recipe.get("bgOpacity", 0.80)) * 0.88
    eq_bg_rgb = np.clip(bg_c[None, None, :] * (0.82 + 0.30 * grain[:, :, None]), 0.0, 1.0)
    out["equip_bg.png"] = _to_rgba_image(eq_bg_rgb, eq_bg_a)

    # Crisp thin left forged bar (3px wide in 256px -> ~1.5px logical!) fading softly at top/bottom
    xx = np.arange(ew, dtype=np.float32)[None, :]
    bar_m = np.clip(1.0 - np.abs(xx - 2.0) / 2.2, 0.0, 1.0) * fade_y
    bar_c = np.clip(acc_c * 0.55 + hi_c * 0.55, 0.0, 1.0)
    bar_rgb = np.broadcast_to(bar_c[None, None, :], (eh, ew, 3)).copy()
    out["equip_border.png"] = _to_rgba_image(bar_rgb, bar_m * 0.88)

    return out


# ---------------------------------------------------------------------------
# Complete Theme Pack Export
# ---------------------------------------------------------------------------

def forge_theme_pack(
    raw_recipe: Dict[str, Any],
    out_dir: Path,
    update_manifest: bool = True,
    clean_legacy_svgs: bool = False,
) -> Dict[str, Any]:
    """Forge all PBR PNG assets into `out_dir` and optionally update `theme.json`."""
    recipe = resolve_recipe(raw_recipe)
    out_dir.mkdir(parents=True, exist_ok=True)
    outset = float(recipe.get("outset", 0.0))

    # 1. Square / Panel / Card (all share exact 3 source px = 1 logical px rim thickness!)
    sq_bg, sq_b, sq_s, sq_e = forge_rect_family(recipe, 256, 256, seed_offset=1)
    sq_bg.save(out_dir / "square_bg.png")
    sq_b.save(out_dir / "square_border.png")
    sq_s.save(out_dir / "square_selected.png")
    sq_e.save(out_dir / "square_equipped.png")

    card_bg, card_b, card_s, card_e = forge_rect_family(recipe, 384, 144, seed_offset=7)
    card_bg.save(out_dir / "card_bg.png")
    card_b.save(out_dir / "card_border.png")
    card_s.save(out_dir / "card_selected.png")
    card_e.save(out_dir / "card_equipped.png")

    pan_bg, pan_b, _, _ = forge_rect_family(recipe, 384, 256, seed_offset=13)
    pan_bg.save(out_dir / "panel_bg.png")
    pan_b.save(out_dir / "panel_border.png")

    # 2. Diamond, Circle, Hexagon
    for kind, off in (("diamond", 21), ("circle", 31), ("hexagon", 43)):
        bg, br, sl, eq = forge_radial_shape(recipe, kind, size=256, seed_offset=off)
        bg.save(out_dir / f"{kind}_bg.png")
        br.save(out_dir / f"{kind}_border.png")
        sl.save(out_dir / f"{kind}_selected.png")
        eq.save(out_dir / f"{kind}_equipped.png")

    # 3. Wheel & Misc
    for fname, img in forge_wheel_assets(recipe).items():
        img.save(out_dir / fname)
    for fname, img in forge_misc_assets(recipe).items():
        img.save(out_dir / fname)

    if clean_legacy_svgs:
        for svg in out_dir.glob("*.svg"):
            svg.unlink()

    # 4. Build schema 3 `assets` manifest block with logical-pixel `sliceSize`
    sel_tint = recipe.get("selectionTint", "selection")
    eq_tint = recipe.get("equippedTint", "equipped")

    def sliced(file: str, s: int, ssize: float, tint: Optional[str] = None) -> Dict[str, Any]:
        obj: Dict[str, Any] = {"file": file, "slice": s, "sliceSize": ssize}
        if tint:
            obj["tint"] = tint
        if outset > 0.0:
            obj["outset"] = round(outset, 3)
        return obj

    def radial_slot(file: str, tint: Optional[str] = None) -> Any:
        if not tint and outset <= 0.0:
            return file
        obj: Dict[str, Any] = {"file": file}
        if tint:
            obj["tint"] = tint
        if outset > 0.0:
            obj["outset"] = round(outset, 3)
        return obj

    assets_block: Dict[str, Any] = {
        "card": {
            "bg": sliced("card_bg.png", 54, 18.0),
            "border": sliced("card_border.png", 54, 18.0),
            "selected": sliced("card_selected.png", 54, 18.0, sel_tint),
            "equipped": sliced("card_equipped.png", 54, 18.0, eq_tint),
        },
        "square": {
            "bg": sliced("square_bg.png", 60, 20.0),
            "border": sliced("square_border.png", 60, 20.0),
            "selected": sliced("square_selected.png", 60, 20.0, sel_tint),
            "equipped": sliced("square_equipped.png", 60, 20.0, eq_tint),
        },
        "panel": {
            "bg": sliced("panel_bg.png", 60, 20.0),
            "border": sliced("panel_border.png", 60, 20.0),
        },
        "diamond": {
            "bg": radial_slot("diamond_bg.png"),
            "border": radial_slot("diamond_border.png"),
            "selected": radial_slot("diamond_selected.png", sel_tint),
            "equipped": radial_slot("diamond_equipped.png", eq_tint),
        },
        "circle": {
            "bg": radial_slot("circle_bg.png"),
            "border": radial_slot("circle_border.png"),
            "selected": radial_slot("circle_selected.png", sel_tint),
            "equipped": radial_slot("circle_equipped.png", eq_tint),
        },
        "hexagon": {
            "bg": radial_slot("hexagon_bg.png"),
            "border": radial_slot("hexagon_border.png"),
            "selected": radial_slot("hexagon_selected.png", sel_tint),
            "equipped": radial_slot("hexagon_equipped.png", eq_tint),
        },
        "wheel": {
            "ringBg": "wheel_ring_bg.png",
            "outerRim": "wheel_outer_rim.png",
            "innerRim": "wheel_inner_rim.png",
            "hubBg": "wheel_hub_bg.png",
            "hubBorder": "wheel_hub_border.png",
            "hubOverlay": "wheel_hub_overlay.png",
            "pointer": {"file": "wheel_pointer.png", "tint": "accent"},
            "divider": "wheel_spoke.png",
        },
        "divider": "rule.png",
        "markers": {
            "equipped": {"file": "marker_equipped.png", "tint": "equipped"},
            "chargePip": {"file": "marker_pip.png", "tint": "charge"},
        },
        "equipmentPanel": {
            "bg": {"file": "equip_bg.png", "slice": [24, 16, 24, 16], "sliceSize": 12.0},
            "border": {"file": "equip_border.png", "slice": [12, 16, 12, 16], "sliceSize": 8.0},
        },
    }

    manifest_path = out_dir / "theme.json"
    if update_manifest:
        if manifest_path.is_file():
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        else:
            tid = recipe.get("id", out_dir.name)
            manifest = {
                "schema": 3,
                "id": tid,
                "name": recipe.get("name", tid.replace("-", " ").title()),
                "description": recipe.get("description", "Forged with Quickslot Design Studio"),
                "frameStyle": 0,
                "palette": {
                    "text": [236, 231, 221],
                    "secondary": [188, 177, 168],
                    "muted": [132, 124, 120],
                    "accent": recipe["accentMetal"],
                    "background": [*recipe["bgColor"], int(round(recipe["bgOpacity"] * 255))],
                    "border": [*recipe["metalBase"], 225],
                    "selection": recipe["metalHi"],
                    "equipped": [224, 196, 148],
                },
                "font": "@fonts/Jost-Regular.ttf",
                "titleFont": "@fonts/LiberationSerif-Regular.ttf",
                "numberFont": "@fonts/Jost-Medium.ttf",
            }
        manifest["schema"] = 3
        if "styleOverrides" in recipe:
            manifest.setdefault("style", {}).update(recipe["styleOverrides"])
        manifest["assets"] = assets_block
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    return {"outDir": str(out_dir), "files": sorted(p.name for p in out_dir.glob("*.png"))}


def update_packs_manifest(themes_root: Path) -> Path:
    """Rebuild assets/themes/packs-manifest.json with SHA-256 checksums for all bundled art assets."""
    import hashlib

    entries = []
    for theme_dir in sorted(p for p in themes_root.iterdir() if p.is_dir() and (p / "theme.json").is_file()):
        for asset in sorted(theme_dir.iterdir()):
            if asset.suffix.lower() not in (".png", ".svg"):
                continue
            digest = hashlib.sha256(asset.read_bytes()).hexdigest()
            entries.append({
                "theme": theme_dir.name,
                "asset": f"{theme_dir.name}/{asset.name}",
                "sha256": digest,
            })
    manifest = {
        "origin": "Quickslot original clean-room 2.5D PBR theme artwork forged with Quickslot Design Studio.",
        "assets": entries,
    }
    out_path = themes_root / "packs-manifest.json"
    out_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return out_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Quickslot Design Studio PBR Frame Forge CLI")
    parser.add_argument("--all", action="store_true", help="Forge all 16 bundled themes into assets/themes/")
    parser.add_argument("--flagships", action="store_true", help="Forge the 5 flagship themes into assets/themes/")
    parser.add_argument("--theme", help="Bundled theme id to forge (e.g. oathvein, nordic, edge, untarnished, ...)")
    parser.add_argument("--recipe", type=Path, help="Custom JSON recipe file")
    parser.add_argument("--out", type=Path, help="Output theme directory")
    parser.add_argument("--clean-svgs", action="store_true", help="Remove old .svg frame files from output directory")
    parser.add_argument("--update-packs-manifest", action="store_true", help="Regenerate assets/themes/packs-manifest.json")
    parser.add_argument("--list-materials", action="store_true", help="List built-in PBR material presets")
    args = parser.parse_args()

    if args.list_materials:
        for k, v in MATERIALS.items():
            print(f"  {k:20s}  {v['label']}")
        return 0

    if args.all or args.flagships:
        batch = ALL_THEME_RECIPES if args.all else FLAGSHIP_RECIPES
        for tid, rec in sorted(batch.items()):
            dest = (args.out / tid) if args.out else (REPO_ROOT / "assets" / "themes" / tid)
            res = forge_theme_pack(rec, dest, update_manifest=True, clean_legacy_svgs=args.clean_svgs)
            print(f"Forged {tid:20s}: {len(res['files'])} PNGs -> {dest}")
        if args.update_packs_manifest or (args.all and not args.out):
            m = update_packs_manifest(args.out or (REPO_ROOT / "assets" / "themes"))
            print(f"Updated manifest -> {m}")
        return 0

    if args.recipe:
        rec = json.loads(args.recipe.read_text(encoding="utf-8"))
        dest = args.out or (REPO_ROOT / "assets" / "themes" / rec.get("id", args.recipe.stem))
        res = forge_theme_pack(rec, dest, update_manifest=True, clean_legacy_svgs=args.clean_svgs)
        print(f"Forged recipe {args.recipe.name}: {len(res['files'])} PNGs -> {dest}")
        return 0

    if args.theme:
        if args.theme not in ALL_THEME_RECIPES:
            print(f"Unknown theme '{args.theme}'. Choices: {', '.join(sorted(ALL_THEME_RECIPES))}", file=sys.stderr)
            return 2
        dest = args.out or (REPO_ROOT / "assets" / "themes" / args.theme)
        res = forge_theme_pack(ALL_THEME_RECIPES[args.theme], dest, update_manifest=True, clean_legacy_svgs=args.clean_svgs)
        print(f"Forged {args.theme}: {len(res['files'])} PNGs -> {dest}")
        if args.update_packs_manifest:
            update_packs_manifest(REPO_ROOT / "assets" / "themes")
        return 0

    if args.update_packs_manifest:
        m = update_packs_manifest(args.out or (REPO_ROOT / "assets" / "themes"))
        print(f"Updated manifest -> {m}")
        return 0

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
