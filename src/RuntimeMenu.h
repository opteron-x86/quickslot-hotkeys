#pragma once
#include <RE/Skyrim.h>

namespace quickslot {

class RuntimeMenu : public RE::IMenu {
    RE::IMenu::VR_RUNTIME_DATA vrData_;
public:
    // Initialize the VR menu data stored after the 0x30-byte base object.
    explicit RuntimeMenu(const char* name){vrData_.menuName=name;}
    void Unk_09(RE::UI_MENU_Unk09 value)override{vrData_.unk30=value;}
    void Unk_0A()override{}
};
static_assert(sizeof(RE::IMenu)==0x30);
static_assert(sizeof(RuntimeMenu)==0x40);
}
