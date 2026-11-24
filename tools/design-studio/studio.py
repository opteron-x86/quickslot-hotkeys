#!/usr/bin/env python3
"""Quickslot Design Studio — Local Interactive Web UI Server.

Zero external web dependencies (Python stdlib HTTP server + NumPy/Pillow/Cairo PBR forge).
Launch with:
  python3 tools/design-studio/studio.py
Then open http://127.0.0.1:8765 in any browser to tweak materials, rim thickness,
ornaments, custom PNG/AI stamps, preview over Skyrim-like backdrops, and export
directly into assets/themes/<id>/ or Skyrim's Data/Quickslot/themes/<id>/.
"""

from __future__ import annotations
import argparse
import base64
import io
import json
import sys
import tempfile
import webbrowser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict

from PIL import Image

STUDIO_DIR = Path(__file__).resolve().parent
WEB_DIR = STUDIO_DIR / "web"
if str(STUDIO_DIR) not in sys.path:
    sys.path.insert(0, str(STUDIO_DIR))

from forge import (
    REPO_ROOT,
    forge_radial_shape,
    forge_rect_family,
    forge_theme_pack,
    forge_wheel_assets,
)
from materials import ALL_THEME_RECIPES, MATERIALS, resolve_recipe
from stamps import import_stamp

STAMP_DIR = Path(tempfile.mkdtemp(prefix="quickslot_design_studio_stamps_"))

ORNAMENT_STYLES = [
    {"id": "oathvein_clasp", "label": "Oathvein Gothic Clasps"},
    {"id": "nordic_knot", "label": "Nordic Interlace Knotwork"},
    {"id": "edge_minimal", "label": "Edge Minimal Hairline"},
    {"id": "dwemer_rivet", "label": "Dwemer Rivets & Aetherium"},
    {"id": "daedric_horn", "label": "Daedric Talons & Ember"},
    {"id": "filigree_scroll", "label": "Imperial / Filigree Scroll"},
    {"id": "clean", "label": "Clean Forged Bevel"},
]


def _img_to_data_url(img: Image.Image) -> str:
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode("ascii")


def _tint_overlay(overlay: Image.Image, rgb: list[int], alpha_scale: float = 1.0) -> Image.Image:
    import numpy as np

    arr = np.asarray(overlay.convert("RGBA"), dtype=np.float32) / 255.0
    tint = np.array(rgb[:3], dtype=np.float32) / 255.0
    arr[:, :, :3] *= tint[None, None, :]
    arr[:, :, 3] = np.clip(arr[:, :, 3] * alpha_scale, 0.0, 1.0)
    return Image.fromarray(np.clip(arr * 255.0, 0, 255).astype(np.uint8), mode="RGBA")


def _compose_preview_layers(
    bg: Image.Image,
    border: Image.Image,
    sel: Image.Image,
    eq: Image.Image,
    state: str,
    accent: list[int],
    equipped_col: list[int],
) -> Image.Image:
    canvas = Image.new("RGBA", bg.size, (0, 0, 0, 0))
    canvas.alpha_composite(bg)
    canvas.alpha_composite(border)
    if state == "equipped":
        canvas.alpha_composite(_tint_overlay(eq, equipped_col, 0.95))
    elif state == "selected":
        canvas.alpha_composite(_tint_overlay(sel, accent, 1.0))
    return canvas


