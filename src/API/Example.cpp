
#include "QuickslotAPI.h"
#include <windows.h>

struct QuickslotMenu {
    QuickslotAPI::Open open=nullptr;
    QuickslotAPI::Close close=nullptr;
    QuickslotAPI::IsMenuOpen isOpen=nullptr;
    QuickslotAPI::SetHotkeyEnabled setHotkeyEnabled=nullptr;

    bool discover() {
        *this={};
        const auto dll=GetModuleHandleW(L"Quickslot.dll");
        if(!dll)return false;
        const auto version=reinterpret_cast<QuickslotAPI::GetAPIVersion>(GetProcAddress(dll,"Quickslot_GetAPIVersion"));
        if(!version||version()!=QuickslotAPI::version)return false;
        open=reinterpret_cast<QuickslotAPI::Open>(GetProcAddress(dll,"Quickslot_Open"));
        close=reinterpret_cast<QuickslotAPI::Close>(GetProcAddress(dll,"Quickslot_Close"));
        isOpen=reinterpret_cast<QuickslotAPI::IsMenuOpen>(GetProcAddress(dll,"Quickslot_IsMenuOpen"));
        setHotkeyEnabled=reinterpret_cast<QuickslotAPI::SetHotkeyEnabled>(GetProcAddress(dll,"Quickslot_SetHotkeyEnabled"));
        if(open&&close&&isOpen&&setHotkeyEnabled)return true;
        *this={};return false;
    }

    bool openFromLauncher(void(*closeLauncherAndReleaseInput)()) const {
        if(!open||!closeLauncherAndReleaseInput||!open())return false;
        closeLauncherAndReleaseInput();
        return true;
    }
};
