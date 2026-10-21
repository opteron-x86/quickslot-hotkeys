#pragma once
#include "Settings.h"
#include <filesystem>
#include <fstream>
#include <set>

namespace quickslot {
inline bool validPresetName(const std::string& name) {
    if(!validUTF8(name)||name.empty()||name.size()>64||name.front()==' '||name.back()==' ')return false;
    for(unsigned char c:name)if(c<128&&!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='_'||c=='-'))return false;
    std::string upper=name;for(auto& c:upper)c=char(std::toupper(static_cast<unsigned char>(c)));
    if(upper=="CON"||upper=="PRN"||upper=="AUX"||upper=="NUL")return false;
    if(upper.size()==4&&(upper.starts_with("COM")||upper.starts_with("LPT"))&&upper[3]>='0'&&upper[3]<='9')return false;
    return true;
}

inline void importAppearance(Options& target,const Options& source) {
    const auto previous=target;target=source;
    target.language=previous.language;target.keys=previous.keys;target.names=previous.names;target.groupCount=previous.groupCount;target.groupOrder=previous.groupOrder;
    target.editorKey=previous.editorKey;target.editorModifiers=previous.editorModifiers;
    target.skipEmpty=previous.skipEmpty;
    target.menuCapture=previous.menuCapture;target.selectionSpeed=previous.selectionSpeed;
    target.voiceHotkey=previous.voiceHotkey;target.shoutCharge=previous.shoutCharge;
    target.controllerEnabled=previous.controllerEnabled;target.controllerBumpers=previous.controllerBumpers;
    target.controllerModifiers=previous.controllerModifiers;
    target.groupModes=previous.groupModes;
    target.controllerGroups=previous.controllerGroups;target.controllerEditor=previous.controllerEditor;
    target.controllerStick=previous.controllerStick;target.controllerDeadzone=previous.controllerDeadzone;target.controllerStyle=previous.controllerStyle;
    target.vrGroups=previous.vrGroups;target.vrEditor=previous.vrEditor;target.vrStick=previous.vrStick;
}
using DescribeForm=std::function<Json(std::uint32_t)>;
using FindForm=std::function<std::uint32_t(const std::string&,std::uint32_t)>;
inline std::string exportPreset(const Arsenal& arsenal,const Options& options,bool appearanceOnly,const DescribeForm& describe) {
    Json j={{"format","Quickslot preset"},{"version",1},{"scope",appearanceOnly?"appearance":"complete"},{"options",Json::parse(encodeOptions(options))}};
    if(!appearanceOnly) {
        j["arsenal"]=Json::parse(encodeArsenal(arsenal));j["references"]=Json::object();
        const auto reference=[&](std::uint32_t id){if(id)j["references"][std::to_string(id)]=describe(id);};
        eachSavedEntry(arsenal,[&](const auto& entry){eachLeaf(entry,[&](const Entry& e){reference(e.form);reference(e.owner);for(const auto& x:e.exclusions)reference(x.form);});});
    }
    const auto bytes=j.dump(2);if(bytes.size()>8388608)throw std::runtime_error("Preset is too large");return bytes;
}
struct PresetImport {Arsenal arsenal;Options options;std::size_t unresolved=0;bool appearanceOnly=false;};
inline PresetImport importPreset(const std::string& bytes,const Arsenal& current,const Options& options,bool appearanceOnly,const FindForm& find) {
    if(bytes.size()>8388608)throw std::runtime_error("Preset is too large");
    const auto j=Json::parse(bytes);
    if(j.at("format")!="Quickslot preset"||j.at("version")!=1||(j.at("scope")!="appearance"&&j.at("scope")!="complete"))throw std::runtime_error("Unsupported preset");
    PresetImport result{current,options,0,appearanceOnly||j.at("scope")=="appearance"};
    const auto imported=decodeOptions(j.at("options").dump());
    if(result.appearanceOnly){importAppearance(result.options,imported);return result;}
    const auto& refs=j.at("references");if(!refs.is_object()||refs.size()>(Arsenal::count*Group::limit+Arsenal::shortcutLimit)*132)throw std::runtime_error("Invalid preset references");
    const auto scope=j.at("arsenal").at("presetIdentity").get<std::string>();
    const bool sameCharacter=!current.presetIdentity.empty()&&scope==current.presetIdentity;
    result.arsenal=decodeArsenal(j.at("arsenal").dump(),[&](std::uint32_t old,std::uint32_t& mapped){
        const auto it=refs.find(std::to_string(old));if(it==refs.end())return false;
        const auto& ref=*it;if(ref.is_null())return false;
        if(ref.contains("dynamic")) {
            if(!ref.at("dynamic").is_number_unsigned()||ref.at("dynamic").get<std::uint64_t>()!=old||(old>>24)!=0xff)return false;
            mapped=sameCharacter?old:0;return mapped!=0;
        }
        const auto file=ref.at("file").get<std::string>();
        if(file.empty()||file.size()>260||file.find_first_of("/\\:")!=std::string::npos||file.find('\0')!=std::string::npos)throw std::runtime_error("Invalid plugin name");
        if(!ref.at("local").is_number_unsigned()||ref.at("local").get<std::uint64_t>()>0xffffff)throw std::runtime_error("Invalid local form");
        mapped=find(file,ref.at("local").get<std::uint32_t>());return mapped!=0;
    });
    result.arsenal.presetIdentity=current.presetIdentity;

    const auto detach=[&](Entry& e){if(!sameCharacter&&tracksInstance(e)){e.unresolved=true;e.form=e.owner=e.unique=0;}};
    for(auto& group:result.arsenal.groups)for(auto& entry:group.entries)eachLeaf(entry,[&](Entry& e){detach(e);if(e.unresolved)++result.unresolved;});
    for(auto& entry:result.arsenal.shortcuts)eachLeaf(entry,detach);
    result.options=imported;revealSavedGroups(result.options,result.arsenal);return result;
}
inline std::filesystem::path presetPath(const std::filesystem::path& folder,const std::string& name) {
    if(!validPresetName(name))throw std::runtime_error("Use letters, numbers, spaces, hyphens or underscores for the preset name");
    if(std::filesystem::is_symlink(folder))throw std::runtime_error("Preset folder must not be a symbolic link");
    const auto path=folder/std::filesystem::u8path(name+".json");if(std::filesystem::is_symlink(path))throw std::runtime_error("Preset must not be a symbolic link");return path;
}
inline std::vector<std::string> listPresets(const std::filesystem::path& folder) {
    std::vector<std::string> names;if(!std::filesystem::exists(folder))return names;
    if(std::filesystem::is_symlink(folder))throw std::runtime_error("Preset folder must not be a symbolic link");
    for(const auto& entry:std::filesystem::directory_iterator(folder)) {
        if(entry.is_symlink()||!entry.is_regular_file()||entry.path().extension()!=".json")continue;
        const auto utf8=entry.path().stem().u8string();const std::string name(utf8.begin(),utf8.end());if(validPresetName(name))names.push_back(name);
        if(names.size()>=256)break;
    }
    std::sort(names.begin(),names.end());return names;
}
inline std::string readPreset(const std::filesystem::path& folder,const std::string& name) {
    const auto path=presetPath(folder,name);
    if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>8388608)throw std::runtime_error("Invalid preset file");
    std::ifstream file(path,std::ios::binary);std::string bytes;bytes.resize(std::size_t(std::filesystem::file_size(path)));
    file.read(bytes.data(),std::streamsize(bytes.size()));if(!file||file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Could not read preset");return bytes;
}
}
