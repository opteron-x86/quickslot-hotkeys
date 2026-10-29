#pragma once
#include "Storage.h"
#include "VRInput.h"

namespace quickslot {
inline void sanitizeOptions(Options& o) {
    if(!languageID(o.language))o.language="auto";
    o.groupCount=std::clamp(o.groupCount,std::size_t(1),Arsenal::count);
    std::array<bool,Arsenal::count> seen{};
    bool validOrder=true;for(const auto id:o.groupOrder){if(id>=Arsenal::count||seen[id]){validOrder=false;break;}seen[id]=true;}
    if(!validOrder)for(std::size_t i=0;i<Arsenal::count;++i)o.groupOrder[i]=i;
    if(o.editorKey&&(!validGroupKey(o.editorKey)||std::any_of(o.groupOrder.begin(),o.groupOrder.begin()+std::ptrdiff_t(o.groupCount),[&](auto id){return o.keys[id]==o.editorKey;})))o.editorKey=0;
    o.editorModifiers=o.editorKey?(o.editorModifiers&keyModifier::all):0;
    std::vector<unsigned> assigned;if(o.editorKey)assigned.push_back(o.editorKey);
    for(const auto id:o.groupOrder) {
        auto& key=o.keys[id];
        if(key&&(!validGroupKey(key)||std::find(assigned.begin(),assigned.end(),key)!=assigned.end()))key=0;
        if(key)assigned.push_back(key);
    }
    for(unsigned i=0;i<o.groupModifiers.size();++i) {
        o.groupModifiers[i]&=keyModifier::all;
        if(!o.groupModifiers[i])o.groupModifiers[i]=defaultGroupModifiers[i];
    }
    std::vector<unsigned> groupModifiers;
    for(const auto action:{GroupModifier::captureRight,GroupModifier::captureLeft,GroupModifier::unequip,GroupModifier::remove}) {
        const auto mask=o.groupModifiers[unsigned(action)];
        if(std::find(groupModifiers.begin(),groupModifiers.end(),mask)!=groupModifiers.end()){o.groupModifiers=defaultGroupModifiers;break;}
        groupModifiers.push_back(mask);
    }
    std::vector<unsigned> selectorAssigned;
    for(unsigned i=0;i<o.selectorBindings.size();++i) {
        auto& binding=o.selectorBindings[i];if(!selectorActionEnabled(o,KeyboardAction(i)))binding={};
        binding.modifiers&=keyModifier::all;
        if(binding.key>255||modifierForScan(binding.key)||std::find(assigned.begin(),assigned.end(),binding.key)!=assigned.end()||
            std::find(selectorAssigned.begin(),selectorAssigned.end(),binding.key)!=selectorAssigned.end())binding={};
        if(!binding.key)binding.modifiers=0;else selectorAssigned.push_back(binding.key);
    }
    std::vector<int> mouseAssigned;
    for(auto& button:o.mouseActions) {
        if(button< -1||button>4||std::find(mouseAssigned.begin(),mouseAssigned.end(),button)!=mouseAssigned.end())button=-1;
        if(button>=0)mouseAssigned.push_back(button);
    }
    o.selectionSpeed=std::isfinite(o.selectionSpeed)?(o.selectionSpeed==0?0:std::clamp(o.selectionSpeed,.1f,1.f)):1.f;
    o.controllerStyle=std::min(o.controllerStyle,1u);o.controllerStick=std::min(o.controllerStick,2u);
    o.controllerModifiers&=allPadButtons;
    for(auto& mode:o.groupModes)if(unsigned(mode)>unsigned(GroupMode::cycle))mode=GroupMode::browse;
    if(!validPadBinding(o.controllerEditor))o.controllerEditor={};
    std::vector<PadBinding> padAssigned;if(o.controllerEditor.button!=PadButton::none)padAssigned.push_back(o.controllerEditor);
    for(const auto id:o.groupOrder) {
        auto& binding=o.controllerGroups[id];
        if(!validPadBinding(binding)||(binding.button!=PadButton::none&&std::find(padAssigned.begin(),padAssigned.end(),binding)!=padAssigned.end()))binding={};
        if(binding.button!=PadButton::none)padAssigned.push_back(binding);
    }
    std::vector<PadBinding> actionAssigned;
    for(auto& binding:o.controllerActions) {
        if(!validPadBinding(binding)||std::find(actionAssigned.begin(),actionAssigned.end(),binding)!=actionAssigned.end())binding={};
        if(binding.button!=PadButton::none)actionAssigned.push_back(binding);
    }
    o.vrStick=std::min(o.vrStick,2u);
    const auto validVR=[](PadBinding b){return validPadBinding(b)&&validVRButton(b.button)&&validVRButton(b.modifier);};
    if(!validVR(o.vrEditor))o.vrEditor={};
    std::vector<PadBinding> vrAssigned;if(o.vrEditor.button!=PadButton::none)vrAssigned.push_back(o.vrEditor);
    for(const auto id:o.groupOrder) {
        auto& b=o.vrGroups[id];
        if(!validVR(b)||(b.button!=PadButton::none&&std::find(vrAssigned.begin(),vrAssigned.end(),b)!=vrAssigned.end()))b={};
        if(b.button!=PadButton::none)vrAssigned.push_back(b);
    }
    o.layout=std::min(o.layout,5u);o.shape=std::min(o.shape,3u);o.selector=std::min(o.selector,4u);
    o.selectorDock=std::min(o.selectorDock,4u);o.selectorShape=std::min(o.selectorShape,3u);
    o.itemIconColor=std::min(o.itemIconColor,2u);
    const auto finite=[](float v,float fallback,float min,float max){return std::isfinite(v)?std::clamp(v,min,max):fallback;};
    o.inactiveGroups=std::min(o.inactiveGroups,2u);
    o.inactiveGroupOpacity=finite(o.inactiveGroupOpacity,.3f,.05f,.9f);
    o.controllerDeadzone=finite(o.controllerDeadzone,.45f,.2f,.9f);o.shoutCharge=finite(o.shoutCharge,1.2f,.05f,2.5f);
    o.scale=finite(o.scale,1,.55f,1.5f);o.textScale=finite(o.textScale,1,.75f,1.5f);o.opacity=finite(o.opacity,.92f,.4f,1);
    o.placementSpacing=std::min(o.placementSpacing,3u);o.placementFrame=std::min(o.placementFrame,3u);
    o.placementGridSize=finite(o.placementGridSize,16,4,128);
    o.spacing=finite(o.spacing,14,0,128);o.selectorSpacing=finite(o.selectorSpacing,16,0,128);
    o.positionX=finite(o.positionX,1,0,1);o.positionY=finite(o.positionY,1,0,1);
    o.equipmentX=finite(o.equipmentX,.02f,0,1);o.equipmentY=finite(o.equipmentY,.8f,0,1);
    o.equipmentScale=finite(o.equipmentScale,1,.5f,2.f);o.equipmentTextScale=finite(o.equipmentTextScale,1,.75f,1.5f);
    o.equipmentSpacing=finite(o.equipmentSpacing,8,0,128);
    o.equipmentStyle=std::min(o.equipmentStyle,4u);o.equipmentLayout=std::min(o.equipmentLayout,5u);o.equipmentTextPosition=std::min(o.equipmentTextPosition,3u);
    o.equipmentFadeDelay=finite(o.equipmentFadeDelay,4,0,20);o.equipmentIdleOpacity=finite(o.equipmentIdleOpacity,.35f,.05f,.9f);
    o.selectorGap=finite(o.selectorGap,12,0,100);o.selectorX=finite(o.selectorX,.75f,0,1);o.selectorY=finite(o.selectorY,.4f,0,1);
    o.fadeDelay=finite(o.fadeDelay,4,0,20);o.idleOpacity=finite(o.idleOpacity,.35f,0,1);
    const auto validId=[](const std::string& id){return !id.empty()&&id.size()<=48&&std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});};
    if(!validId(o.themeId))o.themeId="minimal";
    for(auto it=o.themeAdjustments.begin();it!=o.themeAdjustments.end();) {
        if(!validId(it->first)){it=o.themeAdjustments.erase(it);continue;}
        auto& a=it->second;for(auto& c:a.accent)c=finite(c,.5f,0,1);
        a.frameOpacity=finite(a.frameOpacity,1,0,1);a.iconScale=finite(a.iconScale,1,.65f,1.2f);++it;
    }
    while(o.themeAdjustments.size()>32)o.themeAdjustments.erase(std::prev(o.themeAdjustments.end()));
    for(auto& name:o.names){name=cleanName(name);if(name.size()>64){std::size_t end=64;while(end>0&&(static_cast<unsigned char>(name[end])&0xc0)==0x80)--end;name.resize(end);}}
}
inline std::string encodeOptions(Options o) {
    sanitizeOptions(o);
    Json tweaks=Json::object();for(const auto& [id,a]:o.themeAdjustments)tweaks[id]={{"customAccent",a.customAccent},{"accent",a.accent},{"frameOpacity",a.frameOpacity},{"iconScale",a.iconScale}};
    const auto binding=[](PadBinding b){return Json::array({unsigned(b.button),unsigned(b.modifier)});};
    Json padGroups=Json::array();for(const auto b:o.controllerGroups)padGroups.push_back(binding(b));
    Json vrGroups=Json::array();for(const auto b:o.vrGroups)vrGroups.push_back(binding(b));
    Json modes=Json::array();for(const auto mode:o.groupModes)modes.push_back(unsigned(mode));
    Json selectorBindings=Json::array();for(const auto b:o.selectorBindings)selectorBindings.push_back(Json::array({b.key,b.modifiers}));
    Json controllerActions=Json::array();for(const auto b:o.controllerActions)controllerActions.push_back(binding(b));
    return Json{{"schema",1},{"language",o.language},{"groupOrder",o.groupOrder},{"skipEmpty",o.skipEmpty},{"menuCapture",o.menuCapture},{"selectionSpeed",o.selectionSpeed},{"themeId",o.themeId},{"themeAdjustments",tweaks},
        {"voiceHotkey",o.voiceHotkey},{"shoutCharge",o.shoutCharge},{"groupUnequipHotkey",o.groupUnequipHotkey},{"groupRemoveHotkey",o.groupRemoveHotkey},
        {"quickLootKeyboard",o.quickLootKeyboard},{"quickLootController",o.quickLootController},
        {"groupCaptureRightHotkey",o.groupCaptureRightHotkey},{"groupCaptureLeftHotkey",o.groupCaptureLeftHotkey},{"groupMoveHotkey",o.groupMoveHotkey},
        {"selectorReorderHotkey",o.selectorReorderHotkey},{"selectorUnequipHotkey",o.selectorUnequipHotkey},{"selectorRemoveHotkey",o.selectorRemoveHotkey},
        {"mouseActions",o.mouseActions},{"groupModifiers",o.groupModifiers},{"selectorBindings",selectorBindings},{"allowUnusedModifiers",o.allowUnusedModifiers},{"controllerActions",controllerActions},
        {"controllerEnabled",o.controllerEnabled},{"controllerBumpers",o.controllerBumpers},{"controllerStyle",o.controllerStyle},{"controllerStick",o.controllerStick},{"controllerDeadzone",o.controllerDeadzone},{"controllerGroups",padGroups},{"controllerEditor",binding(o.controllerEditor)},
        {"groupModes",modes},{"autoSort",o.autoSort},{"controllerModifiers",o.controllerModifiers},{"vrGroups",vrGroups},{"vrEditor",binding(o.vrEditor)},{"vrStick",o.vrStick},
        {"showEquipment",o.showEquipment},{"showEquipmentAmmo",o.showEquipmentAmmo},{"equipmentX",o.equipmentX},{"equipmentY",o.equipmentY},{"hideEmptyGroups",o.hideEmptyGroups},{"inactiveGroups",o.inactiveGroups},{"inactiveGroupOpacity",o.inactiveGroupOpacity},
        {"equipmentDim",o.equipmentDim},{"equipmentFadeDelay",o.equipmentFadeDelay},{"equipmentIdleOpacity",o.equipmentIdleOpacity},
        {"equipmentScale",o.equipmentScale},{"equipmentTextScale",o.equipmentTextScale},{"equipmentSpacing",o.equipmentSpacing},
        {"layoutVersion",2},{"equipmentStyle",o.equipmentStyle},{"equipmentLayout",o.equipmentLayout},{"equipmentTextPosition",o.equipmentTextPosition},{"equipmentShowText",o.equipmentShowText},
        {"itemIconColor",o.itemIconColor},
        {"selectorDock",o.selectorDock},{"selectorGap",o.selectorGap},{"selectorX",o.selectorX},{"selectorY",o.selectorY},
        {"animateScroll",o.animateScroll},{"fadeIdle",o.fadeIdle},{"keepCombatVisible",o.keepCombatVisible},{"keepDrawnVisible",o.keepDrawnVisible},{"fadeDelay",o.fadeDelay},{"idleOpacity",o.idleOpacity},{"keys",o.keys},{"names",o.names},{"editorKey",o.editorKey},{"editorModifiers",o.editorModifiers},{"groupCount",o.groupCount},
        {"layout",o.layout},{"shape",o.shape},{"selector",o.selector},{"scale",o.scale},{"textScale",o.textScale},{"opacity",o.opacity},
        {"placementSpacing",o.placementSpacing},{"placementFrame",o.placementFrame},{"placementGridSize",o.placementGridSize},
        {"spacing",o.spacing},{"positionX",o.positionX},{"positionY",o.positionY},{"showIdle",o.showIdle},{"showHints",o.showHints},
        {"showPoison",o.showPoison},{"showAmmo",o.showAmmo},
        {"selectorShape",o.selectorShape},{"selectorSpacing",o.selectorSpacing},{"showLabels",o.showLabels},{"showCardNames",o.showCardNames},{"showKeys",o.showKeys},{"placementGrid",o.placementGrid},{"placementSnap",o.placementSnap},{"keyIcons",o.keyIcons},{"coloredMagic",o.coloredMagic}}.dump(2);
}
inline Options decodeOptions(const std::string& bytes) {
    if(bytes.size()>65536)throw std::runtime_error("Settings file too large");
    const auto j=Json::parse(bytes);if(j.at("schema")!=1)throw std::runtime_error("Unsupported settings version");
    Options o;o.language=j.value("language",std::string("auto"));o.keys=j.at("keys").get<decltype(o.keys)>();o.names=j.at("names").get<decltype(o.names)>();
    o.editorKey=j.at("editorKey").get<unsigned>();o.groupCount=j.at("groupCount").get<std::size_t>();
    o.editorModifiers=j.at("editorModifiers").get<decltype(o.editorModifiers)>();
    o.groupOrder=j.at("groupOrder").get<decltype(o.groupOrder)>();
    o.skipEmpty=j.at("skipEmpty").get<decltype(o.skipEmpty)>();o.menuCapture=j.at("menuCapture").get<decltype(o.menuCapture)>();o.selectionSpeed=j.at("selectionSpeed").get<decltype(o.selectionSpeed)>();
    o.voiceHotkey=j.at("voiceHotkey").get<decltype(o.voiceHotkey)>();o.shoutCharge=j.at("shoutCharge").get<decltype(o.shoutCharge)>();
    o.controllerEnabled=j.at("controllerEnabled").get<decltype(o.controllerEnabled)>();o.controllerBumpers=j.at("controllerBumpers").get<decltype(o.controllerBumpers)>();
    o.controllerModifiers=j.value("controllerModifiers",allPadButtons);
    o.autoSort=j.value("autoSort",o.autoSort);
    o.groupUnequipHotkey=j.value("groupUnequipHotkey",true);o.groupRemoveHotkey=j.value("groupRemoveHotkey",true);
    o.quickLootKeyboard=j.value("quickLootKeyboard",false);o.quickLootController=j.value("quickLootController",false);
    o.groupCaptureRightHotkey=j.value("groupCaptureRightHotkey",true);o.groupCaptureLeftHotkey=j.value("groupCaptureLeftHotkey",true);o.groupMoveHotkey=j.value("groupMoveHotkey",true);
    o.selectorReorderHotkey=j.value("selectorReorderHotkey",true);o.selectorUnequipHotkey=j.value("selectorUnequipHotkey",true);o.selectorRemoveHotkey=j.value("selectorRemoveHotkey",true);
    o.groupModifiers=j.value("groupModifiers",o.groupModifiers);o.allowUnusedModifiers=j.value("allowUnusedModifiers",false);
    o.mouseActions=j.value("mouseActions",o.mouseActions);
    if(j.contains("selectorBindings")) {
        const auto& bindings=j.at("selectorBindings");
        if(!bindings.is_array()||(bindings.size()!=5&&bindings.size()!=o.selectorBindings.size()))throw std::runtime_error("Invalid keyboard actions");
        for(unsigned i=0;i<bindings.size();++i){const auto b=bindings[i].get<std::array<unsigned,2>>();o.selectorBindings[i]={b[0],b[1]};}
    }
    if(j.contains("groupModes")) {
        const auto modes=j.at("groupModes").get<std::array<unsigned,Arsenal::count>>();
        for(std::size_t i=0;i<Arsenal::count;++i)o.groupModes[i]=GroupMode(modes[i]);
    }
    o.showEquipment=j.value("showEquipment",false);o.showEquipmentAmmo=j.value("showEquipmentAmmo",false);o.hideEmptyGroups=j.value("hideEmptyGroups",false);
    o.equipmentX=j.value("equipmentX",.02f);o.equipmentY=j.value("equipmentY",.8f);
    o.equipmentScale=j.value("equipmentScale",1.f);o.equipmentTextScale=j.value("equipmentTextScale",1.f);
    o.equipmentSpacing=j.value("equipmentSpacing",j.value("layoutVersion",1)<2?1.f:8.f);
    o.equipmentDim=j.value("equipmentDim",false);o.equipmentFadeDelay=j.value("equipmentFadeDelay",4.f);o.equipmentIdleOpacity=j.value("equipmentIdleOpacity",.35f);
    o.equipmentStyle=j.value("equipmentStyle",0u);o.equipmentLayout=j.value("equipmentLayout",j.value("layoutVersion",1)<2?0u:1u);o.equipmentTextPosition=j.value("equipmentTextPosition",3u);
    o.equipmentShowText=j.value("equipmentShowText",true);
    if(j.value("layoutVersion",1)<2) {
        o.equipmentSpacing*=o.equipmentStyle==0?4.f:o.equipmentLayout==1?12.f:8.f;
        o.equipmentLayout=o.equipmentStyle==0?1u:o.equipmentLayout==1?0u:1u;
    }
    o.inactiveGroups=j.value("inactiveGroups",0u);o.inactiveGroupOpacity=j.value("inactiveGroupOpacity",.3f);
    o.controllerStyle=j.at("controllerStyle").get<decltype(o.controllerStyle)>();o.controllerStick=j.at("controllerStick").get<decltype(o.controllerStick)>();o.controllerDeadzone=j.at("controllerDeadzone").get<decltype(o.controllerDeadzone)>();
    const auto binding=[](const Json& value){const auto b=value.get<std::array<unsigned,2>>();return PadBinding{PadButton(b[0]),PadButton(b[1])};};
    if(j.contains("controllerActions")) {
        const auto& actions=j.at("controllerActions");
        if(!actions.is_array()||(actions.size()!=4&&actions.size()!=o.controllerActions.size()))throw std::runtime_error("Invalid controller actions");
        for(unsigned i=0;i<actions.size();++i)o.controllerActions[i]=binding(actions[i]);
    }
    {const auto& groups=j.at("controllerGroups");if(!groups.is_array()||groups.size()!=Arsenal::count)throw std::runtime_error("Invalid controller bindings");for(std::size_t i=0;i<Arsenal::count;++i)o.controllerGroups[i]=binding(groups[i]);}
    o.controllerEditor=binding(j.at("controllerEditor"));
    {const auto& groups=j.at("vrGroups");if(!groups.is_array()||groups.size()!=Arsenal::count)throw std::runtime_error("Invalid VR bindings");for(std::size_t i=0;i<Arsenal::count;++i)o.vrGroups[i]=binding(groups[i]);}
    o.vrEditor=binding(j.at("vrEditor"));
    o.vrStick=j.at("vrStick").get<decltype(o.vrStick)>();
    o.itemIconColor=j.at("itemIconColor").get<decltype(o.itemIconColor)>();
    o.layout=j.at("layout").get<unsigned>();o.shape=j.at("shape").get<unsigned>();o.selector=j.at("selector").get<unsigned>();
    o.scale=j.at("scale").get<float>();o.opacity=j.at("opacity").get<float>();o.spacing=j.at("spacing").get<float>();
    o.textScale=j.value("textScale",1.f);o.placementSpacing=j.value("placementSpacing",1u);o.placementFrame=j.value("placementFrame",0u);o.placementGridSize=j.value("placementGridSize",16.f);
    o.positionX=j.at("positionX").get<float>();o.positionY=j.at("positionY").get<float>();
    o.showIdle=j.at("showIdle").get<bool>();
    o.showHints=j.at("showHints").get<decltype(o.showHints)>();
    o.showPoison=j.value("showPoison",true);o.showAmmo=j.value("showAmmo",true);
    o.selectorShape=j.at("selectorShape").get<decltype(o.selectorShape)>();o.selectorSpacing=j.at("selectorSpacing").get<decltype(o.selectorSpacing)>();
    o.showKeys=j.at("showKeys").get<decltype(o.showKeys)>();o.placementGrid=j.at("placementGrid").get<decltype(o.placementGrid)>();o.placementSnap=j.at("placementSnap").get<decltype(o.placementSnap)>();
    o.showLabels=j.at("showLabels").get<decltype(o.showLabels)>();o.showCardNames=j.at("showCardNames").get<decltype(o.showCardNames)>();o.keyIcons=j.at("keyIcons").get<decltype(o.keyIcons)>();o.coloredMagic=j.at("coloredMagic").get<decltype(o.coloredMagic)>();
    o.themeId=j.at("themeId").get<decltype(o.themeId)>();
    {
        const auto& tweaks=j.at("themeAdjustments");if(!tweaks.is_object()||tweaks.size()>32)throw std::runtime_error("Invalid theme adjustments");
        for(const auto& [id,t]:tweaks.items()){ThemeAdjustments a;a.customAccent=t.at("customAccent").get<decltype(a.customAccent)>();a.accent=t.at("accent").get<decltype(a.accent)>();a.frameOpacity=t.at("frameOpacity").get<decltype(a.frameOpacity)>();a.iconScale=t.at("iconScale").get<decltype(a.iconScale)>();o.themeAdjustments[id]=a;}
    }
    o.selectorDock=j.at("selectorDock").get<decltype(o.selectorDock)>();o.selectorGap=j.at("selectorGap").get<decltype(o.selectorGap)>();o.selectorX=j.at("selectorX").get<decltype(o.selectorX)>();o.selectorY=j.at("selectorY").get<decltype(o.selectorY)>();
    o.animateScroll=j.at("animateScroll").get<decltype(o.animateScroll)>();o.fadeIdle=j.at("fadeIdle").get<decltype(o.fadeIdle)>();o.keepCombatVisible=j.at("keepCombatVisible").get<decltype(o.keepCombatVisible)>();
    o.keepDrawnVisible=j.value("keepDrawnVisible",false);
    o.fadeDelay=j.at("fadeDelay").get<decltype(o.fadeDelay)>();o.idleOpacity=j.at("idleOpacity").get<decltype(o.idleOpacity)>();
    sanitizeOptions(o);return o;
}
inline void revealSavedGroups(Options& o,const Arsenal& a) {
    sanitizeOptions(o);normalizeGroupModes(o,a);
    for(std::size_t id=0;id<Arsenal::count;++id)if((!a.groups[id].entries.empty()||a.groups[id].automatic()||id==a.active)&&!groupVisible(o,id)) {
        const auto it=std::find(o.groupOrder.begin()+std::ptrdiff_t(o.groupCount),o.groupOrder.end(),id);
        std::rotate(o.groupOrder.begin()+std::ptrdiff_t(o.groupCount),it,it+1);++o.groupCount;
    }
}
}
