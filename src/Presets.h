#pragma once
#include "Settings.h"
#include "EditorModel.h"
#include "Layout.h"
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

// Settings fall into the look of the HUD, the controls, the group setup and personal preferences.
// A preset can supply any of the first three; preferences always stay with the player.
namespace presetPart {constexpr unsigned look=1,controls=2,groups=4,all=7;}
inline void copyControls(Options& target,const Options& source) {
    target.keys=source.keys;target.editorKey=source.editorKey;target.editorModifiers=source.editorModifiers;
    target.skipEmpty=source.skipEmpty;target.cycleKeepOtherHand=source.cycleKeepOtherHand;target.holdDelay=source.holdDelay;target.cycleDelay=source.cycleDelay;
    target.menuCapture=source.menuCapture;target.selectionSpeed=source.selectionSpeed;
    target.groupUnequipHotkey=source.groupUnequipHotkey;target.groupRemoveHotkey=source.groupRemoveHotkey;
    target.groupCaptureRightHotkey=source.groupCaptureRightHotkey;target.groupCaptureLeftHotkey=source.groupCaptureLeftHotkey;target.groupMoveHotkey=source.groupMoveHotkey;
    target.selectorReorderHotkey=source.selectorReorderHotkey;target.selectorUnequipHotkey=source.selectorUnequipHotkey;target.selectorRemoveHotkey=source.selectorRemoveHotkey;
    target.groupModifiers=source.groupModifiers;target.selectorBindings=source.selectorBindings;target.allowUnusedModifiers=source.allowUnusedModifiers;
    target.controllerActions=source.controllerActions;
    target.mouseActions=source.mouseActions;
    target.voiceHotkey=source.voiceHotkey;target.shoutCharge=source.shoutCharge;
    target.controllerEnabled=source.controllerEnabled;target.controllerBumpers=source.controllerBumpers;
    target.quickLootKeyboard=source.quickLootKeyboard;target.quickLootController=source.quickLootController;
    target.controllerModifiers=source.controllerModifiers;
    target.consumableKey=source.consumableKey;target.swapKey=source.swapKey;target.doubleTapSwap=source.doubleTapSwap;target.controllerUse=source.controllerUse;target.vrUse=source.vrUse;
    target.controllerGroups=source.controllerGroups;target.controllerEditor=source.controllerEditor;
    target.controllerStick=source.controllerStick;target.controllerDeadzone=source.controllerDeadzone;target.controllerStyle=source.controllerStyle;
    target.vrGroups=source.vrGroups;target.vrEditor=source.vrEditor;target.vrStick=source.vrStick;
}
inline void copyGroupSetup(Options& target,const Options& source) {
    target.names=source.names;target.groupCount=source.groupCount;target.groupOrder=source.groupOrder;
    target.groupModes=source.groupModes;target.separateConsumableUse=source.separateConsumableUse;target.autoSort=source.autoSort;
}
inline void copyPreferences(Options& target,const Options& source) {
    target.language=source.language;target.uiSounds=source.uiSounds;target.activePreset=source.activePreset;
}
inline void importAppearance(Options& target,const Options& source) {
    const auto previous=target;target=source;
    copyControls(target,previous);copyGroupSetup(target,previous);copyPreferences(target,previous);
}
// The current settings with the chosen parts taken from a preset's settings.
inline Options mergeOptions(const Options& current,const Options& source,unsigned parts) {
    auto result=current;
    if(parts&presetPart::look)importAppearance(result,source);
    if(parts&presetPart::controls)copyControls(result,source);
    if(parts&presetPart::groups)copyGroupSetup(result,source);
    return result;
}
// Built-in presets ship with the mod and cannot be changed. Default restores the whole look;
// the layouts rearrange the widget, selector and equipment display and keep everything else.
struct BuiltinPreset {const char* id;const char* name;};
inline constexpr BuiltinPreset builtinPresets[]{{"builtin:default","Default"},{"builtin:cross","Cross"},{"builtin:diamond","Diamond"},{"builtin:circle","Circle"}};
inline const BuiltinPreset* builtinPreset(std::string_view id) {
    for(const auto& preset:builtinPresets)if(id==preset.id)return &preset;
    return nullptr;
}
inline Options applyBuiltin(const Options& current,std::string_view id) {
    auto result=current;
    if(id=="builtin:default")importAppearance(result,Options{});
    else if(const auto* preset=builtinPreset(id)){const auto layout=unsigned(preset-builtinPresets)-1;layoutPreset(result,layout);equipmentLayoutPreset(result,layout);}
    return result;
}
// True when applying the preset's look would change nothing.
inline bool lookMatches(const Options& current,const Options& look) {return encodeOptions(mergeOptions(current,look,presetPart::look))==encodeOptions(current);}
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
inline PresetImport importPreset(const std::string& bytes,const Arsenal& current,const Options& options,unsigned parts,const FindForm& find) {
    if(bytes.size()>8388608)throw std::runtime_error("Preset is too large");
    const auto j=Json::parse(bytes);
    if(j.at("format")!="Quickslot preset"||j.at("version")!=1||(j.at("scope")!="appearance"&&j.at("scope")!="complete"))throw std::runtime_error("Unsupported preset");
    if(j.at("scope")=="appearance")parts&=~presetPart::groups;
    PresetImport result{current,options,0,!(parts&presetPart::groups)};
    const auto imported=decodeOptions(j.at("options").dump());
    result.options=mergeOptions(options,imported,parts);
    if(result.appearanceOnly)return result;
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
    for(auto& group:result.arsenal.groups)eachGroupEntry(group,[&](auto& entry){eachLeaf(entry,[&](Entry& e){detach(e);if(e.unresolved)++result.unresolved;});});
    for(auto& entry:result.arsenal.shortcuts)eachLeaf(entry,detach);
    revealSavedGroups(result.options,result.arsenal);return result;
}
inline PresetImport importPreset(const std::string& bytes,const Arsenal& current,const Options& options,bool appearanceOnly,const FindForm& find) {
    return importPreset(bytes,current,options,appearanceOnly?presetPart::look:presetPart::all,find);
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
// What the editor shows for each preset file: its scope and, for previews, its settings.
inline std::map<std::string,PresetInfo> describePresets(const std::filesystem::path& folder,const std::vector<std::string>& names) {
    std::map<std::string,PresetInfo> result;
    for(const auto& name:names)try {
        const auto path=presetPath(folder,name);
        if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>1048576)continue;
        const auto j=Json::parse(readPreset(folder,name));
        if(j.at("format")!="Quickslot preset"||j.at("version")!=1)continue;
        result[name]={j.at("scope")=="complete",decodeOptions(j.at("options").dump())};
    }catch(const std::exception&){}
    return result;
}
}
