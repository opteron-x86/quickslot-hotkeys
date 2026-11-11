# Quickslot API — version 1

Discover the loaded DLL with `GetModuleHandleW(L"Quickslot.dll")`, then resolve exports with `GetProcAddress`. Include `QuickslotAPI.h` for function pointer types. The version stays `1`: every export listed here keeps its behavior, and new exports are added alongside them. `Example.cpp` shows discovery and the opening handoff.

| Export | Behavior |
| --- | --- |
| `Quickslot_GetAPIVersion()` | Returns `1`. Accept `1` or higher. |
| `Quickslot_Open()` | Accepts an opening request when ready; returns true for an open editor or pending request. |
| `Quickslot_Close()` | Closes the editor and cancels pending opening. Live edits remain saved. |
| `Quickslot_IsMenuOpen()` | Returns the current editor state. |
| `Quickslot_SetHotkeyEnabled(bool)` | Controls the editor-opening shortcut for this game session; initially enabled. |
| `Quickslot_GetGroupCount()` | Optional, Quickslot 0.15 and later; `GetProcAddress` returns null on earlier releases. Returns the number of groups shown in the widget, or `0` before a game is loaded. |
| `Quickslot_TapGroup(position)` | Optional, Quickslot 0.15 and later; `GetProcAddress` returns null on earlier releases. Queues a tap of the group at a widget position (0-based, in display order). The group equips, uses or cycles exactly as its key does. Returns false before a game is loaded, while the editor is open or for a position past the last group. |
| `Quickslot_SwapPrevious()` | Optional, Quickslot 0.15 and later; `GetProcAddress` returns null on earlier releases. Queues a return to the selection made before the current one. Returns false before a game is loaded or while the editor is open. |

After an accepted `Open()` request, close the launcher and release its input and pause ownership within three seconds. Poll `IsMenuOpen()` for the resulting state. Loading, loss of focus or timeout cancels the request.

Gameplay requests run on the game thread in request order when Quickslot's own hotkeys would be accepted; a request made during a menu, scene or kill move is dropped. Up to 16 requests wait at a time.

Exports use C linkage, Windows x64 calling conventions and `noexcept`. Calls are thread-safe; requests execute on the game thread in request order. `SetHotkeyEnabled` is a shared boolean; restore it when the launcher finishes.

## IME text input

Route committed characters to `QuickslotEditor` through Skyrim's input queue as `RE::CharEvent` events. Quickslot forwards those characters to its editor fields. SimpleIME integrations use its engine-input routing declaration for this menu.
