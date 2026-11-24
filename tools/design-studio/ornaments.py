"""Sculpted heightmap & accent/emissive ornament generators using Cairo + NumPy.

Every ornament returns `(height_add, accent_mask, emissive_mask)` float32 arrays in [0, 1]
that stamp directly into the PBR heightfield so corner clasps, knotwork, rivets, and
filigree receive genuine directional bevel lighting, specular catch-lights, and crevice AO.
"""

from __future__ import annotations
import math
from typing import Tuple
import cairo
import numpy as np


def _make_surface(w: int, h: int) -> Tuple[cairo.ImageSurface, cairo.Context]:
    surf = cairo.ImageSurface(cairo.FORMAT_A8, w, h)
    ctx = cairo.Context(surf)
    ctx.set_antialias(cairo.ANTIALIAS_BEST)
    return surf, ctx


def _surf_to_float(surf: cairo.ImageSurface) -> np.ndarray:
    surf.flush()
    w, h = surf.get_width(), surf.get_height()
    stride = surf.get_stride()
    buf = np.frombuffer(surf.get_data(), dtype=np.uint8).reshape((h, stride))
    return (buf[:, :w].astype(np.float32)) / 255.0


def _draw_quad_sym(ctx: cairo.Context, w: int, h: int, fn) -> None:
    """Invoke `fn(ctx)` in all 4 quadrant mirror reflections around the canvas center."""
    cx, cy = w * 0.5, h * 0.5
    for sx in (1.0, -1.0):
        for sy in (1.0, -1.0):
            ctx.save()
            ctx.translate(cx, cy)
            ctx.scale(sx, sy)
            ctx.translate(-cx, -cy)
            fn(ctx)
            ctx.restore()


