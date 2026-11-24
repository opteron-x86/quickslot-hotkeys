# Theme and icon packs

Place a theme in `Data/Quickslot/themes/<folder>/theme.json`. Select it under **Appearance → Theme** to apply it immediately. Use **Reload themes** in **Appearance → Theme** to pick up edits to manifests, images and fonts without restarting Skyrim, or enable **Auto-reload theme changes** while that page is open. Load warnings appear under **Theme diagnostics** and in `Quickslot.log`.

Copy [`docs/starter-theme/`](docs/starter-theme/README.md) to `Data/Quickslot/themes/<your-theme-id>/` as a starting template. From a source checkout on Linux, `./build/native/quickslot_preview . "" skin:<your-theme-id>:3:5:0 1600 900 --watch` renders a live preview and reloads edits automatically (`R` reloads immediately).

```json
{
  "schema": 3,
  "id": "my-theme",
  "name": "My theme",
  "extends": "minimal",
  "frameStyle": 0,
  "palette": {
    "text": [233, 230, 221],
    "secondary": [180, 179, 174],
    "muted": [134, 135, 132],
    "accent": [157, 65, 54],
    "background": [13, 14, 15, 170],
    "border": [126, 126, 119, 180],
    "charge": [138, 108, 214]
  },
  "font": "@fonts/Jost-Regular.ttf",
  "titleFont": "title.ttf",
  "numberFont": "numbers.ttf",
  "recommended": {
    "shape": 3,
    "selectorShape": 3,
    "equipmentStyle": 0,
    "wheelStyle": 1,
    "wheelHighlight": 3,
    "equippedHighlight": 1,
    "iconScale": 1.0,
    "chargeIcons": 2
  }
}
```

`schema`, `id` and `name` are required. Schema `1`, `2` and `3` are supported; schema `3` supports modular PNG/SVG assets, style controls, recommended look defaults and inheritance. IDs use lowercase letters, digits, hyphens or underscores, up to 48 characters. Asset paths are relative to the pack folder. Colors use RGB or RGBA values from 0 to 255.

