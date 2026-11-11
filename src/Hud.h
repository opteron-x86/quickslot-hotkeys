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
    ItemIconProvider itemIcons_;
    ImTextureID keyTexture=0;
    Themes themes_;
    std::map<std::filesystem::path,std::vector<char>> fontData_;
    struct Skin {ImFont* body=nullptr;ImFont* title=nullptr;ImTextureID texture=0;};
public:
    // A selectable typeface. Ids start with "ui/" for the installed interface's ImGuiIcons fonts and "quickslot/" for bundled ones.
    struct FontChoice {std::string id,name;ImFont* font=nullptr;};
private:
    std::vector<FontChoice> fonts_;
    std::string detected_;
    std::map<std::string,Skin> skins_;
    ScrollMotion motion_;
    IdleFade fade_;
    EquipmentFade equipmentFade_;
    KeyGlyphs keyGlyphs{};
public:
    void initialize(const std::string& fontDirectory,const std::string& installedFontDirectory={});
    const std::vector<FontChoice>& fonts()const{return fonts_;}
    // The player's typeface, or null when the theme's fonts apply. "ui" selects the first installed interface font.
    ImFont* chosenFont(const Options& o)const {
        // An unrecognized interface under the Automatic theme still gets its own typeface.
        const bool installed=o.fontId=="ui"||(o.fontId.empty()&&o.themeId==automaticTheme&&detected_.empty());
        if(o.fontId.empty()&&!installed)return nullptr;
        for(const auto& choice:fonts_)if(choice.id==o.fontId||(installed&&choice.id.starts_with("ui/")))return choice.font;
        return nullptr;
    }
    void setItemIconProvider(ItemIconProvider provider){itemIcons_=std::move(provider);}
    void setTheme(const std::string& id,ImTextureID texture){skins_[id].texture=texture;}
    // The pack matching the installed interface; the Automatic theme resolves to it.
    void setDetectedTheme(std::string id){detected_=themes_.contains(id)?std::move(id):std::string{};}
    const std::string& detectedTheme()const{return detected_;}
    const Theme& theme(const Options& o)const{return themes_.get(o.themeId==automaticTheme&&!detected_.empty()?detected_:o.themeId);}
    const Themes& themes()const{return themes_;}
    ImFont* bodyFont(const Options& o)const{if(auto* chosen=chosenFont(o))return chosen;const auto it=skins_.find(theme(o).id);return it==skins_.end()||!it->second.body?sans:it->second.body;}
    EquipmentMetrics equipmentMetrics(const EquippedDisplay&,const Options&)const;
    void setKeys(ImTextureID texture,const KeyGlyphs& glyphs){keyTexture=texture;keyGlyphs=glyphs;}
    void draw(const Snapshot& snapshot,const Options& options,double now,ImVec2 origin={0,0},ImVec2 size={0,0},ImDrawList* list=nullptr,HudBounds* bounds=nullptr);
};
}
