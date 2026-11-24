# Starter theme template

Copy this folder to `Data/Quickslot/themes/<your-theme-id>/` and set `"id"` and `"name"` in `theme.json` to match your theme.

## Live editing

1. Launch Skyrim, press **Shift + `\`**, and open **Appearance → Theme**.
2. Select your theme under **Theme** and turn on **Auto-reload theme changes** (or click **Reload themes** after saving a file).
3. Edit `theme.json` or any `.svg` / `.png` file in your theme folder. Quickslot reloads the manifest, rasterizes updated vector layers, and displays any validation messages under **Theme diagnostics**.

From a source checkout on Linux, `quickslot_preview` also watches `assets/themes/` directly:

```sh
./build/native/quickslot_preview . "" skin:<your-theme-id>:3:5:0 1600 900 --watch
```

Press **R** in the preview window at any time to force a reload.

## Included sample files

- `theme.json` — `schema: 3` manifest demonstrating `extends`, semantic `palette` roles, `style` controls, `@fonts/` shared typography, `"recommended"` look settings, and modular `"assets"` slots.
- `card_border.svg` — `256 × 256` nine-sliced rectangular frame (`slice: 40`).
- `diamond_border.svg` — `256 × 256` upright lozenge border.
- `wheel_outer_rim.svg` — `512 × 512` circular outer rim (`r = 253`) tracking any segmented wheel radius.
- `wheel_pointer.svg` — `64 × 64` upward-pointing indicator mask tinted by `"accent"`.

## PNG reskin dimensions and Design Studio

For hand-painted RGBA PNG packs (Photoshop, Krita, GIMP, Affinity, or Blender), recommended canvas sizes at `3×` logical density are:

- `square` (`256 × 256`, `slice: 60`, `sliceSize: 20`), `card` (`384 × 144`, `slice: 54`, `sliceSize: 18`), `panel` (`384 × 256`, `slice: 60`, `sliceSize: 20`)
- `diamond`, `circle`, `hexagon` (`256 × 256`)
- `wheel.outerRim` (`512 × 512`), `wheel.innerRim` (`384 × 384`), `wheel.hub*` (`256 × 256`), `wheel.pointer` (`96 × 96`)

Setting `"sliceSize"` keeps border and corner thickness constant in logical HUD pixels across small tiles and wide List panels. To synthesize a matching PBR PNG set interactively, run `python3 tools/design-studio/studio.py --open` (see [`tools/design-studio/README.md`](../../tools/design-studio/README.md)).

See [`THEMES.md`](../../THEMES.md) for the complete `schema: 3` reference.
