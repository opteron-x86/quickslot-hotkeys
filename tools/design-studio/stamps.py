"""PNG / AI Ornament Stamp Importer for Quickslot Design Studio.

Takes any external or AI-generated PNG ornament, crest, knotwork, or frame texture and:
  1. Extracts clean alpha (from existing alpha, black-screen luminance matting, or chroma green).
  2. Enforces mathematical 4-way (quad), 8-way (oct), or radial N-fold symmetry.
  3. Extracts a normalized heightmap relief channel so Design Studio can shade the stamp
     using the active theme's forged metal PBR shader.
"""

from __future__ import annotations
from pathlib import Path
from typing import Tuple
import numpy as np
from PIL import Image, ImageFilter


def extract_alpha(img: Image.Image, bg_mode: str = "auto") -> Image.Image:
    rgba = img.convert("RGBA")
    arr = np.asarray(rgba, dtype=np.float32) / 255.0
    r, g, b, a = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2], arr[:, :, 3]

    if bg_mode == "auto":
        # If the image already has meaningful transparency, preserve it; otherwise inspect corners.
        if np.min(a) < 0.05 and np.mean(a < 0.5) > 0.05:
            return rgba
        corners = np.concatenate([
            arr[:8, :8, :3].reshape(-1, 3),
            arr[:8, -8:, :3].reshape(-1, 3),
            arr[-8:, :8, :3].reshape(-1, 3),
            arr[-8:, -8:, :3].reshape(-1, 3),
        ], axis=0)
        mean_c = corners.mean(axis=0)
        if mean_c[1] > 0.6 and mean_c[0] < 0.35 and mean_c[2] < 0.35:
            bg_mode = "chroma"
        elif np.max(mean_c) < 0.16:
            bg_mode = "black"
        else:
            bg_mode = "luma_key"

    if bg_mode == "black":
        # Smooth luminance matting for assets rendered on pure black
        val = np.maximum(np.maximum(r, g), b)
        new_a = np.clip((val - 0.04) / 0.24, 0.0, 1.0)
        # De-multiply subtle black fringe
        safe = np.maximum(new_a, 0.08)
        arr[:, :, :3] = np.clip(arr[:, :, :3] / safe[:, :, None], 0.0, 1.0)
        arr[:, :, 3] = new_a
    elif bg_mode == "chroma":
        # Green-screen spill suppression + crisp matte
        greenness = g - np.maximum(r, b)
        new_a = np.clip(1.0 - (greenness - 0.08) / 0.22, 0.0, 1.0)
        arr[:, :, 1] = np.minimum(g, np.maximum(r, b) + 0.04)
        arr[:, :, 3] = new_a
    elif bg_mode == "luma_key":
        # Key out the average corner color with smooth Euclidean tolerance
        corners = np.concatenate([
            arr[:8, :8, :3].reshape(-1, 3),
            arr[-8:, -8:, :3].reshape(-1, 3),
        ], axis=0)
        key = corners.mean(axis=0)
        dist = np.linalg.norm(arr[:, :, :3] - key[None, None, :], axis=2)
        arr[:, :, 3] = np.clip((dist - 0.06) / 0.18, 0.0, 1.0)

    out = np.clip(arr * 255.0, 0, 255).astype(np.uint8)
    return Image.fromarray(out, mode="RGBA")


def enforce_symmetry(img: Image.Image, symmetry: str = "quad") -> Image.Image:
    """Enforce exact geometric symmetry from the cleanest top-left quadrant/octant."""
    rgba = img.convert("RGBA")
    arr = np.asarray(rgba, dtype=np.uint8).copy()
    h, w, _ = arr.shape
    hh, hw = h // 2, w // 2

    if symmetry in ("quad", "oct"):
        # Take top-left quadrant and mirror cleanly across both axes
        tl = arr[:hh, :hw].copy()
        if symmetry == "oct" and hh == hw:
            # Mirror across main diagonal so 45-degree corners are identical
            tri = np.triu(np.ones((hh, hw), dtype=bool))
            tl_t = np.transpose(tl, (1, 0, 2))
            tl = np.where(tri[:, :, None], tl, tl_t)
        top = np.concatenate([tl, tl[:, ::-1]], axis=1)
        full = np.concatenate([top, top[::-1, :]], axis=0)
        if full.shape[0] != h or full.shape[1] != w:
            return Image.fromarray(full, mode="RGBA").resize((w, h), Image.Resampling.LANCZOS)
        return Image.fromarray(full, mode="RGBA")

    if symmetry.startswith("radial"):
        try:
            folds = max(2, int(symmetry.replace("radial", "") or "8"))
        except ValueError:
            folds = 8
        acc = np.zeros((h, w, 4), dtype=np.float32)
        for k in range(folds):
            rot = rgba.rotate(k * (360.0 / folds), resample=Image.Resampling.BICUBIC)
            acc += np.asarray(rot, dtype=np.float32)
        acc /= float(folds)
        return Image.fromarray(np.clip(acc, 0, 255).astype(np.uint8), mode="RGBA")

    return rgba


def import_stamp(
    path: Path | str,
    target_w: int,
    target_h: int,
    symmetry: str = "quad",
    bg_mode: str = "auto",
    clear_center_ratio: float = 0.0,
) -> Tuple[Image.Image, np.ndarray]:
    """Load an external/AI PNG, clean its alpha, symmetrize it, and return (rgba_image, height_0_to_1)."""
    raw = Image.open(path)
    clean = extract_alpha(raw, bg_mode=bg_mode)
    clean = clean.resize((target_w, target_h), Image.Resampling.LANCZOS)
    sym = enforce_symmetry(clean, symmetry=symmetry)

    arr = np.asarray(sym, dtype=np.float32) / 255.0
    if clear_center_ratio > 0.0:
        yy, xx = np.mgrid[0:target_h, 0:target_w].astype(np.float32)
        nx = (xx - target_w * 0.5) / (target_w * 0.5)
        ny = (yy - target_h * 0.5) / (target_h * 0.5)
        rad = np.maximum(np.abs(nx), np.abs(ny))
        cut = np.clip((rad - clear_center_ratio) / 0.08, 0.0, 1.0)
        arr[:, :, 3] *= cut
        sym = Image.fromarray(np.clip(arr * 255.0, 0, 255).astype(np.uint8), mode="RGBA")

    # Heightmap from alpha-weighted perceptual luminance, slightly softened for smooth Sobel bevels
    luma = 0.299 * arr[:, :, 0] + 0.587 * arr[:, :, 1] + 0.114 * arr[:, :, 2]
    height = np.clip((0.35 + 0.65 * luma) * arr[:, :, 3], 0.0, 1.0)
    soft = Image.fromarray((height * 255.0).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius=0.8))
    return sym, np.asarray(soft, dtype=np.float32) / 255.0
