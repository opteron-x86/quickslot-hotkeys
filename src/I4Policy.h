#pragma once
#include "IconPolicy.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace quickslot {
inline constexpr char i4DefaultSource[]="Interface/skyui/icons_item_psychosteve.swf";
inline const char* i4DefaultLabel(const Entry& entry) {
    if(isHandAction(entry))return "armor_hands";
    switch(baseIcon(entry)) {
    case Icon::sword:return "weapon_sword";
    case Icon::greatsword:return "weapon_greatsword";
    case Icon::dagger:return "weapon_dagger";
    case Icon::axe:return entry.twoHanded?"weapon_battleaxe":"weapon_waraxe";
    case Icon::mace:return entry.twoHanded?"weapon_hammer":"weapon_mace";
    case Icon::battleaxe:return "weapon_battleaxe";
    case Icon::warhammer:return "weapon_hammer";
    case Icon::bow:return "weapon_bow";
    case Icon::crossbow:return "weapon_crossbow";
    case Icon::staff:return "weapon_staff";
    case Icon::shield:return "armor_shield";
    case Icon::torch:return "misc_torch";
    case Icon::arrow:return "weapon_arrow";
    case Icon::bolt:return "weapon_bolt";
    case Icon::pickaxe:return "weapon_pickaxe";
    case Icon::woodcutters_axe:return "weapon_woodaxe";
    case Icon::fire:return "magic_fire";
    case Icon::frost:return "magic_frost";
    case Icon::shock:return "magic_shock";
    case Icon::alteration:return "default_alteration";
    case Icon::conjuration:return "default_conjuration";
    case Icon::destruction:return "default_destruction";
    case Icon::illusion:return "default_illusion";
    case Icon::restoration:return "default_restoration";
    case Icon::health:return "potion_health";
    case Icon::magicka:return "potion_magic";
    case Icon::stamina:return "potion_stam";
    case Icon::poison:return "potion_poison";
    case Icon::food:return "default_food";
    case Icon::potion:return "default_potion";
    case Icon::scroll:return "default_scroll";
    case Icon::shout:return "default_shout";
    case Icon::power:case Icon::lesser_power:case Icon::greater_power:return "default_power";
    default:break;
    }
    if(entry.kind==Kind::spell)return "default_effect";
    if(entry.kind<=Kind::staff) {
        auto category=entry;category.icon=Icon::automatic;return i4DefaultLabel(category);
    }
    return "default_misc";
}
inline bool i4HasForm(const Entry& entry){return entry.form&&!entry.unresolved&&!isReusable(entry);}
inline std::string i4RequestKey(const Entry& entry) {
    return i4HasForm(entry)?"form:"+std::to_string(entry.form):std::string("category:")+i4DefaultLabel(entry);
}
inline std::optional<std::string> i4SourcePath(std::string source) {
    if(source.empty())source="skyui/icons_item_psychosteve.swf";
    if(source.size()>240)return {};
    std::replace(source.begin(),source.end(),'\\','/');
    std::string lower=source;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return c>='A'&&c<='Z'?char(c+32):char(c);});
    if(lower.starts_with("interface/")){source.erase(0,10);lower.erase(0,10);}
    if(!lower.ends_with(".swf")||source.front()=='/'||source.find(':')!=std::string::npos)return {};
    for(unsigned char c:source)if(c<32)return {};
    for(std::size_t start=0;start<source.size();) {
        const auto end=source.find('/',start);const auto part=std::string_view(source).substr(start,end==std::string::npos?end:end-start);
        if(part.empty()||part=="."||part=="..")return {};
        if(end==std::string::npos)break;start=end+1;
    }
    return "Interface/"+source;
}
struct IconFit {double x,y,scale;};
inline std::optional<IconFit> fitI4Icon(double left,double top,double right,double bottom,double size) {
    const double width=right-left,height=bottom-top;
    if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(width)||!std::isfinite(height)||
       !std::isfinite(size)||width<=0||height<=0||size<4)return {};
    const double scale=(size*216/256)/std::max(width,height);
    return IconFit{(size-width*scale)*.5-left*scale,(size-height*scale)*.5-top*scale,scale};
}
// Scaleform render textures contain premultiplied color; ImGui consumes straight RGBA.
inline bool straightenI4Pixels(std::span<std::uint8_t> rgba) {
    bool visible=false;
    for(std::size_t i=0;i+3<rgba.size();i+=4) {
        const unsigned alpha=rgba[i+3];visible|=alpha!=0;
        for(unsigned c=0;c<3;++c)rgba[i+c]=alpha?std::uint8_t(std::min(255u,(unsigned(rgba[i+c])*255+alpha/2)/alpha)):0;
    }
    return visible;
}
}
