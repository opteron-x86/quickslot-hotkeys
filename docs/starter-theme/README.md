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

See [`THEMES.md`](../../THEMES.md) for the complete `schema: 3` reference.
