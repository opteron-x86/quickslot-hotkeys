#pragma once
#include <cstdint>

#ifdef QUICKSLOT_API_EXPORTS
#define QUICKSLOT_API extern "C" __declspec(dllexport)
#else
#define QUICKSLOT_API extern "C"
#endif

// Return the supported API version.
QUICKSLOT_API std::uint32_t Quickslot_GetAPIVersion() noexcept;

// Queue an editor-open request. Release the calling menu's input and pause within three seconds.
QUICKSLOT_API bool Quickslot_Open() noexcept;

// Queue editor closure and cancel a pending open request.
QUICKSLOT_API void Quickslot_Close() noexcept;

// Return whether the editor is currently open.
QUICKSLOT_API bool Quickslot_IsMenuOpen() noexcept;

// Enable or disable the editor-opening shortcut for this session.
QUICKSLOT_API void Quickslot_SetHotkeyEnabled(bool enabled) noexcept;

// Version 2: return the number of groups shown in the widget, or zero before a game is loaded.
QUICKSLOT_API std::uint32_t Quickslot_GetGroupCount() noexcept;

// Version 2: queue a tap of the group at this widget position (0-based). The group equips, uses or cycles as its key does.
QUICKSLOT_API bool Quickslot_TapGroup(std::uint32_t position) noexcept;

// Version 2: queue a return to the selection made before the current one.
QUICKSLOT_API bool Quickslot_SwapPrevious() noexcept;

namespace QuickslotAPI {
inline constexpr std::uint32_t version=2;
using GetAPIVersion=std::uint32_t(*)() noexcept;
using Open=bool(*)() noexcept;
using Close=void(*)() noexcept;
using IsMenuOpen=bool(*)() noexcept;
using SetHotkeyEnabled=void(*)(bool) noexcept;
using GetGroupCount=std::uint32_t(*)() noexcept;
using TapGroup=bool(*)(std::uint32_t) noexcept;
using SwapPrevious=bool(*)() noexcept;
}
