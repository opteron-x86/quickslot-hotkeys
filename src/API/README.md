# Quickslot menu API — version 1

Discover the loaded DLL with `GetModuleHandleW(L"Quickslot.dll")`, then resolve exports with `GetProcAddress`. Include `QuickslotAPI.h` for function pointer types. `Example.cpp` shows discovery and the opening handoff.

| Export | Behavior |
| --- | --- |
| `Quickslot_GetAPIVersion()` | Returns `1`. |
| `Quickslot_Open()` | Accepts an opening request when ready; returns true for an open editor or pending request. |
| `Quickslot_Close()` | Closes the editor and cancels pending opening. Live edits remain saved. |
| `Quickslot_IsMenuOpen()` | Returns the current editor state. |
| `Quickslot_SetHotkeyEnabled(bool)` | Controls the editor-opening shortcut for this game session; initially enabled. |

After an accepted `Open()` request, close the launcher and release its input and pause ownership within three seconds. Poll `IsMenuOpen()` for the resulting state. Loading, loss of focus or timeout cancels the request.

Exports use C linkage, Windows x64 calling conventions and `noexcept`. Calls are thread-safe; requests execute on the game thread in request order. `SetHotkeyEnabled` is a shared boolean; restore it when the launcher finishes.

## IME text input

Route committed characters to `QuickslotEditor` through Skyrim's input queue as `RE::CharEvent` events. Quickslot forwards those characters to its editor fields. SimpleIME integrations use its engine-input routing declaration for this menu.

Quickslot claims text input while an editable field is active and `QuickslotEditor` is on the menu stack. It restores a cleared text-entry counter while that field remains active. Leaving the field or closing the editor releases its claim. Controller on-screen keyboards use Quickslot's editor controls.
