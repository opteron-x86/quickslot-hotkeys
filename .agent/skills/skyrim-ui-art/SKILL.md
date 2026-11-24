---
name: skyrim-ui-art
description: Dark-fantasy Skyrim HUD visual craft rules, 2.5D PBR theme forging via Quickslot Design Studio, PNG/AI ornament stamp processing, and real-backdrop visual inspection workflow. Use whenever authoring, tuning, or reviewing Quickslot HUD themes, borders, selectors, or widget art.
---

# Skyrim UI Art Craft & Quickslot Design Studio

Use this skill whenever designing, forging, or reviewing HUD frame artwork for Quickslot.

## Visual Craft Rules (Dark-Fantasy Skyrim HUD)

1. **Logical-Pixel Rim Discipline**:
   - Keep metallic frame rims delicate: **2.6–4.0 logical HUD pixels** thick (`rimPx` in Design Studio).
   - Always set `"sliceSize"` (logical pixels, e.g., `18`–`20`) alongside `"slice"` (`54`–`60` source px at `3×` density) on nine-sliced PNGs (`square`, `card`, `panel`) so a wide `268×160` List panel keeps the exact same crisp rim thickness as a `78×78` Square tile.
2. **Tactile Materiality Over Flat Vectors**:
   - Every metal/stone border must sandwich a dark-valued, grained metallic channel between razor-thin **1px dark outer and inner etched contours**, a **directional bevel highlight** (consistent top-left key light), **broken micro-wear specular glints** (never continuous uniform-width strokes), crevice ambient occlusion, and a soft feathered outer drop shadow.
   - Keep tile interiors calm dark smoked glass (`bg` layer separated from `border` layer so the user's Background opacity slider works cleanly) with a subtle inner vignette so white/colored SkyUI & I4 item icons pop with high contrast.
3. **Luminous Metal States (Zero Neon Wireframes)**:
   - Author neutral white/silver beveled rim catch-light + soft feathered inner wash textures for `selected` and `equipped` layers (`tint: "selection"` / `"accent"` / `"equipped"`).
   - Quickslot automatically suppresses stacked procedural `AddPolyline` boxes whenever a theme provides `selected` / `equipped` textures.
4. **Cohesive Shape Family**:
   - Every theme pack should forge matching artwork for `square`, `card`, `panel`, `diamond`, `circle`, `hexagon`, `wheel` (`ringBg`, `outerRim`, `innerRim`, `hub*`, `pointer`, `divider`), `divider`, `markers`, and `equipmentPanel` using the same material, lighting angle, and logical rim weight.

## Authoring Workflow with Design Studio

### 1. Forge via CLI (`tools/design-studio/forge.py`)
```bash
# List built-in PBR material presets
python3 tools/design-studio/forge.py --list-materials

# Forge one flagship theme into assets/themes/<id>/
python3 tools/design-studio/forge.py --theme oathvein --out assets/themes/oathvein

# Forge a custom JSON recipe
python3 tools/design-studio/forge.py --recipe my_theme.recipe.json --out assets/themes/my-theme
```

### 2. Interactive Local Web UI (`tools/design-studio/studio.py`)
```bash
python3 tools/design-studio/studio.py --port 8765 --open
```
Use the browser studio to dial in `rimPx`, `chamferPx`, `bevelHeight`, `grain`, `wear`, `patinaStrength`, `emissiveStrength`, palette colors, ornament style, or upload an external/AI-generated PNG stamp (auto-alpha + 4-way/8-way/radial symmetry + height extraction), then export directly into `assets/themes/<id>/` or Skyrim's `Data/Quickslot/themes/<id>/` for instant live hot-reload.

### 3. External / AI Ornament Stamp Post-Processing (`tools/design-studio/stamps.py`)
When incorporating painted ornaments from `image-generator` or Photoshop/Krita:
- Prompt or paint strictly orthographic straight-on views on pure solid black (`#000000`) or transparent alpha.
- Run through `stamps.import_stamp(..., symmetry="quad"|"oct"|"radial8", clear_center_ratio=0.72)` so corners are mathematically symmetrical, the interior stays clear for icons, and the extracted heightmap inherits the active theme's PBR bevel lighting.

## Mandatory Visual Verification Checklist

Never judge HUD art solely on a flat neutral gray canvas. After forging or editing a theme:
1. Render at true `1080p` proportions with `./build/native/quickslot_preview` over both a **dark forest/stone gameplay backdrop** and a **bright snow backdrop** (`Souls Cross` + compact `List` selector + all 5 non-frameless shapes + segmented `Wheel`).
2. Inspect the rendered PNGs directly with `read_file` and verify:
   - List panel rim thickness matches Square tile rim thickness (no 9-slice bloat).
   - Active/selected and equipped tiles show a refined metallic gleam without thick neon wireframe boxes.
   - Item icons, hotkeys, and list labels remain crisp and immediately legible on both dark and snow backdrops.
   - No clipped corner ornaments or alpha fringe halos.
