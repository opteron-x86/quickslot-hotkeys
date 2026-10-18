#pragma once
#include "Model.h"
#include <filesystem>

namespace quickslot {
constexpr std::uint32_t rgba(unsigned r,unsigned g,unsigned b,unsigned a=255){return r|(g<<8)|(b<<16)|(a<<24);}
struct Theme {
    std::string id="minimal",name="Minimal",description;
    std::uint32_t text=rgba(233,230,221),secondary=rgba(180,179,174),muted=rgba(134,135,132),
        accent=rgba(157,65,54),background=rgba(13,14,15,155),border=rgba(126,126,119,180);
    unsigned frameStyle=0; // cut corners, straight, double, nordic
    std::filesystem::path atlas,font,titleFont;
};
inline ThemeAdjustments adjustments(const Options& o){const auto it=o.themeAdjustments.find(o.themeId);return it==o.themeAdjustments.end()?ThemeAdjustments{}:it->second;}
inline std::uint32_t themeAccent(const Theme& t,const Options& o){const auto a=adjustments(o);return a.customAccent?rgba(unsigned(a.accent[0]*255),unsigned(a.accent[1]*255),unsigned(a.accent[2]*255)):t.accent;}
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
};
}
