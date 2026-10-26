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

Item icons support I4 packs and Quickslot atlas packs, with Original, Theme and White color settings. Keycaps come from the active `Data/Interface/ImGuiIcons/Icons` files; select their style through the mod manager and restart.

Frame and font credits are in `licenses/Theme-pack-credits.txt`. Source hashes are in `assets/themes/packs-manifest.json`.

## I4 icons

Install [Inventory Interface Information Injector](https://www.nexusmods.com/skyrimspecialedition/mods/85702) and your preferred I4 packs, then choose **I4** under **Appearance → Style → Icon set**. Quickslot uses each item's assigned SWF artwork, frame label and color. Skyrim's resource loading supplies the active files, including resources in archives.

**Original** preserves the artwork's colors and I4 color assignments. **Theme** applies theme colors, including the **Tint magic icons** setting. **White** displays white artwork. Opacity and unavailable-item dimming apply to each mode.

The bundled Quickslot icons provide artwork for synthetic shortcuts and a fallback when I4 or an icon is unavailable. Select **Quickslot** or an installed atlas pack in **Icon set** to use that pack throughout the HUD.

## Item atlas

`Quickslot/icons/items.png` is a 2048 × 2304 RGBA PNG with eight columns and nine rows of 256 × 256 cells. Artwork uses white RGB values and transparent cutouts. The packer preserves each silhouette's aspect ratio, centers it, and leaves at least 20 pixels of clear padding. Cells are numbered from zero:

| Row | Cells | Order from left to right |
| --- | --- | --- |
| 1 | 0–7 | sword, greatsword, dagger, axe, mace, bow, staff, shield |
| 2 | 8–15 | torch, spell, battleaxe, warhammer, crossbow, alteration, conjuration, destruction |
| 3 | 16–23 | illusion, restoration, fire, frost, shock, potion, health, magicka |
| 4 | 24–31 | stamina, food, poison, scroll, power, shout, arrow, bolt |
| 5 | 32–39 | unarmed, shortsword, rapier, saber, scimitar, falchion, katana, nodachi |
| 6 | 40–47 | flamberge, stiletto, tanto, hatchet, morningstar, club, spiked_club, maul |
| 7 | 48–55 | spear, shortspear, pike, halberd, glaive, naginata, trident, scythe |
| 8 | 56–63 | pickaxe, woodcutters_axe, javelin, quarterstaff, claw, whip, gun, lesser_power |
| 9 | 64–71 | greater_power, unused, unused, unused, unused, unused, unused, unused |

The 64 source icons are stored in `assets/icons/source`. The gun cell aliases the crossbow drawing, making 65 populated cells. To supply gun artwork, add `gun.png` and update its entry in `catalog.json`. The catalog maps filenames to cells; `war_axe.png` supplies the `axe` cell. The manifest records each source hash, packing bounds, conversion method and source type. The packer extracts the white mask from an opaque scroll source and uses alpha for transparent sources.

`lesser_power.png` supplies lesser powers (`kLesserPower`) at cell 63; `greater_power.png` supplies daily powers (`kPower`) at cell 64. The generic `power` cell is at index 28.

Run `python3 tools/pack-icons.py` after updating the source PNGs. It generates `items.png`, `manifest.json` and `src/Icons.h` from the catalog. `--check` verifies that these files match the inputs; `--preview <path.png>` produces a labeled contact sheet. Packing requires Pillow and every source file listed in the catalog. Adding catalog entries or changing the grid requires rebuilding the DLL; replacing artwork in an existing cell preserves the layout contract.

`src/WeaponIcons.h` maps category keywords from the `WeapType`, `OCF_WeapType` and `ABC_I4WIO_` families. A one-handed katana uses the katana cell; a two-handed katana uses nodachi. Specific tags win over broader families, independent of keyword order. Falchion, flamberge and morningstar icons require a matching type tag on the weapon.

Ammo, unarmed and empty-hand actions use atlas cells. Empty-hand actions use the unarmed cell. When the atlas is unavailable, Quickslot uses procedural ammo/hand glyphs and placeholders for other items. The live editor preview uses the same image and tint as the game HUD.

Install a replacement PNG under the same path in a mod with higher priority, then restart Skyrim. The image is loaded once per renderer initialization. A missing or incorrectly sized file records a warning and uses fallback glyphs.

## Selectable item icon packs

An additional set lives in `Data/Quickslot/icons/packs/<folder>/`. Include `items.png` and this `pack.json`:

```json
{"schema":1,"id":"my-icons","name":"My icons","atlas":"items.png","columns":8,"rows":9,"cellSize":256,"icons":65}
```

Keep the cell order above and the 2048 × 2304 size. IDs use lowercase ASCII letters, digits, hyphens or underscores, up to 64 characters; `quickslot` is reserved. Names are UTF-8, up to 80 bytes. The atlas must be a PNG filename within the same folder. Discovery checks up to 64 folders; invalid metadata or images are skipped.

Restart after installation. **Appearance → Style → Icon set** appears when an additional pack is installed and switches among loaded packs immediately, independently of frame themes and keycap packs. The choice supports Undo and appearance presets. Missing packs fall back to Quickslot while retaining the chosen ID for reinstallation.