Bundled themes include built-in **Minimal** and **Skyrim silver**, eleven interface-matching packs (**Amethyst**, **Dear Diary Dark**, **Edge**, **Horizons**, **Immortal**, **Norden**, **Nordic**, **Oathvein**, **Outlander**, **Untarnished**, **Vel'dun**) and five standalone packs (**Crimson Filigree**, **Daedric Oblivion**, **Dwemer Aetherium**, **Imperial Parchment**, **Sovngarde Astral**).

### Inheritance

Set `"extends": "<parent-id>"` in a schema `3` pack to inherit palette roles, style values, fonts and assets from a built-in theme (`minimal`, `silver`) or another installed pack, up to four levels deep. Inherited asset paths resolve inside the parent pack's folder. Setting an asset slot to `""` or `null` clears the inherited image for that slot. Child packs declare their own `detect` list when interface auto-detection is desired.

### Modular PNG and SVG assets (schema 3)

Schema `3` themes supply standalone PNG or SVG images under `"assets"`. Any omitted slot uses `"atlas"` (when present) or procedural geometry.

```json
"assets": {
  "card": {
    "shadow": "card_shadow.png",
    "bg": "card_bg.svg",
    "border": { "file": "card_border.svg", "tint": "border", "slice": [40, 40, 40, 40], "rasterSize": 512 },
    "selected": { "file": "card_selected.svg", "tint": "accent", "slice": 40 },
    "equipped": { "file": "card_equipped.svg", "tint": "equipped", "slice": 40 }
  },
  "square": { "bg": "square_bg.svg", "border": "square_border.svg", "selected": "square_selected.svg" },
  "diamond": { "bg": "diamond_bg.svg", "border": "diamond_border.svg", "selected": "diamond_selected.svg" },
  "circle": { "bg": "circle_bg.svg", "border": "circle_border.svg", "selected": "circle_selected.svg" },
  "wheel": {
    "ringBg": { "file": "wheel_ring_bg.svg", "rasterSize": 1024 },
    "outerRim": { "file": "wheel_outer_rim.svg", "rasterSize": 1024 },
    "innerRim": { "file": "wheel_inner_rim.svg", "rasterSize": 512 },
    "ringBorder": { "file": "wheel_ring_border.svg", "rasterSize": 1024 },
    "hubBg": "wheel_hub_bg.svg",
    "hubBorder": "wheel_hub_border.svg",
    "hubOverlay": "wheel_hub_overlay.svg",
    "pointer": { "file": "wheel_pointer.svg", "tint": "accent" },
    "divider": { "file": "wheel_divider.svg", "tint": "border" }
  },
  "divider": { "file": "divider.svg", "tint": "border" },
  "markers": {
    "equipped": { "file": "marker_equipped.svg", "tint": "equipped" },
    "ready": { "file": "marker_ready.svg", "tint": "equipped" },
    "poison": { "file": "marker_poison.svg", "tint": "poison" },
    "chargePip": { "file": "marker_pip.svg", "tint": "charge" },
    "soulGem": { "file": "marker_soulgem.svg", "tint": "charge" }
  },
  "equipmentPanel": { "bg": "equip_bg.svg", "border": "equip_border.svg" }
}
```

Each asset accepts a file path string or an object:

| Field | Purpose | Default |
| --- | --- | --- |
| `file` | Relative `.png` or `.svg` path inside the pack folder | Required |
| `tint` | Color tint role: `none`, `text`, `secondary`, `muted`, `accent`, `border`, `selection`, `equipped`, `background`, `warning`, `depleted`, `poison`, `charge` | `none` |
| `opacity` | Layer opacity multiplier (`0.0`–`1.0`) | `1.0` |
| `slice` | Nine-slice margins in source pixels: a single integer or `[left, top, right, bottom]` (`0`–`1024`). Corners scale uniformly against the frame's shorter side | `[0, 0, 0, 0]` |
| `rasterSize` | Longest-edge pixel dimension used when rasterizing an SVG (`64`–`2048`) | `512` |

Background layers (`bg`, `ringBg`, `hubBg`) multiply by the user's **Background opacity** and **Frame opacity** settings; border layers multiply by **Frame opacity**. `diamond` and `circle` layers draw upright across the tile's bounding square. On segmented wheels, `wheel.outerRim` scales to the outer radius, `wheel.innerRim` scales to the inner radius across any **Ring thickness** (`10%`–`100%`), and `wheel.ringBg` maps radially across each wedge from inner to outer radius, including raised pointed segments. `wheel.pointer` artwork points upward and rotates toward the active direction.

### Recommended look (schema 3)

An optional `"recommended"` object suggests visual defaults tuned for the theme without changing the user's layout, anchors, sizes, hotkeys or groups. Clicking **Apply theme's recommended look** under **Appearance → Theme** applies the specified fields as a single undoable step:

| Field | Values | Purpose |
| --- | --- | --- |
| `shape` | `0` Cards, `1` Circles, `2` Squares, `3` Diamonds | Group tile shape |
| `selectorShape` | `0` Cards, `1` Circles, `2` Squares, `3` Diamonds | Selector item shape |
| `equipmentStyle` | `0` List, `1` Circles, `2` Diamonds, `3` Squares, `4` Text | Equipment display style |
| `wheelStyle` | `0` Tiles, `1` Segments | Wheel selector style |
| `wheelHighlight` | `0` Fill and outline, `1` Fill, `2` Outline, `3` Rim | Pointed segment highlight |
| `equippedHighlight` | `0` Marker only, `1` Outline, `2` Tint, `3` Outline and tint | Equipped tile highlight |
| `iconScale` | `0.5`–`1.5` | Item icon scale |
| `chargeIcons` | `0` Off, `1` Fill the icon, `2` Soul gem | Enchantment charge display on icons |

### Style options (schema 3)

Optional values under `"style"` tune procedural geometry and editor chrome:

| Field | Range | Default | Purpose |
| --- | --- | --- | --- |
| `cornerCut` | `0`–`24` | From `frameStyle` | Bevel size in pixels on procedural rectangular frames |
| `borderWidth` | `0.5`–`4.0` | `0.8` | Procedural border thickness |
| `shadowOpacity` | `0.0`–`1.0` | `0.35` | Drop-shadow opacity behind tiles |
| `glowOpacity` | `0.0`–`1.0` | `0.28` | Outer halo opacity on active/equipped highlights |
| `iconInset` | `0.05`–`0.35` | `0.14` | Icon padding ratio inside tiles |
| `editorRounding` | `0`–`12` | `2` | Window and control corner rounding in the editor |
| `accentLine` | boolean | `true` | Draws the procedural selected accent bar when no `selected` layer is set |

### Frame atlas (schema 1–3)

`frameStyle` selects clipped corners (0), straight edges (1), double borders (2) or angular linework (3). An optional `"atlas": "frames.png"` supplies a **1024 × 320 RGBA PNG** with four 256 × 320 cells: top, bottom, side and interior texture (artwork within x=4–251 and y=6–313). Diamonds mirror the top and right edges of the bottom frame across the diagonal; circles use the interior texture. `atlasMargin` sets how many artwork pixels at each edge keep a constant size (default `40`, range 0–120).

### Interface detection

The **Automatic** theme selects the pack that matches the installed interface mod. Each pack lists up to 16 entries under `"detect"`, evaluated against Skyrim's resource system (loose files and archives):

```json
"detect": [
  { "file": "Interface/hudmenu.swf", "sha256": "3614b25f17bbebe8…" },
  { "file": "UntarnishedUI_Subtitle.ini", "exists": true },
  { "file": "Interface/fontconfig.txt", "contains": "Trattatello" }
],
"detectPriority": 60
```

Exact `sha256` matches take precedence over `exists` and `contains` checks. Among `exists` and `contains` matches, higher `detectPriority` (`0`–`100`, default `50`) wins. `contains` searches case-insensitively within the first 64 KiB of the file. `Quickslot.log` records the detected pack. Automatic uses Minimal when no pack matches.

### Fonts

**Appearance → Theme → Font** offers the theme's font, the interface's own font and every font found. Interface mods and ImGui Icons packs install fonts in `Data/Interface/ImGuiIcons/Fonts`; **Installed UI font** uses the first readable `.ttf` or `.otf` there. Optional `font`, `titleFont` and `numberFont` fields supply body, title and badge/count typefaces as paths inside the pack folder or `@fonts/<filename>` paths in `Data/Quickslot/fonts/`. The default font is Jost.

### Palette roles

The six base colors (`text`, `secondary`, `muted`, `accent`, `background`, `border`) can be refined with optional roles:

| Role | Used for | Default |
| --- | --- | --- |
| `selection` | Outline and marker of the highlighted frame | `text` |
| `equipped` | Equipped markers and hand letters | `text` |
| `warning` | Unavailable-item slash | Red |
| `depleted` | Zero quantities and unavailable status text | Light red |
| `poison` | Poisoned-weapon droplet | Green |
| `charge` | Enchantment charge pips and soul gem | Violet |
| `panel` | Editor window surface | Opaque shade of `background` |
| `panelBorder` | Editor window and panel borders | `border` |

The editor draws its surfaces, text, controls, rounding and highlights from the active theme's palette, style and accent. Theme overrides for accent and frame opacity are saved by theme ID; icon size is one setting shared by every theme. **Reset theme** restores that pack's defaults.

Item icons use I4, SkyUI or PNGII packs, with Original, Theme and White color settings. Keycaps come from the active `Data/Interface/ImGuiIcons/Icons` files; select their style through the mod manager and restart.

Frame and font credits are in [`licenses/Theme-pack-credits.txt`](https://github.com/opteron-x86/quickslot-hotkeys/blob/master/licenses/Theme-pack-credits.txt) on GitHub. Source hashes are in `assets/themes/packs-manifest.json`.

## I4 icons

Install [Inventory Interface Information Injector (I4) 1.1.0+](https://www.nexusmods.com/skyrimspecialedition/mods/85702) for your Skyrim version and your preferred I4 packs. Quickslot uses each item's assigned SWF artwork, frame label and color. Skyrim's resource loading supplies the active files, including resources in archives.

**Original** preserves the artwork's colors and I4 color assignments. **Theme** applies theme colors, including the **Tint magic icons** setting. **White** displays white artwork. Opacity and unavailable-item dimming apply to each mode.

Smart shortcuts use the artwork assigned to the item they currently select. Combinations display both selected items. Empty-hand and unarmed actions use SkyUI's `armor_hands` gauntlet. A rule with no matching item uses its SkyUI category icon.

I4 packs configure the artwork for inventory items and spells through I4's JSON rules. Quickslot reads each assignment through I4's public API. An unavailable SWF or frame uses the item's SkyUI category icon. Restart Skyrim after changing icon packs.

## PNGII icons

Install [PNG Icon Injector](https://github.com/rijosan87/PNGII) with icon API v1 and choose **PNGII** under **Appearance → Theme → Item icons**. PNGII's item and spell rules choose PNG or SVG artwork using the item's FormID, keywords, plugin and resolved I4 label. Quickslot loads the selected image from loose files or a loaded BSA. I4 supplies artwork when a rule has no match or an image is unavailable.

Artwork fits the icon area with its aspect ratio and transparency preserved. **Original** shows the image's colors. **Theme** and **White** apply the chosen tint. Smart shortcuts query their selected item, combinations show each selected item's artwork, and empty-hand actions use the `armor_hands` label for PNGII matching. Restart Skyrim after changing PNGII rules or images.
