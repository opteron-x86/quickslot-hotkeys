#pragma once
#include "Model.h"
#include "KeyGlyphs.h"
#include "Themes.h"
#include "HudMotion.h"
#include "Layout.h"
#include "ItemIcons.h"
#include <imgui.h>
#include <string>

namespace quickslot {
class Hud {
    ImFont* medium=nullptr;
    ImFont* sans=nullptr;
    struct IconSet {std::string name;ImTextureID texture=0;};
    std::map<std::string,IconSet> iconSets_{{"i4",{"I4",0}},{"quickslot",{"Quickslot",0}}};
    ItemIconProvider itemIcons_;
    ImTextureID keyTexture=0;
    Themes themes_;
    std::map<std::filesystem::path,std::vector<char>> fontData_;
    struct Skin {ImFont* body=nullptr;ImFont* title=nullptr;ImTextureID texture=0;};
    std::map<std::string,Skin> skins_;
    ScrollMotion motion_;
    IdleFade fade_;
    EquipmentFade equipmentFade_;
    KeyGlyphs keyGlyphs{};
public:
    void initialize(const std::string& fontDirectory);
    void setIcons(const std::string& id,const std::string& name,ImTextureID texture){iconSets_[id]={name,texture};}
    void setItemIconProvider(ItemIconProvider provider){itemIcons_=std::move(provider);}
    const auto& iconSets()const{return iconSets_;}
    ImTextureID iconTexture(const Options& o)const{auto it=iconSets_.find(o.itemIconSet);if(it==iconSets_.end()||o.itemIconSet=="i4")it=iconSets_.find("quickslot");return it->second.texture;}
    void setTheme(const std::string& id,ImTextureID texture){skins_[id].texture=texture;}
    const Themes& themes()const{return themes_;}
    ImFont* bodyFont(const Options& o)const{const auto it=skins_.find(themes_.get(o.themeId).id);return it==skins_.end()||!it->second.body?sans:it->second.body;}
    void setKeys(ImTextureID texture,const KeyGlyphs& glyphs){keyTexture=texture;keyGlyphs=glyphs;}
    void draw(const Snapshot& snapshot,const Options& options,double now,ImVec2 origin={0,0},ImVec2 size={0,0},ImDrawList* list=nullptr,HudBounds* bounds=nullptr);
};
}
