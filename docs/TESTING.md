# Gameplay checks

Run these in a loaded game. Record the package version, Skyrim/SKSE version and controller or headset used.

| Area | Check |
| --- | --- |
| Updating | Groups, shortcuts, settings and presets survive installation and save/load. |
| Group lifecycle | Rename, bind and populate a group; enable auto-sort and change its behavior. Remove it and add another repeatedly. Each new group should be empty with a fresh name, default behavior and the lowest unused 1–8 key (1, 3 → 2), skipping the editor key. Check controller/VR defaults, unchanged bindings on other groups, Undo restoring all removed state, and save/load. Clear group should retain its name, bindings and settings. |
| Hold to browse | Tap equips the remembered entry. Hold, browse and release selects another. Cancel restores gameplay controls. |
| Tap to cycle | Returning from another group recalls the saved choice; subsequent taps advance. Check wrapping, rapid taps and unavailable entries. |
| Keyboard | Left/Right browses; Ctrl + Left/Right reorders. Both wrap and retain the highlighted entry. Ctrl belongs to the selector while browsing. |
| Optional shortcuts | Under Controls → Keyboard, disable all editing shortcuts and check that Ctrl/Shift/Alt group chords, Ctrl-arrows, Backspace and Delete reach other mods without triggering Quickslot actions. Plain arrows/wheel browsing and Escape/right-click cancellation should still work. Re-enable actions individually, including right/left capture in gameplay and Inventory/Magic and moving with capture disabled. Verify save/load, Undo and the separate menu-capture switch. |
| Consumables | Tap uses one dose. Browsing readies a dose. Check animation timing, poison confirmation, smart-potion ranking and restocking. |
| Powers and shouts | Check selection, activation, charge, cooldown and used-power dimming. |
| Equipment | Check left/right hands, shields, torches, two-handed weapons, spells and empty-hand actions. Highlights reflect the complete equipped entry. |
| Smart combinations | Check Best Sword + Best Mace, Best Axe + Best Shield, fixed + smart, spell + smart and two identical rules. Verify separate copies, tempering, missing items and restocking. |
| Item identity | Drop, retrieve, sell, buy, store and restock items. Weapons/shields follow their captured copies; torches follow stock of their saved type. |
| Shared shortcuts | Edit a shortcut used by several groups. Check all linked groups, Undo, save/load and preset import/export. |
| Menus | Inventory, Tween, QuickLoot and message boxes receive their controls. Opening a menu cancels pending Quickslot actions. QuickLoot leaves normal HUD visibility active. |
| Controller | Check modifier chords, D-pad navigation, right-stick pointer, drag, text entry, reconnect and focus changes. |
| Equipment display | Right hand first; two-handed items occupy one entry. Check quantity badges, shout timers, spent-power dimming and text-only layouts. |
| Appearance | Check every style, text position, scale and spacing. Spacing changes gaps while icon size stays fixed. Test 720p, 1080p, ultrawide and 4K. |
| Position | Drag each widget, adjust X/Y, snap edges/centers and use Shift for free placement. Check screen edges and independent positions. |
| Visibility | Check show/dim/hide, combat, drawn weapons, browsing and independent equipment visibility. |
| VR | Check helper opening, pointer input, hand bindings, grip chords, headset placement and focus changes. |
| Integrations | Check the menu API with the launcher and RTSS on Windows. |
| IME overlays | Open the editor and trigger a cursor-only language bar, type, select candidates and close the editor. Test a SimpleIME version that still pushes its language-bar menu, since its newer workaround masks the original failure. Inventory/Map/Console, a modal overlay, and loss of window focus must still close the editor and log why. Physical in-game verification is pending. |
| IME text-input ownership | Use SimpleIME's released engine-input routing declaration for end-to-end Chinese text delivery. Disable it separately when checking that the counter is never healed away; its automatic GFx routing does not deliver committed Chinese to Quickslot. In a debug session, clear `ControlMap::GetRuntimeData().textEntryCount` while a text field is active: typing must recover without reopening, with one recovery warning per session. Clear it before field blur or closing; it must stay zero without an extra release or recovery. Check another owner's balanced claim/release, cancellation before menu show, focus loss, game load/reset and unexpected editor-menu removal. |
| IME field focus | In CJK compose mode, open the editor without activating a field: the editor must not claim text input. Activate search, group/shortcut names, hand-picker search and preset names; check the first typed character, composition, candidate selection and English input. Click away, navigate, capture a binding and close/reopen rapidly; composition must stop outside an active field and must not resume from stale focus. Check Tab, rapid field changes, the controller on-screen keyboard and VR pointer text entry. Acquisition/release logs should follow field activation/blur, with no per-frame counter growth. In-game validation of this feature is pending. |

The SimpleIME author [verified the #3 ownership fix](https://github.com/opteron-x86/quickslot-hotkeys/pull/4#issuecomment-6380918875) with nine cycles without its Quickslot declaration and three end-to-end Chinese-input cycles with the declaration. The ordering fix prevented the counter reset in-game; recovery remains covered by the automated interaction probes. Field-focused claiming (#5) requires a new in-game check. Automated UI checks exercise field activation/blur, first-character delivery, key binding, nested search, controller text entry and reopening.

For failures, include `Quickslot.log`, reproduction steps and relevant equipment/input mods. For crashes, include the crash log. Report automated test results and in-game observations separately.
