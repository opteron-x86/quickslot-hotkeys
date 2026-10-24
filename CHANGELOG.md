# Changelog

## 0.10.3

- Fixed editor text fields becoming untypable when an IME integration clears Skyrim's text-entry counter during opening. The editor now waits until its menu is open before claiming text input, and restores a cleared claim while the editor remains open ([#3](https://github.com/opteron-x86/quickslot-hotkeys/issues/3), [#4](https://github.com/opteron-x86/quickslot-hotkeys/pull/4)).
- Guarded cleanup so closing after a cleared claim does not request an extra release. Recovery warnings are limited to once per editor session.
- Documented the engine character-input queue used by the editor for IME integrations.

In-game verification: the SimpleIME author confirmed nine ownership-test cycles with its Quickslot declaration disabled, plus three end-to-end Chinese-input cycles with its released routing declaration enabled. See the [verification notes](https://github.com/opteron-x86/quickslot-hotkeys/pull/4#issuecomment-6380918875).

Updating: keep your settings and presets. SimpleIME's engine-input routing declaration is still needed for committed Chinese text to reach the editor.

## 0.10.2

- Removing a group now resets its name, contents, remembered selection, keyboard/controller/VR bindings, behavior and auto-sort setting. Undo restores the complete group.
- Added groups start fresh, including slots left hidden by older versions. The keyboard binding uses the lowest unused number from **1–8**, skipping the editor shortcut; the default name follows that number. Controller and VR bindings use available defaults. Existing groups keep their bindings.
- Refreshed the editor's name field after adding or removing a group so old names cannot reappear while editing.
- Fixed the editor closing when a non-modal, cursor-only text-entry overlay appears. Gameplay hotkeys still yield to cursor menus; full game menus and other modal interfaces still close the editor.
- Editor close logs now include the reason and, when another menu intervenes, its name and flags.
- Added individual switches and **Enable all / Disable all** under **Controls → Keyboard → Editing shortcuts**. Capture for each hand, moving between groups, unequipping, removing and reordering can all be disabled independently. Disabled actions pass their keys to the game and other mods; browsing and cancellation remain available. Existing settings keep all editing shortcuts enabled.

Updating: keep your settings and presets. Existing groups retain their names, items and bindings; the reset behavior applies when removing or adding a group. Editing shortcuts remain enabled until you opt out.

## 0.10.1

- Fixed DirectX shader-state cleanup that could crash when used alongside another menu mod.
- Updated katana and nodachi icon artwork.
- Clear a group hotkey with **Clear**, or click its binding and press **Esc**. Assigning a key used by another group swaps the two bindings; removed groups release their keys.
- Added **Sort A–Z** and an optional **Auto-sort when adding** setting for each group. Sorting ignores case, preserves the remembered item and supports Undo.

Updating: install the DLL and atlas together. Existing groups keep their order until sorting is requested; auto-sort starts disabled.

## 0.10.0

This standard release brings together the features and refinements from the 0.9.0 release candidates.

- Hold-to-browse and tap-to-cycle groups with remembered selections, wrapping keyboard navigation and shortcut reordering.
- Searchable shared shortcuts and hand combinations, including smart weapons and shields with separate item copies for each hand.
- Smart equipment ranking, consumable selection and restocking, and torch shortcuts that follow inventory stock.
- Controller navigation, modifier bindings, pointer controls and on-screen text entry.
- Independent equipment display with right-hand-first ordering, quantity badges, shout timers and spent-power dimming.
- Theme, layout, scale, spacing, positioning and visibility controls with live previews, Undo and presets.
- Refreshed icon artwork, distinct lesser/greater power icons, and dedicated javelin, quarterstaff, claw and whip drawings. The atlas contains 64 supplied drawings and one gun-to-crossbow alias.
- Automated Linux tests, Windows build/probes, and versioned packages containing the mod, source, symbols, menu API and checksums.

Updating: install the DLL and atlas together and keep existing settings and presets. Custom icon packs must use the new 2048 × 2304, 65-icon layout described in `THEMES.md`.

VR support remains experimental; physical headset validation is pending.

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