def rect_ornaments(
    style: str,
    w: int,
    h: int,
    pad: float,
    cut: float,
    rim_w: float,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Generate (height_add, accent_mask, emissive_mask) for rectangular/square/panel frames."""
    sh, ch = _make_surface(w, h)
    sa, ca = _make_surface(w, h)
    se, ce = _make_surface(w, h)

    x0, y0 = pad, pad
    c = cut
    rw = rim_w

    if style == "oathvein_clasp":
        # Gothic forged beveled corner clasps hugging the chamfer + subtle mid-edge diamond studs.
        def clasp(ctx: cairo.Context, lw: float) -> None:
            ctx.set_source_rgba(1, 1, 1, 1)
            # Tight chamfer sleeve
            ctx.move_to(x0 + c + rw * 1.4, y0 - rw * 0.12)
            ctx.line_to(x0 + c - rw * 0.1, y0 - rw * 0.12)
            ctx.line_to(x0 - rw * 0.12, y0 + c - rw * 0.1)
            ctx.line_to(x0 - rw * 0.12, y0 + c + rw * 1.4)
            ctx.line_to(x0 + rw * 0.85, y0 + c + rw * 0.95)
            ctx.line_to(x0 + rw * 0.85, y0 + c + rw * 0.15)
            ctx.line_to(x0 + c + rw * 0.15, y0 + rw * 0.85)
            ctx.line_to(x0 + c + rw * 0.95, y0 + rw * 0.85)
            ctx.close_path()
            ctx.fill()
            # Delicate inner diagonal specular bevel line across the miter
            ctx.set_line_width(max(1.2, lw))
            ctx.move_to(x0 + c + rw * 0.45, y0 + rw * 1.35)
            ctx.line_to(x0 + rw * 1.35, y0 + c + rw * 0.45)
            ctx.stroke()

        _draw_quad_sym(ch, w, h, lambda ctx: clasp(ctx, rw * 0.24))
        _draw_quad_sym(ca, w, h, lambda ctx: clasp(ctx, rw * 0.20))

        # Subtle cardinal micro-rivet diamond at top/bottom/left/right midpoints
        for mx, my in ((w * 0.5, y0 + rw * 0.46), (w * 0.5, h - y0 - rw * 0.46)):
            r = rw * 0.42
            ch.set_source_rgba(1, 1, 1, 0.85)
            ch.move_to(mx, my - r)
            ch.line_to(mx + r * 1.4, my)
            ch.line_to(mx, my + r)
            ch.line_to(mx - r * 1.4, my)
            ch.close_path()
            ch.fill()

    elif style == "nordic_knot":
        # Authentic stepped Nordic key/knotwork L-brackets at corners (inspired by Nordic UI corner motifs).
        def knot(ctx: cairo.Context, alpha: float) -> None:
            ctx.set_source_rgba(1, 1, 1, alpha)
            ctx.set_line_cap(cairo.LINE_CAP_SQUARE)
            ctx.set_line_join(cairo.LINE_JOIN_MITER)
            lw = max(1.6, rw * 0.30)
            ctx.set_line_width(lw)
            # Outer L-elbow wrapping the corner cleanly inside the rim
            u = rw * 0.82
            ox = x0 + rw * 0.28
            oy = y0 + rw * 0.28
            # Stepped Nordic fretwork spiral in the corner
            ctx.move_to(ox + c + u * 2.5, oy + u * 0.25)
            ctx.line_to(ox + c * 0.72, oy + u * 0.25)
            ctx.line_to(ox + u * 0.25, oy + c * 0.72)
            ctx.line_to(ox + u * 0.25, oy + c + u * 2.5)
            ctx.stroke()
            # Interlocking inner square hook
            ctx.set_line_width(max(1.2, rw * 0.22))
            ctx.move_to(ox + c + u * 1.9, oy + u * 0.95)
            ctx.line_to(ox + c * 0.95, oy + u * 0.95)
            ctx.line_to(ox + c * 0.95, oy + u * 1.65)
            ctx.line_to(ox + c + u * 0.75, oy + u * 1.65)
            ctx.stroke()
            ctx.move_to(ox + u * 0.95, oy + c + u * 1.9)
            ctx.line_to(ox + u * 0.95, oy + c * 0.95)
            ctx.line_to(ox + u * 1.65, oy + c * 0.95)
            ctx.line_to(ox + u * 1.65, oy + c + u * 0.75)
            ctx.stroke()

        _draw_quad_sym(ch, w, h, lambda ctx: knot(ctx, 1.0))
        _draw_quad_sym(ca, w, h, lambda ctx: knot(ctx, 0.85))

    elif style == "edge_minimal":
        # Ultra-clean architectural hairline corner ticks and centered gold micro-pip.
        def tick(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.85)
            ctx.set_line_width(max(1.2, rw * 0.28))
            ctx.move_to(x0 + c + rw * 1.8, y0 + rw * 0.42)
            ctx.line_to(x0 + c + rw * 0.1, y0 + rw * 0.42)
            ctx.line_to(x0 + rw * 0.42, y0 + c + rw * 0.1)
            ctx.line_to(x0 + rw * 0.42, y0 + c + rw * 1.8)
            ctx.stroke()

        _draw_quad_sym(ch, w, h, tick)
        _draw_quad_sym(ca, w, h, tick)

    elif style == "dwemer_rivet":
        # Stepped bronze corner gusset plates with domed brass rivets & subtle glowing aetherium inlay dot.
        def gusset(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.95)
            ctx.move_to(x0 + c + rw * 2.1, y0)
            ctx.line_to(x0 + c, y0)
            ctx.line_to(x0, y0 + c)
            ctx.line_to(x0, y0 + c + rw * 2.1)
            ctx.line_to(x0 + rw * 1.08, y0 + c + rw * 1.75)
            ctx.line_to(x0 + rw * 1.08, y0 + c + rw * 0.22)
            ctx.line_to(x0 + c + rw * 0.22, y0 + rw * 1.08)
            ctx.line_to(x0 + c + rw * 1.75, y0 + rw * 1.08)
            ctx.close_path()
            ctx.fill()

        def rivets(ctx: cairo.Context, rad: float) -> None:
            ctx.set_source_rgba(1, 1, 1, 1.0)
            pts = [
                (x0 + c + rw * 1.35, y0 + rw * 0.50),
                (x0 + rw * 0.50, y0 + c + rw * 1.35),
            ]
            for px, py in pts:
                ctx.arc(px, py, rad, 0, math.tau)
                ctx.fill()

        def aether_dot(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.95)
            mx = x0 + c * 0.46 + rw * 0.36
            my = y0 + c * 0.46 + rw * 0.36
            r = max(1.4, rw * 0.26)
            ctx.move_to(mx, my - r)
            ctx.line_to(mx + r, my)
            ctx.line_to(mx, my + r)
            ctx.line_to(mx - r, my)
            ctx.close_path()
            ctx.fill()

        _draw_quad_sym(ch, w, h, gusset)
        _draw_quad_sym(ch, w, h, lambda ctx: rivets(ctx, max(1.6, rw * 0.30)))
        _draw_quad_sym(ca, w, h, gusset)
        _draw_quad_sym(ce, w, h, aether_dot)

    elif style == "daedric_horn":
        # Angular forged obsidian corner talons + glowing inner molten ember veins along chamfer.
        def talon(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 1.0)
            # Subtle angular beak poking cleanly to corner without looking cartoony
            ctx.move_to(x0 + c + rw * 1.9, y0 - rw * 0.08)
            ctx.line_to(x0 + c * 0.45, y0 - rw * 0.25)
            ctx.line_to(x0 - rw * 0.30, y0 - rw * 0.30)
            ctx.line_to(x0 - rw * 0.25, y0 + c * 0.45)
            ctx.line_to(x0 - rw * 0.08, y0 + c + rw * 1.9)
            ctx.line_to(x0 + rw * 0.95, y0 + c + rw * 1.2)
            ctx.line_to(x0 + rw * 0.95, y0 + c * 0.65)
            ctx.line_to(x0 + c * 0.65, y0 + rw * 0.95)
            ctx.line_to(x0 + c + rw * 1.2, y0 + rw * 0.95)
            ctx.close_path()
            ctx.fill()

        def ember_vein(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.92)
            ctx.set_line_width(max(1.2, rw * 0.18))
            ctx.move_to(x0 + c + rw * 1.5, y0 + rw * 0.52)
            ctx.line_to(x0 + c * 0.72 + rw * 0.18, y0 + rw * 0.52)
            ctx.line_to(x0 + rw * 0.52, y0 + c * 0.72 + rw * 0.18)
            ctx.line_to(x0 + rw * 0.52, y0 + c + rw * 1.5)
            ctx.stroke()

        _draw_quad_sym(ch, w, h, talon)
        _draw_quad_sym(ca, w, h, talon)
        _draw_quad_sym(ce, w, h, ember_vein)

    elif style == "filigree_scroll":
        # Graceful symmetrical forged corner curl + central diamond clasp.
        def curl(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.92)
            ctx.set_line_width(max(1.4, rw * 0.26))
            ctx.set_line_cap(cairo.LINE_CAP_ROUND)
            ctx.move_to(x0 + c + rw * 2.2, y0 + rw * 0.46)
            ctx.curve_to(
                x0 + c + rw * 0.6, y0 + rw * 0.46,
                x0 + c * 0.7, y0 + c * 0.7,
                x0 + rw * 0.46, y0 + c + rw * 2.2,
            )
            ctx.stroke()
            # Delicate inner diamond pip on the chamfer
            mx, my = x0 + c * 0.52 + rw * 0.42, y0 + c * 0.52 + rw * 0.42
            r = rw * 0.34
            ctx.move_to(mx, my - r)
            ctx.line_to(mx + r, my)
            ctx.line_to(mx, my + r)
            ctx.line_to(mx - r, my)
            ctx.close_path()
            ctx.fill()

        _draw_quad_sym(ch, w, h, curl)
        _draw_quad_sym(ca, w, h, curl)

    return _surf_to_float(sh), _surf_to_float(sa), _surf_to_float(se)


def radial_ornaments(
    style: str,
    size: int,
    radius: float,
    rim_w: float,
    n_fold: int = 8,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Generate sculpted ornaments around circular, diamond, hexagon, or radial wheel rims."""
    sh, ch = _make_surface(size, size)
    sa, ca = _make_surface(size, size)
    se, ce = _make_surface(size, size)
    cx = cy = size * 0.5

    def repeat(ctx: cairo.Context, draw_fn) -> None:
        for k in range(n_fold):
            ang = -math.pi * 0.5 + k * (math.tau / n_fold)
            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(ang)
            draw_fn(ctx)
            ctx.restore()

    if style == "oathvein_clasp":
        def stud(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.95)
            r0 = radius - rim_w * 0.5
            s = rim_w * 0.55
            ctx.move_to(0, -r0 - s * 1.1)
            ctx.line_to(s * 0.9, -r0)
            ctx.line_to(0, -r0 + s * 1.1)
            ctx.line_to(-s * 0.9, -r0)
            ctx.close_path()
            ctx.fill()

        repeat(ch, stud)
        repeat(ca, stud)

    elif style == "nordic_knot":
        def rune_bar(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.92)
            r0 = radius - rim_w * 0.5
            ctx.set_line_width(max(1.4, rim_w * 0.24))
            s = rim_w * 0.62
            ctx.move_to(-s * 1.3, -r0 - s * 0.35)
            ctx.line_to(0, -r0 + s * 0.45)
            ctx.line_to(s * 1.3, -r0 - s * 0.35)
            ctx.stroke()

        repeat(ch, rune_bar)
        repeat(ca, rune_bar)

    elif style == "dwemer_rivet":
        def rivet(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 1.0)
            r0 = radius - rim_w * 0.5
            ctx.arc(0, -r0, max(1.6, rim_w * 0.34), 0, math.tau)
            ctx.fill()

        def notch(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.85)
            r0 = radius - rim_w * 0.5
            ctx.arc(0, -r0, max(1.1, rim_w * 0.18), 0, math.tau)
            ctx.fill()

        repeat(ch, rivet)
        repeat(ca, rivet)
        repeat(ce, notch)

    elif style == "daedric_horn":
        def barb(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 1.0)
            r0 = radius - rim_w * 0.35
            s = rim_w * 0.72
            ctx.move_to(0, -r0 - s * 1.25)
            ctx.line_to(s * 0.75, -r0 + s * 0.45)
            ctx.line_to(0, -r0 + s * 0.12)
            ctx.line_to(-s * 0.75, -r0 + s * 0.45)
            ctx.close_path()
            ctx.fill()

        def glow_pip(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.92)
            r0 = radius - rim_w * 0.52
            s = rim_w * 0.28
            ctx.move_to(0, -r0 - s)
            ctx.line_to(s * 0.7, -r0)
            ctx.line_to(0, -r0 + s)
            ctx.line_to(-s * 0.7, -r0)
            ctx.close_path()
            ctx.fill()

        repeat(ch, barb)
        repeat(ca, barb)
        repeat(ce, glow_pip)

    elif style in ("edge_minimal", "filigree_scroll"):
        def pip(ctx: cairo.Context) -> None:
            ctx.set_source_rgba(1, 1, 1, 0.85)
            r0 = radius - rim_w * 0.5
            s = rim_w * (0.32 if style == "edge_minimal" else 0.48)
            ctx.move_to(0, -r0 - s)
            ctx.line_to(s * 0.8, -r0)
            ctx.line_to(0, -r0 + s)
            ctx.line_to(-s * 0.8, -r0)
            ctx.close_path()
            ctx.fill()

        repeat(ch, pip)
        repeat(ca, pip)

    return _surf_to_float(sh), _surf_to_float(sa), _surf_to_float(se)
