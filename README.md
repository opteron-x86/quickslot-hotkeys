# Quickslot

[Downloads](https://github.com/opteron-x86/quickslot-hotkeys/releases) · [Development builds](https://github.com/opteron-x86/quickslot-hotkeys/actions/workflows/build.yml) · [Changelog](CHANGELOG.md) · [Building](BUILDING.md)

Quickslot organizes weapons, spells and consumables into hotkey groups. Hold a group key, browse, and release to select. Tap to equip or use the remembered entry.

## Install

Install the main ZIP through Mod Organizer 2 or Vortex and launch through SKSE. The `SKSE` and `Quickslot` folders belong in Skyrim's `Data` folder. Open the editor in a loaded game with **Shift + `\`**.

Requirements:

- [SKSE](https://skse.silverlock.org/) matching your Skyrim executable.
- [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444) matching your game version.
- [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604).
- [Inventory Interface Information Injector (I4) 1.1.0+](https://www.nexusmods.com/skyrimspecialedition/mods/85702) for your Skyrim version.
- Microsoft Visual C++ Redistributable, x64.

**Skyrim 1.5.97:** Install Fuzzles' I4 port for this runtime.

VR uses SKSEVR, [VR Address Library](https://github.com/alandtse/skyrim_vr_address_library), [SkyUI VR](https://github.com/Odie/skyui-vr), and [ImGui VR Helper 1.9.4+](https://www.nexusmods.com/skyrimspecialedition/mods/183466). VR support is experimental.

Optional integrations include [PNG Icon Injector](https://github.com/rijosan87/PNGII) with icon API v1 for item artwork, [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) for its editor entry, ImGui Icons for keycaps, and Skyrim Souls RE for selection slowdown. Physical controller/headset checks are listed in [Testing](docs/TESTING.md).

## Groups

Add inventory items and spells in **Groups**. **New** creates combinations, smart potions, smart weapons, smart ammo, shields and empty-hand actions. Saved shortcuts appear in the searchable item list and can belong to several groups; editing one updates every linked group.

**Remove group** resets the group's contents, name, bindings, behavior and auto-sort setting; Undo restores it. **Add group** creates a fresh group with the lowest unused key from **1–8**, skipping the editor and consumable shortcuts, and available default controller/VR bindings. If no default binding is free, it stays unbound. Existing groups keep their bindings. **Clear group** removes only its items.

**Sort A–Z** sorts the current group's items by name, ignoring case. **Auto-sort when adding** sorts it now and after future additions, including hotkey captures and moves into the group. Both preserve the remembered item and support Undo in the editor; automatic sorting starts disabled. You can still reorder items manually.

A combination has a searchable picker for each hand. Choose inventory, spells or smart rules such as Best Sword + Best Mace or Best Axe + Best Shield. **Both hands** copies a spell; **Use equipped** captures the current pair.

Choose a behavior for each group:

- **Hold to browse:** tap equips or uses the remembered entry; hold, browse and release chooses another.
- **Tap to cycle:** the first press when returning to a group recalls its selection. Further presses advance and equip.
- **Tap to cycle / hold to browse:** tap and release to recall or advance. Hold to open the selector, browse in either direction and release to select the highlighted entry.

**Keep the other hand equipped while cycling**, under **Controls → General**, starts enabled. Tap-to-cycle skips one-handed weapon copies held in the opposite hand. Smart weapons choose the strongest eligible copy. This applies to manual and Favorites-linked groups. Browsing hand actions can move a weapon between hands; turn the option off to allow those transfers during cycling too.

**Controls → General → Timing** sets the **Hold delay** that separates a tap from a hold (100–800 ms). **Cycle confirm delay** gives tapped cycling a selection window: each tap shows the next choice in the selector, your **Game speed while selecting** setting applies, and the last choice equips once when the delay ends. The cancel binding closes the window with your equipment unchanged. At zero, every tap equips.

Groups containing consumables start with **Consumables → Tap to use**. Browsing a potion readies it; the next tap uses one dose. Choose **Use selected consumable action** to select potions, food, poisons and smart potions with the group binding, then use a separate binding to consume one dose. This setting enables every group behavior, including tap-to-cycle with hold-to-browse. Equipment in mixed groups equips normally.

Bind **Use selected consumable** under **Controls → Keyboard** or **Controls → Controller**; VR has its own binding. The action starts unbound. Select a consumable through a group first. The action uses the last explicitly selected consumable across groups, including after selecting equipment. The equipment widget shows that selection. Depletion preserves the selection for restocking; smart potions choose a matching potion when used. Poison use opens Skyrim’s confirmation. Further doses require a new press after the current use finishes. Bindings, group behavior and the selected consumable persist with saves and complete presets.

Powers and shouts support activation from the group hotkey, with configurable shout charge.

Smart weapons rank by physical damage, including tempering and player modifiers. Smart shields rank by armor; smart ammo ranks by damage. Smart potions offer strongest, weakest and best fit for missing health, magicka or stamina. Regeneration potions offer strongest or weakest.

Weapons, armor and shields follow the captured copy. Torches and consumables follow inventory stock of their saved type. Unavailable entries remain in the group for retrieval or restocking.

In the group editor, open the **Manual** dropdown and check the Favorites categories to include. For example, select **Favorite weapons** and **Favorite shields / torches** to cycle through both with one binding. Categories also include spells, potions, poisons, food, ammo, scrolls and powers/shouts. **All Favorites** selects every category; clear individual checkboxes to narrow the list. Clearing every checkbox leaves an empty Favorites group. Generated groups show all matching entries once each in alphabetical order and use a configurable default hand, subject to normal equipment restrictions. Manual groups retain their 16-item limit.

**Exclude** controls a favorite’s visibility in this group. Each group supports 128 exclusions; **Reset exclusions** shows them again. Switch to **Manual** to return to your manually added shortcuts and remembered selection. Source settings, exclusions and the remembered favorite are saved with the character and complete presets. Depleted or unfavorited entries disappear; restocking a still-favorited item brings it back. The list refreshes on inventory/menu changes and approximately once per second, cancelling a pending selection if membership changes. If several identical weapons share one favorited stack, equip one in Skyrim first so Quickslot can identify that copy.

## Controls

Bindings are configurable under **Controls**. Up to eight groups are available; keyboard groups start with **1**, **2** and **3**, and controller groups start with the four D-pad directions.

To clear a group's keyboard hotkey, click **Clear**, or click its binding and press **Esc**. Assigning another group's key swaps their bindings in one step. Removing a group releases its hotkey; cleared bindings remain unassigned after saving and loading.

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
| Editor tabs and pages | Mouse | LB / RB change tabs; LT / RT change pages |
| Editor pointer | Mouse | Right stick; A / Cross clicks or drags |
| Close editor | Escape / Close | Start / Options |

Left/Right browsing and Ctrl + Left/Right reordering wrap at both ends. Activating a name or search field with a controller opens the on-screen keyboard. Selecting an inventory row focuses its Add button for the next confirmation.

**Ctrl + group key** adds the equipped right hand or highlighted Inventory/Magic item; **Ctrl + Shift + group key** adds the left hand. **Shift + group key** unequips the remembered entry; **Alt + group key** removes its shortcut. While browsing, **Ctrl + destination group key** moves the highlighted shortcut to that group.

**Return to previous selection** re-equips the weapon, spell or combination you selected before your current one; using it again swaps back. Bind it under **Controls → Keyboard**. With **Controls → General → Double-tap a group to return to the previous selection**, two quick taps of the group you are on do the same from keyboard or controller. Double tap applies to hold-to-browse groups; cycling groups keep advancing on each tap.

All keyboard actions are configured under **Controls → Keyboard → Remap actions…**. For a group editing action, choose a modifier combination or **None** to disable it. For browsing, selected-item and cancel keys, click the binding to assign it, or use **Clear** (Escape during capture also clears). Assigning an occupied binding swaps the actions. **Reset keyboard actions** restores the default bindings. Upgrades preserve your bindings and disabled actions.

To use Left Alt for walking, set **Remove group shortcut** to **None** or another modifier, then enable **Allow group selection with unused modifiers**. This setting lets unused modifier chords select groups. It starts disabled.

The right/left capture bindings apply during gameplay and in Inventory/Magic. To disable capture only in those menus, turn off **Controls → General → Add from Inventory / Magic**; gameplay capture stays available.

While browsing, moving and reordering reserve their assigned modifiers. Set both actions to **None** to release those modifiers; capture bindings have independent settings.

Controller modifiers are configured per binding. Hold the modifier first; release the group button to select. Releasing the modifier cancels browsing. Other assigned chords, such as B + D-pad, stay available to other mods. **Modifier buttons** controls which held buttons suppress unmodified group bindings.

Choose **Modifier** and **Button** in either order when editing a controller or VR binding. A complete, valid combination becomes active immediately. During an unfinished edit, the current binding stays active. Selecting **None** for **Button** clears the binding.

Under **Controls → Controller → Browsing actions**, remap cancel, unequip, previous and next, including optional modifier chords. **Enable previous / next buttons** controls the last two actions. For example, assign X as a group modifier and Y to **Unequip highlighted entry**.

XInput and native PS4 input are supported. Steam Input can supply XInput. Skyrim must recognize the controller first.

While holding a group, optional **Equip in right/left hand** actions equip the highlighted item immediately for this use. Configure keys and mouse buttons in **Controls → Keyboard → Remap actions…**, or controller actions in **Controls → Controller → Browsing actions**. **Use mouse buttons for hands** assigns left mouse to the right hand and right mouse to the left hand, leaving keyboard cancellation available. **Use bumpers for hands** assigns RB/LB and replaces bumper browsing; the stick still browses. Shields and torches use the left hand; two-handed equipment uses both hands. Saved combinations equip their configured hands.

## VR controls

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

New installations start on the **Automatic** theme, which matches Dear Diary Dark Mode, Edge, Norden, Nordic, Oathvein, Untarnished or Vel'dun when that interface is installed and uses its font from `Interface/ImGuiIcons/Fonts`. Choose any theme and **Font** under **Appearance → Theme**, then customize colors, frames, icon size, text size and spacing. The editor follows the selected theme. Themes include Skyrim silver, Dear Diary Dark, Edge, Minimal, Norden, Nordic, Oathvein, Untarnished and Vel'dun. Item artwork and installed ImGui keycaps are configured independently.

Use **Key icons** to switch between ImGui key artwork and plain hotkey labels on the frames.

Item icons use the artwork and colors assigned by your installed I4 packs. **Appearance → Theme → Icon color** offers **Original**, **Theme** and **White**. Smart shortcuts show the item currently chosen by their rule; combinations show both selected items. Empty-hand and unarmed actions use a gauntlet icon. When a rule has no matching item or its artwork is unavailable, Quickslot uses a SkyUI category icon.

[PNG Icon Injector (PNGII)](https://github.com/rijosan87/PNGII) can supply matching PNG and SVG artwork. Install a PNGII build with icon API v1 and your preferred packs. **Appearance → Theme → Use PNGII icons** starts enabled; matching artwork appears in the widget, browser, equipment display and editor preview. I4 supplies icons for the remaining items. **Original** preserves PNGII artwork colors, and **Theme** or **White** tint its silhouette. Restart Skyrim after changing icon packs.

Groups and equipment support horizontal, vertical, diamond, cross, circle and staggered-cross arrangements. **Cross layout**, **Diamond layout** and **Circle layout** apply matching arrangements and frame shapes. Selectors support horizontal/vertical strips, arcs, rings, lists and a wheel. On the **Wheel**, every item keeps its place around a hub: hold the group key or button, point with the browsing stick or the mouse, and release to select. The camera stays still while the mouse points; scrolling and previous / next still step. **Position** lets you drag outlined elements, adjust X/Y and snap to a grid. Hold Shift for free movement.

**Spacing** uses a 0–128 numeric gap for groups, selectors and equipment. Zero brings neighboring frames together; higher values open up the layout. Equipment names occupy their measured width, and spacing adds a gap after the visible content. Gaps scale with their display. In the preview, **Zoom** controls magnification and **Fit preview** fits the current arrangement into the panel.

**Appearance → Equipment** shows the right hand, left hand, selected consumable and power/shout. Two-handed equipment uses one entry. Choose list, circle, diamond, square or text styles, with independent arrangement, scale, spacing and placement. **Show equipment names** adds labels to circle, diamond and square tiles, with text above, below, left or right. Equipment layout presets show compact tiles. Quantities and shout timers appear on icons; spent powers dim. Enable **Show equipped ammunition** to add an entry with the equipped arrows or bolts and their current count. This option starts off and also supports text-only layouts. **Show bow ammunition** controls the count on bow and crossbow icons.

Equipped entries carry a marker; set its size and color with **Equipped marker size** and **Custom equipped color** under **Appearance → Theme**. Shout cooldowns drain as a clockwise shade over the icon. The widget eases in, the selector opens and closes smoothly, and a group pulses when its item equips or a dose is used; **Appearance → Selector → Animate opening and fading** controls this motion. **Controls → General → Interface sounds** plays Skyrim's menu sounds as the selector opens, moves and cancels.

Poisoned weapons display a green droplet. Bows and crossbows display the equipped arrow or bolt count. These indicators also appear on smart weapon shortcuts. Both indicators can be disabled under **Appearance → Widget**.

Groups and equipment have independent visibility settings. Choose always visible, idle fading or hiding where available, and configure combat/drawn-weapon visibility. QuickLoot receives input while the HUD follows its normal visibility settings. Full menus take input and hide the HUD.

Scenes that take control of the player, including OStim, receive keyboard and controller input while Quickslot's HUD is hidden. Quickslot cancels any active selection and resumes when the scene returns control to the player.

See [Theme and icon packs](THEMES.md) for customization files.

QuickLoot input is optional under **Controls → General**. Enable keyboard/mouse, controller, or both; both start disabled. While browsing a Quickslot group, captured navigation goes to Quickslot and other input still reaches QuickLoot. Other menus and the editor retain their normal restrictions.

## Settings and languages

Changes save automatically to a settings file shared by every character. Enable **Presets → Keep settings with this character** to store controls, group names and appearance in that character's saves instead; those changes are stored when you save the game, and turning the option off makes the current settings the shared ones. **Undo** and **Revert session** restore editor states; **Presets** saves appearance or a complete setup, and **Delete preset** removes a saved file. The editor remembers its current tab during the game session.

Choose a menu language under **Appearance → General**. Translation files belong in `Data/Quickslot/translations`; the English reference and format guide are included. Fonts support UTF-8 names with Cyrillic, Greek and CJK fallbacks. Theme fonts, including Futura, take priority. Install extra fallback fonts in `Data/Quickslot/fonts/fallback` and restart.

MO2 stores generated settings in Overwrite or the configured output mod. Keep these files when updating. Close Skyrim before installing an update, then restart.

## Builds and support

[Releases](https://github.com/opteron-x86/quickslot-hotkeys/releases) contain the mod ZIP, matching source, symbols, menu API and checksums. Development packages are under **Artifacts** in successful [build runs](https://github.com/opteron-x86/quickslot-hotkeys/actions/workflows/build.yml). See [Building](BUILDING.md) and [Releasing](docs/RELEASING.md) for development commands.

For bug reports, include reproduction steps, the game version and `Quickslot.log` from `Documents/My Games/Skyrim Special Edition/SKSE` (`Skyrim VR` for VR). Include a crash log for crashes.

The [API](src/API/README.md) supports editor opening, closing, state queries and shortcut control, plus group taps and returning to the previous selection from other mods.

IME authors can find text-routing instructions in the [integration guide](src/API/README.md#ime-text-input).

Item artwork was supplied by the mod author. Frame/font credits and dependency notices are in [licenses](licenses). Dependency revisions are recorded in `vendor/dependencies.json`.
