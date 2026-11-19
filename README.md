# Quickslot

[Downloads](https://github.com/opteron-x86/quickslot-hotkeys/releases) · [Development builds](https://github.com/opteron-x86/quickslot-hotkeys/actions/workflows/build.yml) · [Changelog](CHANGELOG.md) · [Building](BUILDING.md)

Quickslot organizes weapons, spells and consumables into hotkey groups. Hold a group key, browse, and release to select. Tap to equip or use the remembered entry.

## Quick start

1. Install Quickslot with its requirements and load a save. Your character starts with four groups that follow its Skyrim Favorites: **Weapons**, **Spells**, **Potions** and **Powers & shouts**, on keys **1**–**4** and the D-pad.
2. Favorite items in Skyrim's Inventory or Magic menu. Each one appears in its group.
3. Tap a group key to equip the item you used last. Hold the key to open the selector, browse with the mouse wheel, arrow keys or right stick, and release to select.
4. While the selector is open, press left mouse or RT to put the highlighted item in your right hand, and right mouse or LT for your left. Press the same button again to take it out.
5. Press **Shift + `\`**, or hold D-pad Up and press Start, to open the editor. There you build your own groups, choose how each group's key behaves, and change the look under **Appearance**.

## Install

Install the main ZIP through Mod Organizer 2 or Vortex and launch through SKSE. The `SKSE` and `Quickslot` folders belong in Skyrim's `Data` folder. Open the editor in a loaded game with **Shift + `\`**. Until you open it for the first time, the widget shows the shortcut that opens it.

Requirements:

- [SKSE](https://skse.silverlock.org/) matching your Skyrim executable.
- [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444) matching your game version.
- [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604).
- Microsoft Visual C++ Redistributable, x64.

[Inventory Interface Information Injector (I4) 1.1.0+](https://www.nexusmods.com/skyrimspecialedition/mods/85702) is optional. With it, items show the artwork your I4 packs assign; without it, each item shows SkyUI's category icon for its type. On Skyrim 1.5.97, use Fuzzles' I4 port for that runtime.

VR uses SKSEVR, [VR Address Library](https://github.com/alandtse/skyrim_vr_address_library), [SkyUI VR](https://github.com/Odie/skyui-vr), and [ImGui VR Helper 1.9.4+](https://www.nexusmods.com/skyrimspecialedition/mods/183466). VR support is experimental.

Optional integrations include [PNG Icon Injector](https://github.com/rijosan87/PNGII) with icon API v1 for item artwork, [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) for its editor entry, ImGui Icons for keycaps, and Skyrim Souls RE for selection slowdown. Physical controller/headset checks are listed in [Testing](docs/TESTING.md).

## Groups

### Starting groups

A character new to Quickslot starts with four groups that follow its Skyrim Favorites: **Weapons** (weapons, shields and torches), **Spells** (spells and scrolls), **Potions** (potions, food and poisons) and **Powers & shouts**. Favorite an item in Skyrim and it appears in its group. Each group can switch to a list of your own under **Contents**.

### Building groups

Add inventory items and spells in **Groups**. **New** creates combinations, smart potions, smart weapons, smart ammo, shields and empty-hand actions. Saved shortcuts appear in the searchable item list and can belong to several groups; editing one updates every linked group.

**Remove group** resets the group's contents, name, bindings, behavior and auto-sort setting; Undo restores it. **Add group** creates a fresh group with the lowest unused key from **1–8**, skipping the editor and consumable shortcuts, and available default controller/VR bindings. If no default binding is free, it stays unbound. Existing groups keep their bindings. **Clear group** removes only its items.

**Sort A–Z** sorts the current group's items by name, ignoring case. **Auto-sort when adding** sorts it now and after future additions, including hotkey captures and moves into the group. Both preserve the remembered item and support Undo in the editor; automatic sorting starts disabled. You can still reorder items manually.

A combination has a searchable picker for each hand. Choose inventory, spells or smart rules such as Best Sword + Best Mace or Best Axe + Best Shield. **Both hands** copies a spell; **Use equipped** captures the current pair.

### Group behavior

Choose what a tap of each group's key does under **Tap**:

- **Equip the last item:** tap equips or uses the remembered entry; hold, browse and release chooses another.
- **Step to the next item:** the first press when returning to a group recalls its selection. Further presses advance and equip. With **Hold to browse** on, hold to open the selector, browse in either direction and release to select the highlighted entry; with it off, every press steps at once.

**Keep the other hand equipped while cycling**, under **Controls → General**, starts enabled. A group that steps to the next item skips one-handed weapon copies held in the opposite hand. Smart weapons choose the strongest eligible copy. This applies to manual and Favorites-linked groups. Browsing hand actions can move a weapon between hands; turn the option off to allow those transfers during cycling too.

**Controls → General → Timing** sets the **Hold delay** that separates a tap from a hold (100–800 ms). **Cycle confirm delay** gives tapped cycling a selection window: each tap shows the next choice in the selector, your **Game speed while selecting** setting applies, and the last choice equips once when the delay ends. The cancel binding closes the window with your equipment unchanged. At zero, every tap equips.

### Consumables

In a group that equips its last item, **Consumables → Tap to use** is the starting choice: browsing a potion readies it and the next tap uses one dose. Choose **Use selected consumable action** to select potions, food, poisons and smart potions with the group binding, then use a separate binding to consume one dose. A group that steps to the next item always selects its consumables this way, and shows a reminder while the use action has no binding. Equipment in mixed groups equips normally.

Bind **Use selected consumable** under **Controls → Keyboard** or **Controls → Controller**; VR has its own binding. The action starts unbound. Select a consumable through a group first. The action uses the last explicitly selected consumable across groups, including after selecting equipment. The equipment widget shows that selection. Depletion preserves the selection for restocking; smart potions choose a matching potion when used. Poison use opens Skyrim’s confirmation. Further doses require a new press after the current use finishes. Bindings, group behavior and the selected consumable persist with saves and complete presets.

### Powers, smart shortcuts and item identity

Powers and shouts support activation from the group hotkey, with configurable shout charge.

Smart weapons rank by physical damage, including tempering and player modifiers. Smart shields rank by armor; smart ammo ranks by damage. Smart potions offer strongest, weakest and best fit for missing health, magicka or stamina. Regeneration potions offer strongest or weakest.

Weapons, armor and shields follow the captured copy. Torches and consumables follow inventory stock of their saved type. Unavailable entries remain in the group for retrieval or restocking.

### Favorites groups

In the group editor, open the **Manual** dropdown and check the Favorites categories to include. For example, select **Favorite weapons** and **Favorite shields / torches** to cycle through both with one binding. Categories also include spells, potions, poisons, food, ammo, scrolls and powers/shouts. **All Favorites** selects every category; clear individual checkboxes to narrow the list. Clearing every checkbox leaves an empty Favorites group. Generated groups show all matching entries once each in alphabetical order and use a configurable default hand, subject to normal equipment restrictions. Manual groups retain their 16-item limit.

**Exclude** controls a favorite’s visibility in this group. Each group supports 128 exclusions; **Reset exclusions** shows them again. Switch to **Manual** to return to your manually added shortcuts and remembered selection. Source settings, exclusions and the remembered favorite are saved with the character and complete presets. Depleted or unfavorited entries disappear; restocking a still-favorited item brings it back. The list refreshes on inventory/menu changes and approximately once per second, cancelling a pending selection if membership changes. If several identical weapons share one favorited stack, equip one in Skyrim first so Quickslot can identify that copy.

## Controls

### Bindings

Bindings are configurable under **Controls**. Up to eight groups are available; keyboard groups start with **1**, **2** and **3**, and controller groups start with the four D-pad directions.

To clear a group's keyboard hotkey, click **Clear**, or click its binding and press **Esc**. Assigning another group's key swaps their bindings in one step. Removing a group releases its hotkey; cleared bindings remain unassigned after saving and loading.

| Action | Keyboard / mouse | Controller |
| --- | --- | --- |
| Equip or use remembered entry | Tap group key | Tap group button |
| Browse | Hold group key + wheel or Left/Right | Hold group button + right stick or LB/RB |
| Select | Release group key | Release group button |
| Choose a hand, or take the item out of it | Left mouse (right hand) / right mouse (left hand) | RT (right hand) / LT (left hand) |
| Reorder while browsing | Ctrl + Left/Right | Editor controls |
| Cancel | Escape | B / Circle |
| Unequip highlighted entry | Backspace while browsing | X / Square |
| Remove highlighted shortcut | Delete while browsing | Editor controls |
| Open editor | Shift + `\` | Hold D-pad Up, then Start / Options |
| Navigate editor | Mouse / keyboard | D-pad / left stick; A / Cross confirms |
| Editor tabs and pages | Mouse | LB / RB change tabs; LT / RT change pages |
| Editor pointer | Mouse | Right stick; A / Cross clicks or drags |
| Close editor | Escape / Close | Start / Options |

Left/Right browsing and Ctrl + Left/Right reordering wrap at both ends. Activating a name or search field with a controller opens the on-screen keyboard. Selecting an inventory row focuses its Add button for the next confirmation.

### Editing from the keyboard

In Inventory and Magic, **Ctrl + group key** adds the highlighted item to that group for the right hand and **Ctrl + Shift + group key** adds it for the left. While browsing, **Ctrl + destination group key** moves the highlighted shortcut to that group.

During gameplay, a group key held with Ctrl, Shift or Alt selects the group, so sprinting, sneaking and walking keys stay out of the way. Turn on **Controls → Keyboard → Use editing shortcuts during gameplay** to edit groups with those chords outside menus: **Ctrl + group key** adds the equipped right hand, **Ctrl + Shift + group key** adds the left hand, **Shift + group key** unequips the remembered entry and **Alt + group key** removes its shortcut.

**Return to previous selection** re-equips the weapon, spell or combination you selected before your current one; using it again swaps back. Bind it under **Controls → Keyboard**. With **Controls → General → Double-tap a group to return to the previous selection**, two quick taps of the group you are on do the same from keyboard or controller. Double tap applies to hold-to-browse groups; cycling groups keep advancing on each tap.

**Controls → Keyboard** lists every keyboard and mouse action with its binding: group keys, the editor and other keys, the actions available while the selector is open, and the group-key modifiers. For a group editing action, choose a modifier combination or **None** to disable it. For browsing, selected-item and cancel keys, click the binding to assign it, or use **Clear** (Escape during capture also clears). Assigning an occupied binding swaps the actions. **Reset keyboard actions** restores the default bindings. Upgrades preserve your bindings and disabled actions.

With gameplay editing shortcuts on, to use Left Alt for walking, set **Remove group shortcut** to **None** or another modifier, then enable **Allow group selection with unused modifiers**. This setting lets unused modifier chords select groups. It starts disabled.

The right/left capture bindings apply in Inventory/Magic, and during gameplay while **Use editing shortcuts during gameplay** is on. To disable capture only in those menus, turn off **Controls → General → Add from Inventory / Magic**.

While browsing, moving and reordering reserve their assigned modifiers. Set both actions to **None** to release those modifiers; capture bindings have independent settings.

### Controller

Controller modifiers are configured per binding. Hold the modifier first; release the group button to select. Releasing the modifier cancels browsing. Other assigned chords, such as B + D-pad, stay available to other mods. **Modifier buttons** controls which held buttons suppress unmodified group bindings. The triggers start outside that list, so group buttons work while you block or attack.

Choose **Modifier** and **Button** in either order when editing a controller or VR binding. A complete, valid combination becomes active immediately. During an unfinished edit, the current binding stays active. Selecting **None** for **Button** clears the binding.

Under **Controls → Controller → While the selector is open**, remap cancel, unequip, previous, next and the two hand buttons, including optional modifier chords. **Enable previous / next buttons** controls the last two actions. For example, assign X as a group modifier and Y to **Unequip highlighted entry**.

XInput and native PS4 input are supported. Steam Input can supply XInput. Skyrim must recognize the controller first.

### Choosing a hand

While the selector is open, a hand button puts the highlighted item in that hand: the left mouse button or RT for the right hand, the right mouse button or LT for the left. Pressing the button of the hand an item is already in takes it out. Dual wielding is two presses: point at one item and press a hand button, then the other. An item returns to the hand you last put it in when you tap its group; in a Favorites group that lasts until you load a save. Shields and torches go to the left hand from either button, two-handed equipment uses both hands, and potions, powers and combinations are selected as a release would. Releasing the group key uses the item's usual hand.

Settings from earlier versions keep their bindings. **Use mouse buttons for hands** under **Controls → Keyboard** and **Use triggers for hands** under **Controls → Controller** assign the hand buttons in one step while a hand button is unassigned; the mouse preset moves cancel to Escape. Keyboard keys can be assigned to the hand actions as well.

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

### Pages and sizes

**Appearance** has a page for each part of the HUD: **Theme**, **Widget** (the group tiles), **Selector**, **Equipment** and **General**. The element pages share the same sections — Layout, Size, Placement, Content and Visibility. Each slider shows its value in percent, pixels or seconds, has **−** and **+** buttons that move one step, and a mark that restores the default; Ctrl+click a slider to type a value. **Reset page** restores one page and **General → Reset all appearance** restores them all. Each page starts with its main settings: arrangement, shape, scale, opacity, placement and what to show. **Show advanced settings**, beside the page buttons, adds spacing, text sizes, exact placement, idle fading and the less common display options; settings from earlier versions start with it on.

The widget, selector and equipment display each have their own **Scale** (25–300%), **Text size** and **Opacity**, and in the Free arrangement each tile has its own size. Changing one never resizes another.

### Themes and icons

New installations show diamond tiles with an arc selector and start on the **Automatic** theme, which matches Dear Diary Dark Mode, Edge, Norden, Nordic, Oathvein, Untarnished or Vel'dun when that interface is installed and uses its font from `Interface/ImGuiIcons/Fonts`. Choose any theme and **Font** under **Appearance → Theme**, then customize colors, frames, icon size, text size and spacing. The editor follows the selected theme. Themes include Skyrim silver, Dear Diary Dark, Edge, Minimal, Norden, Nordic, Oathvein, Untarnished and Vel'dun. Item artwork and installed ImGui keycaps are configured independently.

Use **Key icons** to switch between ImGui key artwork and plain hotkey labels on the frames.

**Appearance → Theme** also sets one **Icon size** for every item icon, a **Background opacity** that fades the inside of frames and keeps their borders, and a **Custom text color** and **Custom selection color**. In the **Groups** tab, **Color** gives a group its own color for its highlights, its name and the selector while that group is open; group colors are saved with the group setup.

With I4 installed, item icons use the artwork and colors assigned by your I4 packs; without it, each item shows SkyUI's category icon for its type. **Appearance → Theme → Icon color** offers **Original**, **Theme** and **White**. Smart shortcuts show the item currently chosen by their rule; combinations show both selected items. Empty-hand and unarmed actions use a gauntlet icon. When a rule has no matching item or its artwork is unavailable, Quickslot uses a SkyUI category icon.

[PNG Icon Injector (PNGII)](https://github.com/rijosan87/PNGII) can supply matching PNG and SVG artwork. Install a PNGII build with icon API v1 and your preferred packs. **Appearance → Theme → Use PNGII icons** starts enabled; matching artwork appears in the widget, browser, equipment display and editor preview. I4 supplies icons for the remaining items. **Original** preserves PNGII artwork colors, and **Theme** or **White** tint its silhouette. Restart Skyrim after changing icon packs.

### Layout and placement

Groups and equipment support horizontal, vertical, diamond, cross, circle and staggered-cross arrangements; groups also have a **Grid** with a set number of **Columns**. **Card width** sets the room for names on card tiles, **Mirror cards** puts their icon on the right, and **Tile size** resizes circle, square and diamond tiles while their names keep the text size. Strip, arc and list selectors show 3, 5, 7 or 9 **Visible items**, and the wheel has a **Wheel size**. The **Cards**, **Cross**, **Diamond**, **Circle** and **Wheel** presets under **Presets** apply matching arrangements and frame shapes.

Selectors support horizontal/vertical strips, arcs, rings, lists and a wheel. On the **Wheel**, every item keeps its place around a hub: hold the group key or button, point with the browsing stick or the mouse, and release to select. The camera stays still while the mouse points; scrolling and previous / next still step. **Wheel style** chooses **Tiles**, which uses the selector's item shape, or **Segments**, a ring of wedges with the pointed item's name and hand in the hub. While a selector is open its group's tile is outlined, and the selector's last line shows the group's name and the entry's position; **Show group and position** turns that line off.

**Wheel size** is the wheel's outer radius. The ring, its icons and the center grow with it, and a wheel with many entries grows until each has room for its icon. The **Wheel** section shapes a segmented wheel: **Ring thickness** (at 100% the wedges meet in the center), the **Gap** between wedges, where the **Item name** goes (in the center, below the wheel or hidden), the pointed wedge's **Highlight** (fill and outline, fill, outline, or a rim of the accent color), **Raise the pointed segment**, **Ring opacity** and **Item names on segments**. Any wheel can turn off **Show center circle** and **Show pointer**, and **Rotation** turns its first item away from the top.

**Free** under **Appearance → Widget → Arrangement** gives each group's tile its own position and size: put two groups on one side of the screen and two on the other, or make one tile larger. Choosing Free starts every tile where the previous arrangement had it. Pick a tile under **Placement** to set its **Tile size**, anchor and offset, or drag and resize each tile in **Position**; **Reset tile positions** lines them up again. A tile's position belongs to its group, so reordering groups keeps it, and a group added later sits beside the last tile. A docked selector opens beside the tile in use, clear of the other tiles, and the item name and notices appear by that tile.

Each element's **Placement** section holds its anchor and its X/Y offset in pixels. The anchor is one of nine points of the screen — corners, edge centers and the middle — and the element rests its matching point there, so it keeps its distance from that point when its size or the resolution changes. **Position** opens the full-screen view. **Element** chooses what to place: the widget or, in the Free arrangement, each tile, then the selector and the equipment display. Its **Scale** slider and corner handle resize only that element, and **Appearance** in the panel holds that element's main look settings. Drag an outlined element, drag the handle on its corner to resize it around its anchor, move it one pixel with the arrow keys (ten with Shift) or with LB + D-pad, and use **Center horizontally** and **Center vertically**. Dragging picks the nearest anchor; choose one in the grid to keep it. The grid snaps edges and centers, and Shift moves freely. **Screen area** anchors the HUD to the full screen or to a centered 16:9, 2:1 or 21:9 area, which keeps it within the central part of an ultrawide screen.

**Spacing** uses a 0–128 numeric gap for groups, selectors and equipment. Zero brings neighboring frames together; higher values open up the layout. Equipment names occupy their measured width, and spacing adds a gap after the visible content. Gaps scale with their display. In the preview, **Zoom** controls magnification and **Fit preview** fits the current arrangement into the panel.

### Equipment display

**Appearance → Equipment** shows the right hand, left hand, selected consumable and power/shout. Two-handed equipment uses one entry. Choose list, circle, diamond, square or text styles, with independent arrangement, scale, spacing and placement. **Show equipment names** adds labels to circle, diamond and square tiles, with text above, below, left or right. The Cross, Diamond, Circle and Wheel presets show compact tiles. Quantities and shout timers appear on icons; spent powers dim. Enable **Show equipped ammunition** to add an entry with the equipped arrows or bolts and their current count. This option starts off and also supports text-only layouts. **Show bow ammunition** controls the count on bow and crossbow icons.

### Markers, visibility and motion

Equipped entries carry a diamond marker or their hand letters, and an arrowhead marks the consumable the Use key takes. After a selection, the item's name appears beside its group's tile for a moment; cards that show item names carry it themselves. Set the marker's size and color with **Equipped marker size** and **Custom equipped color** under **Appearance → Theme**. **Equipped highlight** on the same page marks the whole tile of an equipped item with an outline, a tint or both in that color. Shout cooldowns drain as a clockwise shade over the icon. The widget eases in, the selector opens and closes smoothly, and a group pulses when its item equips, a dose is used, or its key is pressed for an item already in hand; **Appearance → General → Animate opening and fading** controls this motion. **Settings → Interface sounds** plays Skyrim's menu sounds as the selector opens, moves and cancels.

**Appearance → Theme → Enchantment charge** shows an enchanted weapon's remaining charge on its icon in the widget, the selector and the equipment display: **Fill the icon** keeps the icon's color up to the charge and dims the spent part above it, and **Soul gem** adds a small gem that fills with the charge and turns red when empty. While browsing, **Selector → Show enchantment charge** shows the highlighted weapon's charge as five diamonds on the line under its hand.

Poisoned weapons display a green droplet. Bows and crossbows display the equipped arrow or bolt count. These indicators also appear on smart weapon shortcuts. Both indicators can be disabled under **Appearance → Widget**.

Groups and equipment have independent visibility settings. Choose always visible, idle fading or hiding where available, and configure combat/drawn-weapon visibility. QuickLoot receives input while the HUD follows its normal visibility settings. Full menus take input and hide the HUD.

Scenes that take control of the player, including OStim, receive keyboard and controller input while Quickslot's HUD is hidden. Quickslot cancels any active selection and resumes when the scene returns control to the player.

See [Theme and icon packs](THEMES.md) for customization files.

QuickLoot input is optional under **Settings**. Enable keyboard/mouse, controller, or both; both start disabled. While browsing a Quickslot group, captured navigation goes to Quickslot and other input still reaches QuickLoot. Other menus and the editor retain their normal restrictions.

## Settings and languages

### Saving

Changes save automatically to a settings file shared by every character. Each group's name, bindings, place in the order, behavior and color belong to the character and are stored in its saves with the group's contents, when you save the game. Enable **Settings → Keep settings with this character** to store controls, appearance and preferences in that character's saves instead; those changes are stored when you save the game, and turning the option off makes the current settings the shared ones.

**Undo** and **Revert session** restore editor states; **Presets** lists the built-in presets and your own. **Default** restores the whole look; **Cards**, **Cross**, **Diamond**, **Circle** and **Wheel** rearrange the widget, selector and equipment display. **Cards** shows card tiles with a strip selector and an equipment list, and **Wheel** shows diamond tiles with a segmented wheel selector. Select a preset to see it in the preview, then choose what **Apply preset** takes from it: **Look**, **Controls** and, for a complete setup, **Groups**. Save the current settings under a name as appearance only or a complete setup with **Save preset**. Typing or selecting an existing preset's name selects that preset's own scope, and **Update preset** saves over it; **Rename** and **Duplicate** use the name you type, and **Delete preset** removes the file. **Current look** names the preset in use and shows when your settings have moved away from it.

Under **Settings → Input device presets**, choose a preset for **Controller** and for **Keyboard and mouse**: its look is applied a moment after you start using that device. Choose one for each device to switch back and forth. The editor remembers its current tab during the game session.

### Settings tab

**Settings** holds the preferences that stay with you: language, interface sounds, input device presets, QuickLoot input and the character option. **Settings → Reset Quickslot** returns every setting to its default and replaces the character's groups with groups that follow Skyrim Favorites, as a new installation shows a new character; presets stay, and Undo restores everything while the editor is open.

### Languages

Choose a menu language under **Settings**. Translation files belong in `Data/Quickslot/translations`; the English reference and format guide are included. Fonts support UTF-8 names with Cyrillic, Greek and CJK fallbacks. Theme fonts, including Futura, take priority. Install extra fallback fonts in `Data/Quickslot/fonts/fallback` and restart.

MO2 stores generated settings in Overwrite or the configured output mod. Keep these files when updating. Close Skyrim before installing an update, then restart.

## Builds and support

[Releases](https://github.com/opteron-x86/quickslot-hotkeys/releases) contain the mod ZIP, matching source, symbols, menu API and checksums. Development packages are under **Artifacts** in successful [build runs](https://github.com/opteron-x86/quickslot-hotkeys/actions/workflows/build.yml). See [Building](BUILDING.md) and [Releasing](docs/RELEASING.md) for development commands.

For bug reports, include reproduction steps, the game version and `Quickslot.log` from `Documents/My Games/Skyrim Special Edition/SKSE` (`Skyrim VR` for VR). Include a crash log for crashes.

The [API](src/API/README.md) supports editor opening, closing, state queries and shortcut control, plus group taps and returning to the previous selection from other mods.

IME authors can find text-routing instructions in the [integration guide](src/API/README.md#ime-text-input).

Item artwork was supplied by the mod author. Frame/font credits and dependency notices are in the [licenses](https://github.com/opteron-x86/quickslot-hotkeys/tree/master/licenses) folder on GitHub. Dependency revisions are recorded in `vendor/dependencies.json`.
