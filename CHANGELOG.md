# Changelog

## 0.16.0

Updating: group keys held with Ctrl, Shift or Alt now select the group during gameplay. Turn on **Controls → Keyboard → Use editing shortcuts during gameplay** to keep adding, unequipping and removing with those chords outside menus.

- **Use editing shortcuts during gameplay**, under **Controls → Keyboard**, controls the Ctrl, Shift and Alt group-key chords outside menus. It starts off. Adding from Inventory and Magic with Ctrl and Ctrl + Shift stays available.
- Until the editor has been opened, the widget shows the shortcut that opens it. An empty group's selector shows how to add items.
- The **Groups** tab labels the **Contents** and **Behavior** choices and explains them, **Items** and **New** on hover.
- Typing or selecting an existing preset's name sets the save scope to that preset's own. Choosing **Appearance only** for a complete preset shows that its groups will be removed. **Export preset** is now **Save preset**.
- A full group reports "This group is full (16 items)" from gameplay and from Inventory and Magic.
- New installations, the **Default** preset and the reset buttons show diamond tiles with an arc selector. Existing settings keep their look.
- A character new to Quickslot starts with four groups that follow its Skyrim Favorites: Weapons, Spells, Potions and Powers & shouts.
- Each group's name, bindings, order, behavior and color are stored in the character's saves with its contents. Saves from earlier versions keep the setup from the settings file.
- **Tap** and **Hold to browse** replace the three group behaviors in the **Groups** tab. Existing groups keep their behavior.
- A group keeps its behavior when a potion, food or poison is added. In a group that steps to the next item, taps select consumables and **Use selected consumable** uses them; the editor and the HUD show a reminder while that action has no binding.
- A favorite that runs out stays the group's remembered item and is selected again when it is restocked.
- While the selector is open, a hand button puts the highlighted item in that hand, and pressing the button of the hand it is already in takes it out. New installations use the left and right mouse buttons and RT and LT; Escape cancels. **Use mouse buttons for hands** and **Use triggers for hands** assign them on earlier settings.
- An item returns to the hand it was last put in. A shield or torch goes to the left hand from either hand button, and a potion, power or combination is selected.
- **Controls → Keyboard** and **Controls → Controller** list every action with its binding on one page.
- On new installations a held trigger no longer keeps group buttons from Quickslot, so groups work while blocking or attacking. **Modifier buttons** changes this.
- A **Settings** tab holds language, interface sounds, input device presets, QuickLoot input and **Keep settings with this character**.
- **Settings → Reset Quickslot** returns every setting to its default and starts the character's groups from Skyrim Favorites. Presets stay and Undo restores everything while the editor is open.
- Appearance pages start with their main settings. **Show advanced settings** adds spacing, text sizes, exact placement, idle fading and the less common display options; earlier settings start with it on.
- A group's tile pulses when its key is pressed for an item that is already equipped.
- **Free** under **Appearance → Widget → Arrangement** gives each group's tile its own position and size. Drag and resize tiles one at a time in **Position**, or set them under **Placement**. The selector, item name and notices gather around the tile in use.
- Positions saved by earlier versions as a share of the screen become an anchor and offset the first time the HUD draws, without moving.
- I4 is optional. Without it, each item shows SkyUI's category icon for its type.
- **Appearance → Theme → Icons → Item icons** chooses **I4** (then SkyUI), **SkyUI** or **PNGII** (then I4, then SkyUI). It replaces **Use PNGII icons**; settings with PNGII icons on start on PNGII, and settings with **Use I4 icons** off start on SkyUI.
- Every built-in look shows the equipment display as a vertical list at the middle of the left edge.
- The README opens with a quick start.
- The English translation file lists every string the menus and HUD show, and no others.
- Built-in presets add **Cards**, with card tiles, a strip selector and an equipment list, and **Wheel**, with diamond tiles and a segmented wheel selector.
- The HUD shows the weapon enchantment charge meter while the Quickslot widget is on screen.
- **Match widget scale** is removed. The widget, selector, equipment display and each Free tile have their own **Scale**; a selector that matched the widget keeps that size.
- **Position** chooses what to place from one **Element** list, and its **Scale** slider and resize handle change only that element. An **Appearance** section in the panel holds the element's main look settings.
- Position no longer shows "-0 px", and its panel scrolls when it is taller than the screen.
- **Wheel size** replaces **Wheel radius**: the ring, its icons and the center grow together. Existing wheels keep their look.
- A segmented wheel adds **Ring thickness**, **Gap**, **Item name** (center, below or hidden), **Highlight** (fill and outline, fill, outline or rim), **Raise the pointed segment**, **Ring opacity** and **Item names on segments**.
- Every wheel adds **Show center circle**, **Show pointer** and **Rotation**.
- A segmented wheel grows when its entries need room for their icons.
- **Selector → Show enchantment charge** shows an enchanted weapon's charge as five diamonds on their own line under its hand.
- **Appearance → Theme → Enchantment charge** shows a weapon's charge on its icon in tiles, the selector and the equipment display: **Fill the icon** (the default) or a **Soul gem**.
- **Selector → Show group and position** turns off the selector's last line.
- The selector shows what the highlighted potion, food, poison or scroll does under its name. **Selector → Show item descriptions** turns this off.
- **Controls → General → First tap on another group equips its item** can be turned off so every tap steps to the next item.
- The selector's dock choice under **Placement** is named **Attach**.

