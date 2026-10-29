# Changelog

## Unreleased

- Preserve Skyrim's mouse pointer when opening the console.

## 0.13.0

- Improved compatibility with I4 ports, with in-game dependency messages and detailed missing-icon logs.
- Groups, selectors and equipment share a 0–128 spacing range, with compact layouts at zero and wider gaps at higher values.
- Horizontal equipment spacing follows the width of each item's icon and labels.
- Equipment supports horizontal, vertical, diamond, cross, circle and staggered-cross arrangements, layout presets and optional item names.
- Appearance previews have adjustable zoom and a **Fit preview** button.
- Quickslot yields controls to UIExtensions and custom modal menus, including unpaused menus.
- Quickslot yields controls and hides its HUD during scenes that take control of the player, including OStim. Controls resume when the scene returns control to the player.
- Display matching PNG and SVG artwork from PNG Icon Injector in the widget, browser, equipment display and editor preview. Configure **Use PNGII icons** and **Icon color** under **Appearance → Style**. I4 supplies the remaining icons.

**Skyrim 1.5.97:** I4 icons display with Fuzzles' I4 port.

## 0.12.0

- Item icons use Inventory Interface Information Injector (I4) 1.1.0+, a required dependency. Choose Original, Theme or White coloring in Appearance → Style.
- Smart shortcuts show the item they currently select, combinations show both items, and empty-hand actions use a gauntlet icon.
- Add equipped arrows or bolts and their count to the equipment widget with **Appearance → Equipment → Show equipped ammunition**.
- Item quantities, ammunition counts and cooldown timers use outlined text.
- Hotkeys appear as plain text on frames when key artwork is disabled or unavailable.

Updating: install I4 1.1.0+ for your Skyrim version. Keep your settings and presets.

## 0.11.0

