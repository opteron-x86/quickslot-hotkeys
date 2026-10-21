# Quickslot

[Downloads](https://github.com/opteron-x86/quickslot-hotkeys/releases) · [Development builds](https://github.com/opteron-x86/quickslot-hotkeys/actions/workflows/build.yml) · [Changelog](CHANGELOG.md) · [Building](BUILDING.md)

Quickslot organizes weapons, spells and consumables into hotkey groups. Hold a group key, browse, and release to select. Tap to equip or use the remembered entry.

## Install

Install the main ZIP through Mod Organizer 2 or Vortex and launch through SKSE. The `SKSE` and `Quickslot` folders belong in Skyrim's `Data` folder. Open the editor in a loaded game with **Shift + `\`**.

Requirements:

- [SKSE](https://skse.silverlock.org/) matching your Skyrim executable.
- [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444) matching your game version.
- [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604).
- Microsoft Visual C++ Redistributable, x64.

VR uses SKSEVR, [VR Address Library](https://github.com/alandtse/skyrim_vr_address_library), [SkyUI VR](https://github.com/Odie/skyui-vr), and [ImGui VR Helper 1.9.4+](https://www.nexusmods.com/skyrimspecialedition/mods/183466). VR support is experimental; physical headset validation is pending.

Optional integrations include [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) for its editor entry, ImGui Icons for keycaps, and Skyrim Souls RE for selection slowdown. Physical controller/headset checks are listed in [Testing](docs/TESTING.md).

## Groups

Add inventory items and spells in **Groups**. **New** creates combinations, smart potions, smart weapons, smart ammo, shields and empty-hand actions. Saved shortcuts appear in the searchable item list and can belong to several groups; editing one updates every linked group.

A combination has a searchable picker for each hand. Choose inventory, spells or smart rules such as Best Sword + Best Mace or Best Axe + Best Shield. **Both hands** copies a spell; **Use equipped** captures the current pair.

Choose a behavior for each group:

- **Hold to browse:** tap equips or uses the remembered entry; hold, browse and release chooses another.
- **Tap to cycle:** the first press when returning to a group recalls its selection. Further presses advance and equip.

Groups containing consumables use tap-to-use and hold-to-choose. Browsing a potion readies it; the next tap uses one dose. Powers and shouts support activation from the group hotkey, with configurable shout charge.

Smart weapons rank by physical damage, including tempering and player modifiers. Smart shields rank by armor; smart ammo ranks by damage. Smart potions offer strongest, weakest and best fit for missing health, magicka or stamina. Regeneration potions offer strongest or weakest.

Weapons, armor and shields follow the captured copy. Torches and consumables follow inventory stock of their saved type. Unavailable entries remain in the group for retrieval or restocking.

## Controls

Bindings are configurable under **Controls**. Up to eight groups are available; keyboard groups start with **1**, **2** and **3**, and controller groups start with the four D-pad directions.

| Action | Keyboard / mouse | Controller |
| --- | --- | --- |
| Equip or use remembered entry | Tap group key | Tap group button |
| Browse | Hold group key + wheel or Left/Right | Hold group button + right stick or LB/RB |
| Select | Release group key | Release group button |
| Reorder while browsing | Ctrl + Left/Right | Editor controls |
| Cancel | Escape / right-click | B / Circle |
| Unequip highlighted entry | Backspace while browsing | X / Square |
| Remove highlighted shortcut | Delete while browsing | Editor controls |
| Open editor | Shift + `\` | Hold D-pad Up, then Start / Options |
| Navigate editor | Mouse / keyboard | D-pad / left stick; A / Cross confirms |
| Editor pointer | Mouse | Right stick; A / Cross clicks or drags |
| Close editor | Escape / Close | Start / Options |

Left/Right browsing and Ctrl + Left/Right reordering wrap at both ends. Activating a name or search field with a controller opens the on-screen keyboard. Selecting an inventory row focuses its Add button for the next confirmation.

**Ctrl + group key** adds the equipped right hand or highlighted Inventory/Magic item; **Ctrl + Shift + group key** adds the left hand. **Shift + group key** unequips the remembered entry; **Alt + group key** removes its shortcut. While browsing, **Ctrl + destination group key** moves the highlighted shortcut to that group.

Controller modifiers are configured per binding. Hold the modifier first; release the group button to select. Releasing the modifier cancels browsing. Other assigned chords, such as B + D-pad, pass to their owner. **Modifier buttons** controls which held buttons suppress unmodified group bindings.

XInput and native PS4 input are supported. Steam Input can supply XInput. Skyrim must recognize the controller first.

### VR controls

**Main** and **Off** follow Skyrim VR's handedness setting. Bindings are configurable under **Controls → Controller**.

| Action | Default |
| --- | --- |
| Group 1 / 2 | Off grip + Main trigger / stick click |
| Group 3 / 4 | Main grip + Off trigger / stick click |
| Browse | Hold the chord and move the Main stick or touchpad |
| Select / cancel | Release group button / modifier |
| Open editor | Off grip + Main A/X, or ImGui VR Helper |
| Edit | Helper pointer and trigger |

## Appearance

Choose a theme, then customize colors, frames, icon size, text size and spacing. Themes include Skyrim silver, Dear Diary Dark, Edge, Minimal, Norden, Nordic, Oathvein, Untarnished and Vel'dun. Item icon sets and installed ImGui keycaps are selected independently.

Group layouts include rows, columns, crosses, diamonds and circles. Selectors support horizontal/vertical strips, arcs, rings and lists. **Position** lets you drag outlined elements, adjust X/Y and snap to a grid. Hold Shift for free movement.

**Appearance → Equipment** shows the right hand, left hand, selected consumable and power/shout. Two-handed equipment uses one entry. Choose list, circle, diamond, square or text layouts, with independent scale, spacing and placement. Quantities and shout timers appear on icons; spent powers dim.

Groups and equipment have independent visibility settings. Choose always visible, idle fading or hiding where available, and configure combat/drawn-weapon visibility. QuickLoot receives input while the HUD follows its normal visibility settings. Full menus take input and hide the HUD.

See [Theme and icon packs](THEMES.md) for customization files.

## Settings and languages

Changes save automatically. **Undo** and **Revert session** restore editor states; **Presets** saves appearance or a complete setup. The editor remembers its current tab during the game session.

Choose a menu language under **Appearance → Style**. Translation files belong in `Data/Quickslot/translations`; the English reference and format guide are included. Fonts support UTF-8 names with Cyrillic, Greek and CJK fallbacks. Theme fonts, including Futura, take priority. Install extra fallback fonts in `Data/Quickslot/fonts/fallback` and restart.

MO2 stores generated settings in Overwrite or the configured output mod. Keep these files when updating. Close Skyrim before replacing the DLL or atlas, then restart.

## Builds and support

[Releases](https://github.com/opteron-x86/quickslot-hotkeys/releases) contain the mod ZIP, matching source, symbols, menu API and checksums. Development packages are under **Artifacts** in successful [build runs](https://github.com/opteron-x86/quickslot-hotkeys/actions/workflows/build.yml). See [Building](BUILDING.md) and [Releasing](docs/RELEASING.md) for development commands.

For bug reports, include reproduction steps, the game version and `Quickslot.log` from `Documents/My Games/Skyrim Special Edition/SKSE` (`Skyrim VR` for VR). Include a crash log for crashes.

The [menu API](src/API/README.md) supports editor opening, closing, state queries and shortcut control.

Item artwork was supplied by the mod author. Frame/font credits and dependency notices are in [licenses](licenses). Dependency revisions are recorded in `vendor/dependencies.json`.
