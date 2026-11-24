# Quickslot Design Studio

Quickslot Design Studio synthesizes physically shaded 2.5D PBR RGBA PNG theme packs for Quickslot (`schema: 3`) and provides a local browser UI for live material, ornament, and stamp authoring.

## Requirements

- Python 3.10+
- `numpy`, `Pillow`, and `pycairo` (`cairo`)

## Interactive Web UI

Start the local studio server:

```sh
python3 tools/design-studio/studio.py --open
```

Design Studio opens at `http://127.0.0.1:8765` and lets you:

- Load any of the 16 bundled theme recipes or built-in material presets (`oxblood_iron`, `nordic_steel`, `edge_silver`, `dwemer_bronze`, `daedric_obsidian`, `amethyst_crystal`, `crimson_gold`, `dear_diary_bronze`, `horizons_slate`, `immortal_bronze`, `imperial_brass`, `norden_frost`, `outlander_brass`, `astral_silver`, `untarnished_graphite`, `veldun_olive`).
- Adjust logical rim thickness (`rimPx`), corner chamfer, bevel relief, surface grain, edge wear, patina depth, emissive inlay, background opacity, and material colors.
- Choose a sculpted ornament style (`oathvein_clasp`, `nordic_knot`, `edge_minimal`, `dwemer_rivet`, `daedric_horn`, `filigree_scroll`, `clean`) or upload any custom PNG ornament stamp with automatic alpha extraction and 4-way, 8-way, or radial symmetry.
- Preview `Souls Cross`, compact `List` panels, `Square`, `Diamond`, `Circle`, `Hexagon`, `Card`, and `Wheel` artwork across **Dark Forest**, **Bright Snow**, and **Dungeon** backdrops in **Normal**, **Equipped**, and **Selected** states, with zoom controls (`75%`–`300%`, `1x`/`1.5x`/`2x`/`Fit`, and `Ctrl` + mouse wheel) for 4K displays.
- Export the complete PNG pack and `theme.json` straight into `assets/themes/<id>/` or Skyrim's `Data/Quickslot/themes/<id>/` for instant hot-reload with **Auto-reload theme changes**.

## Command-Line Forging

```sh
# List built-in material presets
python3 tools/design-studio/forge.py --list-materials

# Forge a single bundled theme
python3 tools/design-studio/forge.py --theme oathvein --out assets/themes/oathvein

# Forge all 16 bundled themes and refresh assets/themes/packs-manifest.json
python3 tools/design-studio/forge.py --all --clean-svgs

# Forge a custom saved recipe JSON
python3 tools/design-studio/forge.py --recipe my-theme.recipe.json --out Data/Quickslot/themes/my-theme
```

## Hand-Painted PNG Reskinning

Design Studio is optional for theme authors. Any theme folder under `Data/Quickslot/themes/<your-theme>/` accepts standard RGBA `.png` or `.svg` files from Photoshop, Krita, GIMP, Affinity, or Blender. Use `slice` with `sliceSize` in `theme.json` so nine-sliced borders keep a constant logical thickness across tiles and panels. See [`THEMES.md`](../../THEMES.md) for the complete `schema: 3` specification.
