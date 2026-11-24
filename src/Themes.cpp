#include "Themes.h"
#include "Sha256.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <set>

namespace quickslot {
namespace {
using Json=nlohmann::json;
bool validID(const std::string& id){return !id.empty()&&id.size()<=48&&std::all_of(id.begin(),id.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});}
std::string lowerAscii(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
std::filesystem::path assetPath(const std::filesystem::path& root,const std::string& value) {
    if(value.empty())return {};
    if(value.find('\\')!=std::string::npos||value.find(':')!=std::string::npos)throw std::runtime_error("Invalid asset path");
    const std::filesystem::path p(value);if(p.is_absolute())throw std::runtime_error("Absolute asset path");
    for(const auto& part:p)if(part==".."||part==".")throw std::runtime_error("Asset traversal");
    const auto full=root/p;
    const auto relative=std::filesystem::weakly_canonical(full).lexically_relative(std::filesystem::weakly_canonical(root));
    if(relative.empty()||*relative.begin()=="..")throw std::runtime_error("Asset outside theme pack");
    return full;
}
std::filesystem::path asset(const std::filesystem::path& root,const Json& j,const char* key) {
    if(!j.contains(key)||j.at(key).is_null())return {};
    return assetPath(root,j.at(key).get<std::string>());
}
std::filesystem::path fontAsset(const std::filesystem::path& packRoot,const std::filesystem::path& sharedFontsRoot,const Json& j,const char* key) {
    if(!j.contains(key)||j.at(key).is_null())return {};
    const auto raw=j.at(key).get<std::string>();
    if(raw.empty())return {};
    if(raw.starts_with("@fonts/"))return assetPath(sharedFontsRoot,raw.substr(7));
    return assetPath(packRoot,raw);
}
std::uint32_t color(const Json& j,const char* key,std::uint32_t fallback) {
    if(!j.contains(key))return fallback;
    const auto value=j.at(key).get<std::vector<unsigned>>();if(value.size()!=3&&value.size()!=4)throw std::runtime_error("Expected RGB or RGBA color");
    for(auto v:value)if(v>255)throw std::runtime_error("Color outside byte range");
    return rgba(value[0],value[1],value[2],value.size()==4?value[3]:255);
}
ThemeTint parseTint(const std::string& name) {
    if(name=="none")return ThemeTint::none;
    if(name=="text")return ThemeTint::text;
    if(name=="secondary")return ThemeTint::secondary;
    if(name=="muted")return ThemeTint::muted;
    if(name=="accent")return ThemeTint::accent;
    if(name=="border")return ThemeTint::border;
    if(name=="selection")return ThemeTint::selection;
    if(name=="equipped")return ThemeTint::equipped;
    if(name=="background")return ThemeTint::background;
    if(name=="warning")return ThemeTint::warning;
    if(name=="depleted")return ThemeTint::depleted;
    if(name=="poison")return ThemeTint::poison;
    if(name=="charge")return ThemeTint::charge;
    throw std::runtime_error("Unknown asset tint role");
}
bool validImageExtension(const std::filesystem::path& p) {
    const auto ext=lowerAscii(p.extension().string());
    return ext==".png"||ext==".svg";
}
ThemeImage parseImage(const std::filesystem::path& root,const Json& value) {
    ThemeImage image;
    if(value.is_null())return image;
    if(value.is_string()) {
        const auto raw=value.get<std::string>();if(raw.empty())return image;
        image.path=assetPath(root,raw);
        if(!validImageExtension(image.path))throw std::runtime_error("Theme image must be PNG or SVG");
        return image;
    }
    if(!value.is_object())throw std::runtime_error("Expected theme image path or object");
    if(!value.contains("file")||value.at("file").is_null())return image;
    const auto raw=value.at("file").get<std::string>();if(raw.empty())return image;
    image.path=assetPath(root,raw);
    if(!validImageExtension(image.path))throw std::runtime_error("Theme image must be PNG or SVG");
    if(value.contains("tint"))image.tint=parseTint(value.at("tint").get<std::string>());
    if(value.contains("opacity")) {
        const float op=value.at("opacity").get<float>();
        if(!std::isfinite(op))throw std::runtime_error("Invalid image opacity");
        image.opacity=std::clamp(op,0.f,1.f);
    }
    if(value.contains("slice")) {
        const auto& s=value.at("slice");
        if(s.is_number_unsigned()) {
            const unsigned v=std::min(s.get<unsigned>(),1024u);
            image.slice={v,v,v,v};
        } else if(s.is_array()&&s.size()==4) {
            for(std::size_t i=0;i<4;++i)image.slice[i]=std::min(s[i].get<unsigned>(),1024u);
        } else throw std::runtime_error("Invalid image slice");
    }
    if(value.contains("rasterSize"))image.rasterSize=std::clamp(value.at("rasterSize").get<unsigned>(),64u,2048u);
    if(value.contains("outset")) {
        const float o=value.at("outset").get<float>();
        if(!std::isfinite(o))throw std::runtime_error("Invalid image outset");
        image.outset=std::clamp(o,0.f,.35f);
    }
    return image;
}
void applyImageSlot(ThemeImage& target,const std::filesystem::path& root,const Json& obj,const char* key) {
    if(obj.contains(key))target=parseImage(root,obj.at(key));
}
void applyFrameLayers(FrameLayers& target,const std::filesystem::path& root,const Json& obj,const char* key) {
    if(!obj.contains(key))return;
    const auto& v=obj.at(key);
    if(v.is_null()){target={};return;}
    if(!v.is_object())throw std::runtime_error("Expected frame layer object");
    applyImageSlot(target.shadow,root,v,"shadow");
    applyImageSlot(target.bg,root,v,"bg");
    applyImageSlot(target.border,root,v,"border");
    applyImageSlot(target.selected,root,v,"selected");
    applyImageSlot(target.equipped,root,v,"equipped");
}
void applyWheelLayers(WheelLayers& target,const std::filesystem::path& root,const Json& obj,const char* key) {
    if(!obj.contains(key))return;
    const auto& v=obj.at(key);
    if(v.is_null()){target={};return;}
    if(!v.is_object())throw std::runtime_error("Expected wheel layer object");
    applyImageSlot(target.ringBg,root,v,"ringBg");
    if(v.contains("ringBorder")&&!v.contains("outerRim"))applyImageSlot(target.outerRim,root,v,"ringBorder");
    else applyImageSlot(target.outerRim,root,v,"outerRim");
    applyImageSlot(target.innerRim,root,v,"innerRim");
    applyImageSlot(target.hubBg,root,v,"hubBg");
    applyImageSlot(target.hubBorder,root,v,"hubBorder");
    applyImageSlot(target.hubOverlay,root,v,"hubOverlay");
    applyImageSlot(target.pointer,root,v,"pointer");
    applyImageSlot(target.divider,root,v,"divider");
}
void applyMarkerLayers(MarkerLayers& target,const std::filesystem::path& root,const Json& obj,const char* key) {
    if(!obj.contains(key))return;
    const auto& v=obj.at(key);
    if(v.is_null()){target={};return;}
    if(!v.is_object())throw std::runtime_error("Expected marker layer object");
    applyImageSlot(target.equipped,root,v,"equipped");
    applyImageSlot(target.ready,root,v,"ready");
    applyImageSlot(target.poison,root,v,"poison");
    applyImageSlot(target.chargePip,root,v,"chargePip");
    applyImageSlot(target.soulGem,root,v,"soulGem");
}
void applyEquipmentPanel(EquipmentPanelLayers& target,const std::filesystem::path& root,const Json& obj,const char* key) {
    if(!obj.contains(key))return;
    const auto& v=obj.at(key);
    if(v.is_null()){target={};return;}
    if(!v.is_object())throw std::runtime_error("Expected equipment panel layer object");
    applyImageSlot(target.bg,root,v,"bg");
    applyImageSlot(target.border,root,v,"border");
}
void applyStyle(ThemeStyle& s,const Json& j) {
    if(!j.contains("style"))return;
    const auto& v=j.at("style");
    if(!v.is_object())throw std::runtime_error("Invalid theme style");
    const auto bounded=[&](const char* key,float& out,float lo,float hi) {
        if(!v.contains(key))return;
        const float x=v.at(key).get<float>();
        if(!std::isfinite(x))throw std::runtime_error("Invalid style number");
        out=std::clamp(x,lo,hi);
    };
    bounded("cornerCut",s.cornerCut,0.f,24.f);
    bounded("borderWidth",s.borderWidth,.5f,4.f);
    bounded("shadowOpacity",s.shadowOpacity,0.f,1.f);
    bounded("glowOpacity",s.glowOpacity,0.f,1.f);
    bounded("iconInset",s.iconInset,.05f,.35f);
    bounded("editorRounding",s.editorRounding,0.f,12.f);
    if(v.contains("accentLine"))s.accentLine=v.at("accentLine").get<bool>();
}
std::vector<ThemeSignature> parseDetect(const Json& j) {
    std::vector<ThemeSignature> out;
    if(!j.contains("detect"))return out;
    const auto& signatures=j.at("detect");
    if(!signatures.is_array()||signatures.size()>16)throw std::runtime_error("Invalid detection list");
    for(const auto& entry:signatures) {
        if(!entry.is_object())throw std::runtime_error("Invalid detection signature");
        ThemeSignature sig;
        sig.file=entry.at("file").get<std::string>();
        const bool validFile=!sig.file.empty()&&sig.file.size()<=160&&sig.file.front()!='/'&&sig.file.find("..")==std::string::npos&&
            std::all_of(sig.file.begin(),sig.file.end(),[](unsigned char c){return c>=32&&c<127&&c!='\\'&&c!=':';});
        if(!validFile)throw std::runtime_error("Invalid detection signature");
        const bool hasSha=entry.contains("sha256"),hasExists=entry.contains("exists"),hasContains=entry.contains("contains");
        if(unsigned(hasSha)+unsigned(hasExists)+unsigned(hasContains)!=1)throw std::runtime_error("Invalid detection signature");
        if(hasSha) {
            sig.kind=DetectKind::sha256;sig.sha256=entry.at("sha256").get<std::string>();
            const bool validDigest=sig.sha256.size()==64&&std::all_of(sig.sha256.begin(),sig.sha256.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
            if(!validDigest)throw std::runtime_error("Invalid detection signature");
        } else if(hasExists) {
            if(!entry.at("exists").is_boolean()||!entry.at("exists").get<bool>())throw std::runtime_error("Invalid detection signature");
            sig.kind=DetectKind::exists;
        } else {
            sig.kind=DetectKind::contains;sig.contains=entry.at("contains").get<std::string>();
            if(sig.contains.empty()||sig.contains.size()>128)throw std::runtime_error("Invalid detection signature");
        }
        out.push_back(std::move(sig));
    }
    return out;
}
struct TrackedFlags {bool selectionSet=false,equippedSet=false;};
void applyManifest(Theme& t,TrackedFlags& flags,const Json& j,const std::filesystem::path& root,unsigned schema) {
    t.id=j.at("id").get<std::string>();
    t.name=j.at("name").get<std::string>();
    if(j.contains("description"))t.description=j.value("description",std::string{});
    if(j.contains("extends"))t.extends=j.value("extends",std::string{});
    if(j.contains("frameStyle"))t.frameStyle=std::min(j.at("frameStyle").get<unsigned>(),3u);
    if(j.contains("palette")) {
        const auto& palette=j.at("palette");
        if(!palette.is_object())throw std::runtime_error("Invalid palette");
        t.text=color(palette,"text",t.text);
        t.secondary=color(palette,"secondary",t.secondary);
        t.muted=color(palette,"muted",t.muted);
        t.accent=color(palette,"accent",t.accent);
        t.background=color(palette,"background",t.background);
        t.border=color(palette,"border",t.border);
        if(palette.contains("selection")){t.selection=color(palette,"selection",t.text);flags.selectionSet=true;}
        else if(!flags.selectionSet)t.selection=t.text;
        if(palette.contains("equipped")){t.equipped=color(palette,"equipped",t.text);flags.equippedSet=true;}
        else if(!flags.equippedSet)t.equipped=t.text;
        t.warning=color(palette,"warning",t.warning);
        t.depleted=color(palette,"depleted",t.depleted);
        t.poison=color(palette,"poison",t.poison);
        t.charge=color(palette,"charge",t.charge);
        t.panel=color(palette,"panel",t.panel);
        t.panelBorder=color(palette,"panelBorder",t.panelBorder);
    }
    if(j.contains("atlasMargin"))t.atlasMargin=std::min(j.at("atlasMargin").get<unsigned>(),120u);
    if(j.contains("detectPriority"))t.detectPriority=std::min(j.at("detectPriority").get<unsigned>(),100u);
    if(j.contains("detect"))t.detect=parseDetect(j);
    const auto sharedFonts=root.parent_path().parent_path()/"fonts";
    if(j.contains("atlas"))t.atlas=asset(root,j,"atlas");
    if(j.contains("font"))t.font=fontAsset(root,sharedFonts,j,"font");
    if(j.contains("titleFont"))t.titleFont=fontAsset(root,sharedFonts,j,"titleFont");
    if(j.contains("numberFont"))t.numberFont=fontAsset(root,sharedFonts,j,"numberFont");
    if(schema>=3) {
        applyStyle(t.style,j);
        if(j.contains("recommended")) {
            const auto& rec=j.at("recommended");
            if(rec.is_null())t.recommendedJson.clear();
            else {
                if(!rec.is_object()||rec.size()>64)throw std::runtime_error("Invalid recommended look");
                for(const auto& [k,val]:rec.items()) {
                    if(k.empty()||k.size()>48||!(val.is_boolean()||val.is_number()))throw std::runtime_error("Invalid recommended look value");
                    if(val.is_number_float()&&!std::isfinite(val.get<double>()))throw std::runtime_error("Invalid recommended look value");
                }
                t.recommendedJson=rec.dump();
            }
        }
        if(j.contains("assets")) {
            const auto& assets=j.at("assets");
            if(!assets.is_object())throw std::runtime_error("Invalid assets object");
            applyFrameLayers(t.assets.card,root,assets,"card");
            applyFrameLayers(t.assets.square,root,assets,"square");
            applyFrameLayers(t.assets.diamond,root,assets,"diamond");
            applyFrameLayers(t.assets.circle,root,assets,"circle");
            applyWheelLayers(t.assets.wheel,root,assets,"wheel");
            applyImageSlot(t.assets.divider,root,assets,"divider");
            applyMarkerLayers(t.assets.markers,root,assets,"markers");
            applyEquipmentPanel(t.assets.equipmentPanel,root,assets,"equipmentPanel");
            if(assets.contains("glyphs")) {
                const auto& g=assets.at("glyphs");
                if(g.is_null())t.assets.glyphs.clear();
                else {
                    if(!g.is_array()||g.size()>16)throw std::runtime_error("Invalid glyphs array");
                    t.assets.glyphs.clear();
                    for(const auto& item:g)if(auto img=parseImage(root,item))t.assets.glyphs.push_back(std::move(img));
                }
            }
        }
    }
}
bool bytesContainCaseInsensitive(const std::vector<unsigned char>& bytes,const std::string& needle) {
    if(bytes.empty()||needle.empty())return false;
    const std::size_t limit=std::min<std::size_t>(bytes.size(),65536);
    const std::string lowerNeedle=lowerAscii(needle);
    std::string hay;hay.reserve(limit);
    for(std::size_t i=0;i<limit;++i)hay.push_back(char(std::tolower(bytes[i])));
    return hay.find(lowerNeedle)!=std::string::npos;
}
}
Themes::Themes(){Theme a;packs_.push_back(a);a.id="silver";a.name="Skyrim silver";a.accent=a.text;packs_.push_back(a);}
std::string Themes::detect(const ResourceReader& read)const {
    if(!read)return {};
    std::map<std::string,std::vector<unsigned char>> files;
    std::map<std::string,std::string> digests;
    const auto fetch=[&](const std::string& path)->const std::vector<unsigned char>& {
        auto [it,added]=files.try_emplace(path);
        if(added)it->second=read(path);
        return it->second;
    };
    // Tier 1: exact SHA-256 matches win immediately, in pack order.
    for(const auto& pack:packs_)for(const auto& signature:pack.detect) {
        if(signature.kind!=DetectKind::sha256)continue;
        auto [it,added]=digests.try_emplace(signature.file);
        if(added){const auto& bytes=fetch(signature.file);if(!bytes.empty())it->second=sha256(bytes);}
        if(it->second==signature.sha256)return pack.id;
    }
    // Tier 2: exists / contains matches ranked by detectPriority desc, then id asc.
    std::vector<const Theme*> candidates;
    for(const auto& pack:packs_) {
        if(std::any_of(pack.detect.begin(),pack.detect.end(),[](const ThemeSignature& s){return s.kind!=DetectKind::sha256;}))
            candidates.push_back(&pack);
    }
    std::sort(candidates.begin(),candidates.end(),[](const Theme* a,const Theme* b) {
        if(a->detectPriority!=b->detectPriority)return a->detectPriority>b->detectPriority;
        return a->id<b->id;
    });
    for(const auto* pack:candidates)for(const auto& signature:pack->detect) {
        if(signature.kind==DetectKind::sha256)continue;
        const auto& bytes=fetch(signature.file);
        if(bytes.empty())continue;
        if(signature.kind==DetectKind::exists)return pack->id;
        if(signature.kind==DetectKind::contains&&bytesContainCaseInsensitive(bytes,signature.contains))return pack->id;
    }
    return {};
}
std::uint32_t themePanel(const Theme& t) {
    if(t.panel>>24)return t.panel;
    // An opaque surface in the background's hue, lifted slightly so controls separate from it.
    const auto lift=[](std::uint32_t c,unsigned shift){return std::min(255u,((c>>shift)&255)+12);};
    return rgba(lift(t.background,0),lift(t.background,8),lift(t.background,16),250);
}
std::uint32_t themePanelBorder(const Theme& t){return t.panelBorder>>24?t.panelBorder:(t.border&0x00ffffffu)|0xb4000000u;}
const Theme& Themes::get(const std::string& id)const{for(const auto& t:packs_)if(t.id==id)return t;return packs_.front();}
bool Themes::contains(const std::string& id)const{for(const auto& t:packs_)if(t.id==id)return true;return false;}
void Themes::load(const std::filesystem::path& directory) {
    *this=Themes{};std::vector<std::filesystem::path> files;std::error_code ec;
    if(!std::filesystem::is_directory(directory,ec))return;
    for(const auto& entry:std::filesystem::directory_iterator(directory)) {
        if(files.size()>=64){warnings_.push_back("Theme discovery limit reached (64 folders)");break;}
        if(entry.is_directory()&&std::filesystem::is_regular_file(entry.path()/"theme.json"))files.push_back(entry.path()/"theme.json");
    }
    std::sort(files.begin(),files.end());
    struct Pending {
        std::string id,extends;
        unsigned schema=1;
        std::filesystem::path root;
        Json json;
    };
    std::vector<Pending> pending;
    std::map<std::string,std::size_t> byId;
    for(const auto& path:files)try {
        if(std::filesystem::file_size(path)>65536)throw std::runtime_error("Manifest too large");
        std::ifstream f(path);const auto j=Json::parse(f);
        const unsigned schema=j.at("schema").get<unsigned>();
        if(schema<1||schema>3)throw std::runtime_error("Unsupported theme schema");
        const auto id=j.at("id").get<std::string>();
        const auto name=j.at("name").get<std::string>();
        const auto description=j.value("description",std::string{});
        const auto extends=schema>=3?j.value("extends",std::string{}):std::string{};
        if(!validID(id)||name.empty()||name.size()>80||description.size()>512)throw std::runtime_error("Invalid theme identity");
        if(!extends.empty()&&!validID(extends))throw std::runtime_error("Invalid parent theme id");
        if(byId.contains(id))throw std::runtime_error("Duplicate theme id");
        // Validate every field immediately so bad manifests fail deterministically in folder order.
        Theme probe;TrackedFlags probeFlags;
        applyManifest(probe,probeFlags,j,path.parent_path(),schema);
        std::set<std::filesystem::path> missing;
        const auto checkFile=[&](const std::filesystem::path& p) {
            if(!p.empty()&&!std::filesystem::is_regular_file(p)&&missing.insert(p).second)
                warnings_.push_back(id+": missing "+p.filename().string()+"; using fallback");
        };
        for(const auto& p:{probe.atlas,probe.font,probe.titleFont,probe.numberFont})checkFile(p);
        forEachThemeImage(probe.assets,[&](const ThemeImage& img){checkFile(img.path);});
        byId[id]=pending.size();
        pending.push_back({id,extends,schema,path.parent_path(),j});
    }catch(const std::exception& e){warnings_.push_back(path.parent_path().filename().string()+": "+e.what());}

    // Resolve extends inheritance chains (max depth 4, cycle-safe).
    std::map<std::string,std::pair<Theme,TrackedFlags>> builtins;
    for(const auto& b:packs_)builtins[b.id]={b,{}};
    std::map<std::string,std::pair<Theme,TrackedFlags>> resolved;
    enum class Visit {none,visiting,done,failed};
    std::vector<Visit> state(pending.size(),Visit::none);
    std::function<bool(std::size_t,unsigned)> resolve=[&](std::size_t idx,unsigned depth)->bool {
        if(state[idx]==Visit::done)return true;
        if(state[idx]==Visit::failed)return false;
        const auto& p=pending[idx];
        if(state[idx]==Visit::visiting){warnings_.push_back(p.id+": cyclic extends chain");state[idx]=Visit::failed;return false;}
        if(depth>4){warnings_.push_back(p.id+": extends depth limit exceeded");state[idx]=Visit::failed;return false;}
        state[idx]=Visit::visiting;
        Theme base;TrackedFlags flags;
        if(!p.extends.empty()) {
            if(const auto it=byId.find(p.extends);it!=byId.end()) {
                if(!resolve(it->second,depth+1)){state[idx]=Visit::failed;return false;}
                const auto& parent=resolved.at(p.extends);
                base=parent.first;flags=parent.second;
                // A child pack does not inherit its parent's interface auto-detection signatures.
                base.detect.clear();base.extends.clear();
            } else if(const auto bit=builtins.find(p.extends);bit!=builtins.end()) {
                base=bit->second.first;flags=bit->second.second;
            } else {
                warnings_.push_back(p.id+": missing parent theme "+p.extends);
                state[idx]=Visit::failed;return false;
            }
        }
        try {
            applyManifest(base,flags,p.json,p.root,p.schema);
        }catch(const std::exception& e) {
            warnings_.push_back(p.id+": "+e.what());state[idx]=Visit::failed;return false;
        }
        resolved[p.id]={std::move(base),flags};
        state[idx]=Visit::done;return true;
    };
    for(std::size_t i=0;i<pending.size();++i) {
        if(!resolve(i,0))continue;
        auto theme=resolved.at(pending[i].id).first;
        const auto it=std::find_if(packs_.begin(),packs_.end(),[&](const auto& existing){return existing.id==theme.id;});
        if(it!=packs_.end())*it=std::move(theme);else packs_.push_back(std::move(theme));
    }
}
}
