#pragma once
#include "Model.h"
#include "KeyGlyphs.h"
#include "Themes.h"
#include "HudMotion.h"
#include "Layout.h"
#include "ItemIcons.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cctype>
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
    Eased shown_,selectorOpen_;
    std::array<TilePulse,Arsenal::count> pulses_;
    CooldownScale cooldowns_;
    KeyGlyphs keyGlyphs{};
public:
    void initialize(const std::string& fontDirectory,const std::string& installedFontDirectory={});
    const std::vector<FontChoice>& fonts()const{return fonts_;}
    // The installed interface's text typeface. Interface font folders also hold rune, script and icon
    // faces (ImGui Icons ships Daedric-Script and fa-solid), so those are skipped, and a Regular or
    // Book weight is preferred. Null when the folder has no text face.
    static const FontChoice* interfaceFont(const std::vector<FontChoice>& fonts) {
        const FontChoice* first=nullptr;
        for(const auto& choice:fonts) {
            if(!choice.id.starts_with("ui/")||!choice.font)continue;
            std::string name=choice.id.substr(3);for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
            if(name.starts_with("fa-")||std::ranges::any_of(std::array{"script","rune","awesome","icon","symbol","glyph","dingbat","emoji"},[&](const char* mark){return name.contains(mark);}))continue;
            if(name.contains("regular")||name.contains("book"))return &choice;
            if(!first)first=&choice;
        }
        return first;
    }
    // The player's typeface, or null when the theme's fonts apply. "ui" selects the installed interface's text font.
    ImFont* chosenFont(const Options& o)const {
        // An unrecognized interface under the Automatic theme still gets its own typeface.
        const bool installed=o.fontId=="ui"||(o.fontId.empty()&&o.themeId==automaticTheme&&detected_.empty());
        if(installed){const auto* choice=interfaceFont(fonts_);return choice?choice->font:nullptr;}
        if(o.fontId.empty())return nullptr;
        for(const auto& choice:fonts_)if(choice.id==o.fontId)return choice.font;
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
    // Draws an entry's artwork for lists outside the HUD, with the player's icon color settings.
    void drawItemIcon(ImDrawList* list,ImVec2 center,float size,const Entry& entry,const Options& options,bool available=true)const;
    void draw(const Snapshot& snapshot,const Options& options,double now,ImVec2 origin={0,0},ImVec2 size={0,0},ImDrawList* list=nullptr,HudBounds* bounds=nullptr);
};
}