## 0.15.1

- With the Automatic theme and ImGui Icons installed, the widget and selector use a readable text font. The installed UI font skips the Daedric, Dragon and other script faces and the icon font in `Interface/ImGuiIcons/Fonts`.

## 0.15.0

Updating: install the complete release package over the previous one. Settings, presets and positions carry over and keep their look. Translation packs keep working; new menu text shows in English until a pack adds it.

- The mod package holds the plugin, its settings file, fonts, translations, themes and the README and theme guide. License texts and credits are in the [licenses](https://github.com/opteron-x86/quickslot-hotkeys/tree/master/licenses) folder on GitHub.
- Loading a save keeps every readable group and shortcut when one entry is damaged, and reports the repair on screen. A group record written by a different Quickslot version stays in the save until you create new groups.
- Set a **Cycle confirm delay** under **Controls → General → Timing**. Tap-to-cycle groups then show each tapped choice in the selector, apply your **Game speed while selecting** setting (full speed, slow motion or pause), and equip the last choice once when the delay ends. Cancel during the delay with your cancel binding.
- Set the **Hold delay** that separates a tap from a hold under **Controls → General → Timing**.
- Inventory and Magic capture messages follow the selected language.
- **Show keys** has one control, under **Appearance → Widget**.
- **Appearance** has a page for each part of the HUD — **Theme**, **Widget**, **Selector**, **Equipment** and **General** — with the same Layout, Size, Placement, Content and Visibility sections on each. Language, interface sounds and animation are under **General**.
- The selector can have its own scale: turn off **Appearance → Selector → Match widget scale**. The widget, selector and equipment display each have their own **Text size** and **Opacity**, and **Scale** now ranges from 25% to 300%. Existing settings keep the sizes and opacity they had.
- Placement uses anchors. Each element holds on to one of nine points of the screen with an X/Y offset in pixels, so it keeps its place when its size, group count or the resolution changes. Existing positions stay where they are and become anchored the first time you move them.
- **Position** mode adds a resize handle on the selected element, one-pixel moves with the arrow keys (Shift for ten) or LB + D-pad, **Center horizontally** / **Center vertically**, an anchor picker and pixel X/Y controls. The same anchor and X/Y controls are on each element's page.
- **Screen area** anchors the HUD to a centered 16:9, 2:1 or 21:9 area for ultrawide screens.
- New layout options: a **Grid** arrangement with **Columns**, **Card width**, **Mirror cards**, **Tile size** for circle, square and diamond tiles, 3–9 **Visible items** in strip, arc and list selectors, and a **Wheel radius**.
- New color and size options under **Appearance → Theme**: one **Icon size** for every theme, **Background opacity** for the inside of frames, **Custom text color** and **Custom selection color**. Each group can have its own **Color** from the **Groups** tab.
- **Appearance → Selector → Wheel style** offers **Segments**: the wheel becomes a ring of wedges, one for each entry, with the pointed entry's name in the hub. It holds every entry of a Favorites group.
- Diamond tiles are symmetric on textured themes with the border on all four sides, show a larger icon, and keep hand letters, counts and markers inside the tile. Circle tiles keep hand letters and counts inside too.
- Cards show their key or button inside the card, at the end opposite the icon. A card with keys shown is 26 pixels wider.
- A newly selected item's name appears beside its own group's tile. Cards that show item names carry the name themselves.
- While the selector is open, its group's tile is outlined, and the selector's last line names the group with the entry's position.
- An arrowhead marks the consumable the Use key takes; equipped items keep the diamond.
- **Appearance → Theme → Equipped highlight** marks the tile of an equipped item with an outline, a tint or both, in the equipped color.
- Every slider shows its value in percent, pixels or seconds, has **−** and **+** buttons that move one step, and a reset mark that restores its default. **Reset page** restores a whole page. Each reset is one Undo step.
- **Presets → Keep settings with this character** stores controls, group names and appearance in that character's saves. Other characters keep the shared settings file.
- **Return to previous selection** re-equips the entry you selected before your current one, and pressing it again swaps back. Bind a key under **Controls → Keyboard**, or enable **Controls → General → Double-tap a group to return to the previous selection**.
- The API adds `Quickslot_GetGroupCount`, `Quickslot_TapGroup` and `Quickslot_SwapPrevious` for other mods. The API version stays 1, so existing launchers keep finding Quickslot. See the [API guide](src/API/README.md).
- The editor shows item artwork beside each inventory choice and group entry.
- In the editor, the controller's shoulder buttons change tabs and its triggers change pages within Appearance and Controls.
- **Presets** is a preset manager. Built-in **Default**, **Cross**, **Diamond** and **Circle** presets sit above your own; selecting or pointing at a preset shows it in a preview. **Rename**, **Duplicate**, **Update preset** and **Delete preset** manage your files, and **Current look** names the preset in use and marks it as modified once you change something.
- **Apply preset** takes the parts you tick: **Look**, **Controls** and, from a complete setup, **Groups**. Language and interface sounds stay as you set them.
- **Appearance → General → Input device presets** applies a chosen preset's look when you start using a controller, and another when you return to keyboard and mouse.
- **Appearance → General → Reset all appearance** applies the Default preset. The Cross, Diamond and Circle layouts are built-in presets under **Presets**.
- The controller's on-screen keyboard includes common punctuation.
- Added a **Wheel** selector under **Appearance → Selector**. Every item keeps its place around a hub; point at one with the browsing stick or the mouse and release the group key or button to select it. The camera stays still while the mouse points. Scrolling and previous / next still step through the wheel.
- The widget eases in, the selector opens and closes smoothly, and a group pulses when its item equips or a dose is used. Turn this off with **Appearance → General → Animate opening and fading**.
- Shout cooldowns show as a shade that drains clockwise over the icon, alongside the seconds.
- **Interface sounds** play Skyrim's menu sounds when the selector opens, moves and cancels. Configure under **Appearance → General**.
- Set **Equipped marker size** and a **Custom equipped color** under **Appearance → Theme** ([#40](https://github.com/opteron-x86/quickslot-hotkeys/issues/40)).
- The **Automatic** theme matches your installed interface mod (Dear Diary Dark Mode, Edge, Norden, Nordic, Oathvein, Untarnished or Vel'dun) and is the default for new installations. Existing settings keep their theme.
- Choose a **Font** under **Appearance → Theme**, including the font your interface mod installs in `Interface/ImGuiIcons/Fonts`.
- Frame artwork keeps even borders and corners on cards, squares, list rows and diamonds.
- The editor takes its surfaces, text and highlights from the selected theme.
- Theme packs can set selection, equipped, warning, depleted, poison and editor panel colors, plus the frame corner size. See [Theme and icon packs](THEMES.md).
- A settings file with missing or invalid values keeps every valid setting. Quickslot copies the original to `Quickslot.settings.rejected.json` beside it.

## 0.14.0

- Added a configurable **Use selected consumable** action for keyboard, controller and VR, plus a group setting for selecting consumables with cycling or browsing. The equipment widget retains the last selected consumable across groups.
- Groups offer **Tap to cycle / hold to browse**, with taps committed on release and held selection in both directions.
- Favorites groups can combine multiple categories, such as weapons and shields/torches, in one list.
- Controller and VR bindings allow Modifier and Button to be selected in either order.
- Tap-to-cycle keeps one-handed weapon copies equipped in the opposite hand by default. Smart weapons choose the strongest eligible copy. Configure this under **Controls → General → Keep the other hand equipped while cycling**.
- Switching PNGII icons on in the editor keeps the interface responsive while artwork loads.

Updating: install the complete release package and keep your settings and presets. Select the new group behaviors in **Groups** and bind **Use selected consumable** under **Controls**.

## 0.13.1

- Skyrim's console keeps its mouse pointer when opened during gameplay or after using the Quickslot editor.

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
