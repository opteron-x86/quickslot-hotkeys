#include "Themes.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <set>

namespace quickslot {
namespace {
using Json=nlohmann::json;
bool validID(const std::string& id){return !id.empty()&&id.size()<=48&&std::all_of(id.begin(),id.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});}
std::filesystem::path asset(const std::filesystem::path& root,const Json& j,const char* key) {
    const auto value=j.value(key,std::string{});if(value.empty())return {};
    if(value.find('\\')!=std::string::npos||value.find(':')!=std::string::npos)throw std::runtime_error("Invalid asset path");
    const std::filesystem::path p(value);if(p.is_absolute())throw std::runtime_error("Absolute asset path");
    for(const auto& part:p)if(part==".."||part==".")throw std::runtime_error("Asset traversal");
    const auto full=root/p;
    const auto relative=std::filesystem::weakly_canonical(full).lexically_relative(std::filesystem::weakly_canonical(root));
    if(relative.empty()||*relative.begin()=="..")throw std::runtime_error("Asset outside theme pack");
    return full;
}
std::uint32_t color(const Json& j,const char* key,std::uint32_t fallback) {
    if(!j.contains(key))return fallback;
    const auto value=j.at(key).get<std::vector<unsigned>>();if(value.size()!=3&&value.size()!=4)throw std::runtime_error("Expected RGB or RGBA color");
    for(auto v:value)if(v>255)throw std::runtime_error("Color outside byte range");
    return rgba(value[0],value[1],value[2],value.size()==4?value[3]:255);
}
}
Themes::Themes(){Theme a;packs_.push_back(a);a.id="silver";a.name="Skyrim silver";a.accent=a.text;packs_.push_back(a);}
const Theme& Themes::get(const std::string& id)const{for(const auto& t:packs_)if(t.id==id)return t;return packs_.front();}
bool Themes::contains(const std::string& id)const{for(const auto& t:packs_)if(t.id==id)return true;return false;}
void Themes::load(const std::filesystem::path& directory) {
    *this=Themes{};std::vector<std::filesystem::path> files;std::error_code ec;
    if(!std::filesystem::is_directory(directory,ec))return;
    for(const auto& entry:std::filesystem::directory_iterator(directory)) {
        if(files.size()>=64){warnings_.push_back("Theme discovery limit reached (64 folders)");break;}
        if(entry.is_directory()&&std::filesystem::is_regular_file(entry.path()/"theme.json"))files.push_back(entry.path()/"theme.json");
    }
    std::sort(files.begin(),files.end());std::set<std::string> seen;
    for(const auto& path:files)try {
        if(std::filesystem::file_size(path)>16384)throw std::runtime_error("Manifest too large");
        std::ifstream f(path);const auto j=Json::parse(f);if(j.at("schema")!=1)throw std::runtime_error("Unsupported theme schema");
        Theme t;t.id=j.at("id").get<std::string>();t.name=j.at("name").get<std::string>();t.description=j.value("description",std::string{});
        if(!validID(t.id)||t.name.empty()||t.name.size()>80||t.description.size()>512)throw std::runtime_error("Invalid theme identity");
        if(seen.contains(t.id))throw std::runtime_error("Duplicate theme id");
        t.frameStyle=std::min(j.value("frameStyle",0u),3u);
        const auto palette=j.value("palette",Json::object());
        t.text=color(palette,"text",t.text);t.secondary=color(palette,"secondary",t.secondary);t.muted=color(palette,"muted",t.muted);
        t.accent=color(palette,"accent",t.accent);t.background=color(palette,"background",t.background);t.border=color(palette,"border",t.border);
        t.atlas=asset(path.parent_path(),j,"atlas");t.font=asset(path.parent_path(),j,"font");t.titleFont=asset(path.parent_path(),j,"titleFont");
        for(const auto& p:{t.atlas,t.font,t.titleFont})if(!p.empty()&&!std::filesystem::is_regular_file(p))warnings_.push_back(t.id+": missing "+p.filename().string()+"; using fallback");
        seen.insert(t.id);const auto it=std::find_if(packs_.begin(),packs_.end(),[&](const auto& p){return p.id==t.id;});
        if(it!=packs_.end())*it=std::move(t);else packs_.push_back(std::move(t));
    }catch(const std::exception& e){warnings_.push_back(path.parent_path().filename().string()+": "+e.what());}
}
}