- Configure keyboard editing modifiers, browsing keys and controller actions under **Controls**. Choose **None** or **Clear** to disable a binding. Enable **Allow group selection with unused modifiers** to use group keys while holding a walking modifier ([#14](https://github.com/opteron-x86/quickslot-hotkeys/pull/14)).
- Use Quickslot while QuickLoot is displayed, with separate keyboard/mouse and controller options under **Controls → General** ([#15](https://github.com/opteron-x86/quickslot-hotkeys/pull/15)).
- See poison indicators on weapons and equipped arrow/bolt counts on bows and crossbows. Configure these indicators under **Appearance → Style** ([#16](https://github.com/opteron-x86/quickslot-hotkeys/pull/16)).
- Choose a hand for the highlighted weapon or spell while browsing. Hand choices apply to the current use, with configurable keyboard, mouse and controller bindings plus mouse-button and bumper presets ([#17](https://github.com/opteron-x86/quickslot-hotkeys/pull/17)).
- Link groups to Skyrim Favorites by category. Groups display all matching favorites, with alphabetical browsing, exclusions and a default hand ([#18](https://github.com/opteron-x86/quickslot-hotkeys/pull/18)).

Updating: keep your settings and presets. Enable QuickLoot input and hand-selection bindings in Controls to use those features.

## 0.10.3

- Reliable editor text entry with IME integrations ([#4](https://github.com/opteron-x86/quickslot-hotkeys/pull/4)).

Updating: keep your settings and presets. Enable SimpleIME's engine-input routing declaration for Chinese text entry in the editor.

## 0.10.2

- **Remove group** resets its contents, name, bindings, behavior and auto-sort setting. Undo restores the group.
- **Add group** assigns the lowest available number key from **1–8** and available controller/VR defaults.
- Group names refresh when adding or removing groups.
- Editor text entry supports cursor-only IME overlays.
- Editor logs include closing reasons and menu details.
- Enable or disable individual keyboard editing shortcuts under **Controls → Keyboard → Editing shortcuts**, with **Enable all / Disable all** controls.

Updating: keep your settings and presets.

## 0.10.1

- Improved stability when using Quickslot alongside other menu mods.
- Updated katana and nodachi icons.
- Clear group hotkeys with **Clear** or **Esc** during binding. Assigning an occupied key swaps the group bindings.
- Sort groups by name with **Sort A–Z** or **Auto-sort when adding**. Both support Undo.

Updating: install the complete release package and keep your settings and presets.

## 0.10.0

- Hold-to-browse and tap-to-cycle groups with remembered selections and shortcut reordering.
- Searchable shared shortcuts, hand combinations and smart equipment choices.
- Consumable selection and restocking, including torches.
- Controller navigation, modifier bindings, pointer controls and on-screen text entry.
- Equipment display with quantities, shout timers and spent-power indicators.
- Themes, layouts, positioning, visibility controls, live previews and presets.
- Weapon icons, distinct lesser/greater power icons, and javelin, quarterstaff, claw and whip categories.
- Mod, source, symbols, menu API and checksum downloads.
- Experimental VR support.

Updating: install the complete release package and keep your settings and presets.

## 0.9.0-rc20

- Smart weapon and shield rules in combination hand pickers; separate item copies for each hand.
- Best Shield ranked by armor, alongside physical-damage weapon ranking.
- Left/Right browses; Ctrl + Left/Right reorders. Both wrap and retain the highlighted entry.
- Weapon-category icons for smart weapons.
- Right-hand-first equipment display, quantities and shout timers on icons, and dimmed spent powers.
- Correct hand-picker focus after switching from controller to mouse.
- GitHub builds with mod, source, symbols and API downloads.

## 0.9.0-rc19

- Spacing controls the gaps between icons at a fixed icon size, including previews.
- Widget scale controls preview size.

## 0.9.0-rc18

- Torch shortcuts follow stock of their saved type and become available after restocking.
- Torch quantities and left-hand equipment state appear in groups and combinations.

## 0.9.0-rc17

- Independent equipment spacing for every style and arrangement, with live preview, Undo and presets.

## 0.9.0-rc16

- Tap-to-cycle recalls the selected item when returning to a group.
- Controller text entry opens from name and search fields.
- D-pad access to nested panels and the Add button; right-stick pointer with click and drag.
- Independent equipment scale and text size.
- Native torch and two-handed equipment handling.

## 0.9.0-rc15

- Group and equipment HUD visibility remains active during QuickLoot navigation.
- Counts, cooldowns and equipment state update while looting.

## 0.9.0-rc14

- QuickLoot receives controller, keyboard and mouse input while open.
- Opening QuickLoot cancels selection and pending actions.

## 0.9.0-rc13

- Independent equipment visibility, idle delay and opacity.
- List, circle, diamond, square and text-only layouts with configurable text placement.

## 0.9.0-rc12

- Hold-to-browse and tap-to-cycle group modes.
- Consumables use tap-to-use and hold-to-choose.
- Compact equipment display and complete-combination highlighting.

## 0.9.0-rc11

- Per-group cycling and hold actions.
- Equipment display, inactive-group visibility and hand markers.
- Public menu API for opening, closing, state queries and shortcut control.

## 0.9.0-rc10

- Editor tab memory and independent text sizing.
- Position outlines, edge/center snapping and aspect-ratio guides.
- Fine, Normal, Coarse and Custom grid spacing.

## 0.9.0-rc9

- Always-visible, idle-fade, idle-hide and selection-only widget modes.
- Combat and drawn-weapon visibility overrides.
- Interactive-menu input priority.

## 0.9.0-rc8

- Searchable shared shortcut library with linked edits across groups.
- Selectable item atlases and atlas-based hand-action icons.
- Javelin, quarterstaff, claw, whip and gun categories.

## 0.9.0-rc7

- Unicode search, font fallbacks and UTF-8 translation packs.
- Smart weapons and ammunition.
- Equipment toggle actions and DirectX state restoration.

## 0.9.0-rc6

- Controller modifier matching and configurable modifier buttons.
- Unified Groups editor with searchable combination hand pickers.
- Both hands, Use equipped, automatic names and inline shortcut editing.

## 0.9.0-rc5

- Named themes and unified widget positioning.
- Theme-independent layout presets.
- Configurable F1, F7 and F8 shortcuts.

## 0.9.0-rc4

- Standalone builds from pinned dependency sources.
- Concise theme descriptions and organized theme assets.

## 0.9.0-rc3

- Shift + backslash editor shortcut, with configurable modifiers.

## 0.9.0-rc2

- Single-line inventory rows, compact quantities and concise editor labels.

## 0.9.0-rc1

- SE, AE and VR build targets using CommonLibSSE-NG 9.0.0.
- Experimental VR HUD/editor support through ImGui VR Helper.
- VR controller bindings, grip chords, handedness and pointer input.
- Complete supplied icon set with tintable white artwork.
- Installation/build guides and theme/dependency credits.