def render_live_preview(payload: Dict[str, Any]) -> Dict[str, str]:
    recipe = resolve_recipe(payload.get("recipe", {}))
    state = payload.get("state", "normal")
    accent = recipe.get("accentMetal", [206, 178, 114])
    equipped_col = recipe.get("metalHi", [236, 212, 168])

    sq = _compose_preview_layers(*forge_rect_family(recipe, 256, 256, seed_offset=1), state, accent, equipped_col)
    sq_norm = _compose_preview_layers(*forge_rect_family(recipe, 256, 256, seed_offset=2), "normal", accent, equipped_col)
    sq_sel = _compose_preview_layers(*forge_rect_family(recipe, 256, 256, seed_offset=3), "selected", accent, equipped_col)
    sq_eq = _compose_preview_layers(*forge_rect_family(recipe, 256, 256, seed_offset=4), "equipped", accent, equipped_col)

    card = _compose_preview_layers(*forge_rect_family(recipe, 384, 144, seed_offset=7), state, accent, equipped_col)
    pan_bg, pan_b, _, _ = forge_rect_family(recipe, 384, 256, seed_offset=13)
    panel = Image.new("RGBA", pan_bg.size, (0, 0, 0, 0))
    panel.alpha_composite(pan_bg)
    panel.alpha_composite(pan_b)

    dia = _compose_preview_layers(*forge_radial_shape(recipe, "diamond", 256, seed_offset=21), state, accent, equipped_col)
    cir = _compose_preview_layers(*forge_radial_shape(recipe, "circle", 256, seed_offset=31), state, accent, equipped_col)
    hx = _compose_preview_layers(*forge_radial_shape(recipe, "hexagon", 256, seed_offset=43), state, accent, equipped_col)

    # Compact wheel rim preview
    wheel = forge_wheel_assets(recipe)
    w_comp = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    w_comp.alpha_composite(wheel["wheel_outer_rim.png"].resize((256, 256), Image.Resampling.LANCZOS))
    ir = wheel["wheel_inner_rim.png"].resize((136, 136), Image.Resampling.LANCZOS)
    w_comp.alpha_composite(ir, (60, 60))
    hb = wheel["wheel_hub_bg.png"].resize((120, 120), Image.Resampling.LANCZOS)
    hbr = wheel["wheel_hub_border.png"].resize((120, 120), Image.Resampling.LANCZOS)
    w_comp.alpha_composite(hb, (68, 68))
    w_comp.alpha_composite(hbr, (68, 68))

    return {
        "square": _img_to_data_url(sq),
        "squareNormal": _img_to_data_url(sq_norm),
        "squareSelected": _img_to_data_url(sq_sel),
        "squareEquipped": _img_to_data_url(sq_eq),
        "card": _img_to_data_url(card),
        "panel": _img_to_data_url(panel),
        "diamond": _img_to_data_url(dia),
        "circle": _img_to_data_url(cir),
        "hexagon": _img_to_data_url(hx),
        "wheel": _img_to_data_url(w_comp),
    }


class StudioHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args: Any) -> None:
        return

    def _send_json(self, obj: Any, status: int = HTTPStatus.OK) -> None:
        raw = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def _read_json(self) -> Dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > 16 * 1024 * 1024:
            return {}
        return json.loads(self.rfile.read(length).decode("utf-8"))

    def do_GET(self) -> None:
        if self.path in ("/", "/index.html"):
            html = (WEB_DIR / "index.html").read_bytes()
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(html)))
            self.end_headers()
            self.wfile.write(html)
            return

        if self.path == "/api/presets":
            themes_root = REPO_ROOT / "assets" / "themes"
            theme_ids = sorted(p.name for p in themes_root.iterdir() if p.is_dir() and (p / "theme.json").is_file())
            self._send_json({
                "materials": MATERIALS,
                "flagships": {k: resolve_recipe(v) for k, v in sorted(ALL_THEME_RECIPES.items())},
                "ornaments": ORNAMENT_STYLES,
                "themes": theme_ids,
                "defaultExportRoot": str(themes_root),
            })
            return

        self.send_error(HTTPStatus.NOT_FOUND)

    def do_POST(self) -> None:
        try:
            payload = self._read_json()
            if self.path == "/api/preview":
                self._send_json(render_live_preview(payload))
                return

            if self.path == "/api/upload-stamp":
                b64 = payload.get("dataUrl", "")
                if "," in b64:
                    b64 = b64.split(",", 1)[1]
                raw = base64.b64decode(b64)
                in_path = STAMP_DIR / "uploaded_stamp.png"
                in_path.write_bytes(raw)
                sym = payload.get("symmetry", "quad")
                bg_mode = payload.get("bgMode", "auto")
                cleaned, _ = import_stamp(in_path, 256, 256, symmetry=sym, bg_mode=bg_mode, clear_center_ratio=0.72)
                cleaned_path = STAMP_DIR / "cleaned_stamp.png"
                cleaned.save(cleaned_path)
                self._send_json({
                    "stampPath": str(cleaned_path),
                    "preview": _img_to_data_url(cleaned),
                })
                return

            if self.path == "/api/export":
                recipe = payload.get("recipe", {})
                tid = str(recipe.get("id", "custom-theme")).strip() or "custom-theme"
                export_root = Path(str(payload.get("exportRoot") or (REPO_ROOT / "assets" / "themes"))).expanduser()
                clean_svgs = bool(payload.get("cleanSvgs", False))
                dest = export_root / tid
                res = forge_theme_pack(recipe, dest, update_manifest=True, clean_legacy_svgs=clean_svgs)
                self._send_json({"ok": True, **res})
                return

            self.send_error(HTTPStatus.NOT_FOUND)
        except Exception as exc:
            self._send_json({"ok": False, "error": str(exc)}, status=HTTPStatus.BAD_REQUEST)


def main() -> int:
    parser = argparse.ArgumentParser(description="Quickslot Design Studio Web UI")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--open", action="store_true", help="Open browser automatically")
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), StudioHandler)
    url = f"http://{args.host}:{args.port}"
    print(f"Quickslot Design Studio running at {url}  (Ctrl+C to stop)")
    if args.open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nDesign Studio stopped.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
