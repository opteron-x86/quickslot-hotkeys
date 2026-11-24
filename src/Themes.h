#pragma once
#include "Model.h"
#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace quickslot {
constexpr std::uint32_t rgba(unsigned r,unsigned g,unsigned b,unsigned a=255){return r|(g<<8)|(b<<16)|(a<<24);}
// How an image layer in a schema 3 theme is tinted when drawn.
enum class ThemeTint : unsigned {
    none,text,secondary,muted,accent,border,selection,equipped,background,warning,depleted,poison,charge
};
// One modular PNG or SVG asset slot in a schema 3 theme.
struct ThemeImage {
    std::filesystem::path path;
    ThemeTint tint=ThemeTint::none;
    float opacity=1.f;
    // Nine-slice margins in source artwork pixels: left, top, right, bottom. All zero draws as a single quad.
    std::array<unsigned,4> slice{};
    // Target longest-edge pixel size when rasterizing an SVG asset (clamped 64..2048).
    unsigned rasterSize=512;
    // Optional outward bleed as a fraction of element size (0..0.35), letting spikes, gears or knotwork extend beyond the structural frame.
    float outset=0.f;
    explicit operator bool()const{return !path.empty();}
};
struct FrameLayers {
    ThemeImage shadow,bg,border,selected,equipped;
    bool any()const{return shadow||bg||border||selected||equipped;}
};
struct WheelLayers {
    ThemeImage ringBg,outerRim,innerRim,hubBg,hubBorder,hubOverlay,pointer,divider;
    bool any()const{return ringBg||outerRim||innerRim||hubBg||hubBorder||hubOverlay||pointer||divider;}
};
struct MarkerLayers {
    ThemeImage equipped,ready,poison,chargePip,soulGem;
    bool any()const{return equipped||ready||poison||chargePip||soulGem;}
};
struct EquipmentPanelLayers {
    ThemeImage bg,border;
    bool any()const{return bg||border;}
};
struct ThemeAssets {
    FrameLayers card,square,diamond,circle;
    WheelLayers wheel;
    ThemeImage divider;
    MarkerLayers markers;
    EquipmentPanelLayers equipmentPanel;
    // Optional per-slot etched glyphs / runes cycled across widget tiles and wheel wedges (up to 16).
    std::vector<ThemeImage> glyphs;
};
template<class Assets,class F> void forEachThemeImage(Assets& a,const F& f) {
    for(auto* frame:{&a.card,&a.square,&a.diamond,&a.circle}) {
        f(frame->shadow);f(frame->bg);f(frame->border);f(frame->selected);f(frame->equipped);
    }
    f(a.wheel.ringBg);f(a.wheel.outerRim);f(a.wheel.innerRim);f(a.wheel.hubBg);f(a.wheel.hubBorder);
    f(a.wheel.hubOverlay);f(a.wheel.pointer);f(a.wheel.divider);
    f(a.divider);
    f(a.markers.equipped);f(a.markers.ready);f(a.markers.poison);f(a.markers.chargePip);f(a.markers.soulGem);
    f(a.equipmentPanel.bg);f(a.equipmentPanel.border);
    for(auto& glyph:a.glyphs)f(glyph);
}
struct ThemeStyle {
    // Negative uses the frameStyle default; 0..24 overrides procedural corner bevel size.
    float cornerCut=-1.f;
    float borderWidth=.8f;
    float shadowOpacity=.35f;
    float glowOpacity=.28f;
    float iconInset=.14f;
    float editorRounding=2.f;
    bool accentLine=true;
};
enum class DetectKind : unsigned {sha256,exists,contains};
// An interface file whose hash, presence or content identifies the UI mod a theme matches.
struct ThemeSignature {
    std::string file,sha256;
    DetectKind kind=DetectKind::sha256;
    std::string contains;
};
struct Theme {
    std::string id="minimal",name="Minimal",description,extends;
    std::uint32_t text=rgba(233,230,221),secondary=rgba(180,179,174),muted=rgba(134,135,132),
        accent=rgba(157,65,54),background=rgba(13,14,15,155),border=rgba(126,126,119,180);
    // Optional roles. Selection and equipped follow the text color unless a pack sets them.
    std::uint32_t selection=text,equipped=text,warning=rgba(157,65,54),depleted=rgba(219,143,132),poison=rgba(174,209,122),
        charge=rgba(138,108,214);
    // Editor surfaces. A zero alpha derives the color from the background.
    std::uint32_t panel=0,panelBorder=0;
    unsigned frameStyle=0;
    // Atlas frame corners keep this many artwork pixels at a constant size; zero stretches the whole cell.
    unsigned atlasMargin=40;
    // Breaks ties among tier-2 (exists / contains) detection matches; higher wins.
    unsigned detectPriority=50;
    std::filesystem::path atlas,font,titleFont,numberFont;
    // Optional JSON object of recommended appearance Options values (schema 3).
    std::string recommendedJson;
    ThemeAssets assets;
    ThemeStyle style;
    std::vector<ThemeSignature> detect;
};
inline constexpr char automaticTheme[]="auto";
// Reads a game resource by its Data-relative path; empty when unavailable.
using ResourceReader=std::function<std::vector<unsigned char>(const std::string&)>;
inline ThemeAdjustments adjustments(const Options& o){const auto it=o.themeAdjustments.find(o.themeId);return it==o.themeAdjustments.end()?ThemeAdjustments{}:it->second;}
inline std::uint32_t themeAccent(const Theme& t,const Options& o){const auto a=adjustments(o);return a.customAccent?rgba(unsigned(a.accent[0]*255),unsigned(a.accent[1]*255),unsigned(a.accent[2]*255)):t.accent;}
std::uint32_t themePanel(const Theme&);
std::uint32_t themePanelBorder(const Theme&);
class Themes {
    std::vector<Theme> packs_;
    std::vector<std::string> warnings_;
public:
    Themes();
    void load(const std::filesystem::path& directory);
    const auto& all()const{return packs_;}
    const auto& warnings()const{return warnings_;}
    const Theme& get(const std::string& id)const;
    bool contains(const std::string& id)const;
    // The pack matching the installed interface (tier-1 sha256 first, then tier-2 exists/contains by priority), or empty.
    std::string detect(const ResourceReader& read)const;
};
}
