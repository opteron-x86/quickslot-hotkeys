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
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace quickslot {
struct LoadedTexture {
    ImTextureID id=0;
    float width=0,height=0;
    explicit operator bool()const{return id!=0&&width>0&&height>0;}
};
struct SkinFrameLayers {
    LoadedTexture shadow,bg,border,selected,equipped;
    bool hasBase()const{return shadow||bg||border;}
    bool any()const{return hasBase()||selected||equipped;}
};
struct SkinWheelLayers {
    LoadedTexture ringBg,outerRim,innerRim,hubBg,hubBorder,hubOverlay,pointer,divider;
    bool any()const{return ringBg||outerRim||innerRim||hubBg||hubBorder||hubOverlay||pointer||divider;}
};
struct SkinMarkerLayers {
    LoadedTexture equipped,ready,poison,chargePip,soulGem;
};
struct SkinEquipmentPanel {
    LoadedTexture bg,border;
    bool any()const{return bg||border;}
};
struct SkinTextures {
    ImTextureID atlas=0;
    SkinFrameLayers card,square,diamond,circle;
    SkinWheelLayers wheel;
    LoadedTexture divider;
    SkinMarkerLayers markers;
    SkinEquipmentPanel equipmentPanel;
    std::vector<LoadedTexture> glyphs;
};
template<class Upload> SkinTextures loadSkinTextures(const Theme& theme,Upload&& upload) {
    SkinTextures out;
    const auto loadFrame=[&](const FrameLayers& src,SkinFrameLayers& dst) {
        dst.shadow=upload(src.shadow);dst.bg=upload(src.bg);dst.border=upload(src.border);
        dst.selected=upload(src.selected);dst.equipped=upload(src.equipped);
    };
    loadFrame(theme.assets.card,out.card);
    loadFrame(theme.assets.square,out.square);
    loadFrame(theme.assets.diamond,out.diamond);
    loadFrame(theme.assets.circle,out.circle);
    out.wheel.ringBg=upload(theme.assets.wheel.ringBg);
    out.wheel.outerRim=upload(theme.assets.wheel.outerRim);
    out.wheel.innerRim=upload(theme.assets.wheel.innerRim);
    out.wheel.hubBg=upload(theme.assets.wheel.hubBg);
    out.wheel.hubBorder=upload(theme.assets.wheel.hubBorder);
    out.wheel.hubOverlay=upload(theme.assets.wheel.hubOverlay);
    out.wheel.pointer=upload(theme.assets.wheel.pointer);
    out.wheel.divider=upload(theme.assets.wheel.divider);
    out.divider=upload(theme.assets.divider);
    out.markers.equipped=upload(theme.assets.markers.equipped);
    out.markers.ready=upload(theme.assets.markers.ready);
    out.markers.poison=upload(theme.assets.markers.poison);
    out.markers.chargePip=upload(theme.assets.markers.chargePip);
    out.markers.soulGem=upload(theme.assets.markers.soulGem);
    out.equipmentPanel.bg=upload(theme.assets.equipmentPanel.bg);
    out.equipmentPanel.border=upload(theme.assets.equipmentPanel.border);
    out.glyphs.reserve(theme.assets.glyphs.size());
    for(const auto& glyph:theme.assets.glyphs)out.glyphs.push_back(upload(glyph));
    return out;
}
class Hud {
    ImFont* medium=nullptr;
    ImFont* sans=nullptr;
    ItemIconProvider itemIcons_;
    ImTextureID keyTexture=0;
    Themes themes_;
    std::filesystem::path fontDirectory_,installedFontDirectory_,themesDirectory_;
    struct CachedFont {
        ImFont* font=nullptr;
        std::vector<char> bytes;
        std::uintmax_t size=0;
        std::filesystem::file_time_type mtime{};
    };
    std::map<std::filesystem::path,CachedFont> fontCache_;
    // Previous font buffers kept alive when a font file is hot-reloaded so merged fallbacks never dangle.
    std::vector<std::vector<char>> retiredFontBytes_;
    std::vector<std::filesystem::path> fallbacks_;
    std::uint64_t themeRevision_=0;
    struct Skin {ImFont* body=nullptr;ImFont* title=nullptr;ImFont* number=nullptr;SkinTextures textures;};
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
    std::array<TileSwapMotion,Arsenal::count> swaps_;
    // The angle the wheel's highlight is drawn at and per-wedge hover lift; both glide smoothly toward the pointed segment.
    float wheelHighlight_=0;bool wheelHighlightSet_=false;double wheelHighlightAt_=0;
    std::array<float,32> wedgeLift_{};bool wedgeLiftSet_=false;
    CooldownScale cooldowns_;
    KeyGlyphs keyGlyphs{};
    ImFont* loadCachedFont(const std::filesystem::path& path,float size,bool allowFallback);
public:
    void initialize(const std::string& fontDirectory,const std::string& installedFontDirectory={});
    // Reloads theme manifests and theme/interface fonts from disk. Call between ImGui frames.
    void reloadThemes(const std::filesystem::path& themesDirectory={});
    const std::filesystem::path& themesDirectory()const{return themesDirectory_;}
    std::uint64_t themeRevision()const{return themeRevision_;}
    const std::vector<std::string>& themeWarnings()const{return themes_.warnings();}
    const std::vector<FontChoice>& fonts()const{return fonts_;}
    // True for icon, rune and lore-script typefaces (e.g. Daedric-Script, Falmer-Script, Dragon-Script, fa-solid)
    // that cannot render readable UI text.
    static bool isNonTextFont(std::string_view raw) {
        std::string name(raw);for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
        return name.starts_with("fa-")||std::ranges::any_of(std::array{
            "script","rune","awesome","icon","symbol","glyph","dingbat","emoji","daedric","falmer","dovah"
        },[&](const char* mark){return name.contains(mark);});
    }
    // The installed interface's text typeface. Skips non-text faces and prefers a Regular or Book weight.
    // Null when the folder has no text face.
    static const FontChoice* interfaceFont(const std::vector<FontChoice>& fonts) {
        const FontChoice* first=nullptr;
        for(const auto& choice:fonts) {
            if(!choice.id.starts_with("ui/")||!choice.font)continue;
            const std::string_view tail=std::string_view(choice.id).substr(3);
            if(isNonTextFont(tail))continue;
            std::string name(tail);for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
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
    void setTheme(const std::string& id,ImTextureID texture){skins_[id].textures.atlas=texture;}
    void setThemeTextures(const std::string& id,SkinTextures textures){skins_[id].textures=std::move(textures);}
    // The pack matching the installed interface; the Automatic theme resolves to it.
    void setDetectedTheme(std::string id){detected_=themes_.contains(id)?std::move(id):std::string{};}
    const std::string& detectedTheme()const{return detected_;}
    const Theme& theme(const Options& o)const{return themes_.get(o.themeId==automaticTheme&&!detected_.empty()?detected_:o.themeId);}
    const Themes& themes()const{return themes_;}
    ImFont* bodyFont(const Options& o)const{if(auto* chosen=chosenFont(o))return chosen;const auto it=skins_.find(theme(o).id);return it==skins_.end()||!it->second.body?sans:it->second.body;}
    ImFont* numberFont(const Options& o)const{if(auto* chosen=chosenFont(o))return chosen;const auto it=skins_.find(theme(o).id);return it==skins_.end()||!it->second.number?bodyFont(o):it->second.number;}
    EquipmentMetrics equipmentMetrics(const EquippedDisplay&,const Options&)const;
    void setKeys(ImTextureID texture,const KeyGlyphs& glyphs){keyTexture=texture;keyGlyphs=glyphs;}
    // Draws an entry's artwork for lists outside the HUD, with the player's icon color settings.
    void drawItemIcon(ImDrawList* list,ImVec2 center,float size,const Entry& entry,const Options& options,bool available=true)const;
    void draw(const Snapshot& snapshot,const Options& options,double now,ImVec2 origin={0,0},ImVec2 size={0,0},ImDrawList* list=nullptr,HudBounds* bounds=nullptr);
};
}
