# Theme and icon packs

Place a theme in `Data/Quickslot/themes/<folder>/theme.json`. Restart after installing a pack; selecting it in Appearance applies it immediately.

```json
{
  "schema": 1,
  "id": "my-theme",
  "name": "My theme",
  "frameStyle": 0,
  "palette": {
    "text": [233, 230, 221],
    "secondary": [180, 179, 174],
    "muted": [134, 135, 132],
    "accent": [157, 65, 54],
    "background": [13, 14, 15, 170],
    "border": [126, 126, 119, 180]
  },
  "atlas": "frames.png",
  "font": "font.ttf",
  "titleFont": "title.ttf"
}
```

`schema`, `id` and `name` are required. IDs use lowercase letters, digits, hyphens or underscores, up to 48 characters. Asset paths are relative to the pack folder. Colors use RGB or RGBA values from 0 to 255.

`frameStyle` selects clipped corners (0), straight edges (1), double borders (2) or angular linework (3). An optional frame atlas is a **1024 × 320 RGBA PNG** with four 256 × 320 cells: top, bottom, side and interior texture. Artwork occupies x=4–251 and y=6–313 within each cell. Diamonds rotate the bottom frame; circles use the interior texture.

Optional TTF files supply body and title fonts. The default font is Jost. Theme overrides for accent, frame opacity and icon size are saved by theme ID. **Reset theme** restores that pack's defaults.

Item icons use I4 packs, with Original, Theme and White color settings. Keycaps come from the active `Data/Interface/ImGuiIcons/Icons` files; select their style through the mod manager and restart.

Frame and font credits are in `licenses/Theme-pack-credits.txt`. Source hashes are in `assets/themes/packs-manifest.json`.

## I4 icons

Install [Inventory Interface Information Injector (I4) 1.1.0+](https://www.nexusmods.com/skyrimspecialedition/mods/85702) for your Skyrim version and your preferred I4 packs. Quickslot uses each item's assigned SWF artwork, frame label and color. Skyrim's resource loading supplies the active files, including resources in archives.

**Original** preserves the artwork's colors and I4 color assignments. **Theme** applies theme colors, including the **Tint magic icons** setting. **White** displays white artwork. Opacity and unavailable-item dimming apply to each mode.

Smart shortcuts use the artwork assigned to the item they currently select. Combinations display both selected items. Empty-hand and unarmed actions use SkyUI's `armor_hands` gauntlet. A rule with no matching item uses its SkyUI category icon.

I4 packs configure the artwork for inventory items and spells through I4's JSON rules. Quickslot reads each assignment through I4's public API. An unavailable SWF or frame uses the item's SkyUI category icon. Restart Skyrim after changing icon packs.
