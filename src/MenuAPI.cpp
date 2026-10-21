#define QUICKSLOT_API_EXPORTS
#include "API/QuickslotAPI.h"
#include "MenuAPI.h"

std::uint32_t Quickslot_GetAPIVersion() noexcept{return QuickslotAPI::version;}
bool Quickslot_Open() noexcept{try{return quickslot::menuAPI().requestOpen();}catch(...){return false;}}
void Quickslot_Close() noexcept{try{quickslot::menuAPI().requestClose();}catch(...){}}
bool Quickslot_IsMenuOpen() noexcept{try{return quickslot::menuAPI().isOpen();}catch(...){return false;}}
void Quickslot_SetHotkeyEnabled(bool enabled) noexcept{quickslot::menuAPI().setHotkeyEnabled(enabled);}
