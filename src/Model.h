#pragma once
#include "Icons.h"
#include "Localization.h"
#include "ControllerTypes.h"
#include "ActionBindings.h"
#include <bit>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <map>
#include <string>
#include <vector>

namespace quickslot {
enum class Hand : unsigned { right, left };

enum class Kind : unsigned { sword, greatsword, dagger, axe, mace, bow, staff, shield, torch, spell,
    potion, food, poison, scroll, power, lesserPower, shout, smartPotion, combo, ammo, emptyHand, unarmed, smartWeapon, smartAmmo };

enum class WeaponFamily : unsigned {oneHanded,twoHanded,sword,dagger,axe,mace,greatsword,battleaxe,warhammer,bow,crossbow,shield,count};
struct PotionExclusion {
    std::uint32_t form=0;
    std::uint64_t recipe=0;
    bool operator==(const PotionExclusion&)const=default;
};
struct Entry {
    std::uint32_t form=0,owner=0;
    std::uint16_t unique=0;
    Hand hand=Hand::right;
    Kind kind=Kind::sword;
    bool twoHanded=false,spell=false,unresolved=false;
    std::string name;
    Icon icon=Icon::automatic;
    std::uint64_t recipe=0;
    unsigned equipmentType=0;
    unsigned resource=0,effect=0,policy=0;
    std::vector<PotionExclusion> exclusions;
    std::vector<Entry> parts;
    std::uint32_t shortcut=0;
    bool same(const Entry& b) const {
        if(kind==Kind::emptyHand||kind==Kind::unarmed||b.kind==Kind::emptyHand||b.kind==Kind::unarmed)
            return kind==b.kind&&hand==b.hand;
        if(kind==Kind::smartWeapon||kind==Kind::smartAmmo||b.kind==Kind::smartWeapon||b.kind==Kind::smartAmmo)
            return kind==b.kind&&equipmentType==b.equipmentType&&hand==b.hand;
        if(kind==Kind::smartPotion||b.kind==Kind::smartPotion)
            return kind==b.kind&&resource==b.resource&&effect==b.effect&&policy==b.policy&&exclusions==b.exclusions;
        if(kind==Kind::combo||b.kind==Kind::combo)
            return kind==b.kind&&parts.size()==2&&b.parts.size()==2&&parts[0].same(b.parts[0])&&parts[1].same(b.parts[1]);
        return !unresolved&&!b.unresolved&&form==b.form&&owner==b.owner&&unique==b.unique&&hand==b.hand&&spell==b.spell&&kind==b.kind&&recipe==b.recipe;
    }
};
inline bool isSmartEquipment(const Entry& e){return e.kind==Kind::smartWeapon||e.kind==Kind::smartAmmo;}
inline bool smartTwoHanded(unsigned type){return type==1||(type>=6&&type<=unsigned(WeaponFamily::crossbow));}
inline bool smartShield(const Entry& e){return e.kind==Kind::smartWeapon&&e.equipmentType==unsigned(WeaponFamily::shield);}
inline bool validSmartEquipment(const Entry& e) {
    return isSmartEquipment(e)&&unsigned(e.hand)<=1&&!e.form&&!e.owner&&!e.unique&&!e.spell&&!e.unresolved&&!e.recipe&&
        !e.resource&&!e.effect&&!e.policy&&e.parts.empty()&&e.exclusions.empty()&&
        e.equipmentType<(e.kind==Kind::smartAmmo?2u:unsigned(WeaponFamily::count))&&
        e.twoHanded==(e.kind==Kind::smartWeapon&&smartTwoHanded(e.equipmentType))&&
        (!(e.kind==Kind::smartAmmo||e.twoHanded)||e.hand==Hand::right)&&(!smartShield(e)||e.hand==Hand::left);
}
inline bool isHandAction(const Entry& e){return e.kind==Kind::emptyHand||e.kind==Kind::unarmed;}
inline bool isReusable(const Entry& e){return e.kind==Kind::combo||e.kind==Kind::smartPotion||isSmartEquipment(e)||isHandAction(e);}
inline std::string handActionName(const Entry& e){return e.kind==Kind::unarmed?"Unarmed":e.hand==Hand::left?"Empty left hand":"Empty right hand";}
inline Entry emptyHandEntry(Hand hand){Entry e;e.kind=Kind::emptyHand;e.hand=hand;e.name=handActionName(e);return e;}
inline Entry unarmedEntry(){Entry e;e.kind=Kind::unarmed;e.name=handActionName(e);return e;}
inline bool validHandAction(const Entry& e) {
    return isHandAction(e)&&unsigned(e.hand)<=1&&(e.kind!=Kind::unarmed||e.hand==Hand::right)&&
        !e.form&&!e.owner&&!e.unique&&!e.spell&&!e.twoHanded&&!e.unresolved&&!e.recipe&&
        !e.resource&&!e.effect&&!e.policy&&e.exclusions.empty()&&e.parts.empty();
}
inline bool equipmentSequence(const Entry& e){return e.kind==Kind::combo||e.kind==Kind::unarmed;}
inline Entry equipmentStep(const Entry& e,std::size_t index){return e.kind==Kind::unarmed?emptyHandEntry(Hand(index)):e.parts.at(index);}
inline bool isConsumable(const Entry& e){return e.kind==Kind::potion||e.kind==Kind::food||e.kind==Kind::poison||e.kind==Kind::smartPotion;}
inline bool isStack(const Entry& e){return isConsumable(e)||e.kind==Kind::torch||e.kind==Kind::scroll||e.kind==Kind::ammo||e.kind==Kind::smartAmmo;}
inline bool isVoice(const Entry& e){return e.kind==Kind::power||e.kind==Kind::lesserPower||e.kind==Kind::shout;}
inline bool isInventory(const Entry& e){return !isSmartEquipment(e)&&!e.spell&&!isVoice(e)&&!isHandAction(e)&&e.kind!=Kind::smartPotion&&e.kind!=Kind::combo;}
inline bool tracksInstance(const Entry& e){return isInventory(e)&&!isStack(e);}
template<class E,class F> void eachLeaf(E& e,const F& f){if(e.kind==Kind::combo){for(auto& part:e.parts)f(part);}else f(e);}
inline bool pairable(const Entry& e){return e.kind==Kind::smartWeapon?validSmartEquipment(e)&&!e.twoHanded:e.kind==Kind::emptyHand?validHandAction(e):!isHandAction(e)&&!isSmartEquipment(e)&&!e.unresolved&&e.form&&!isConsumable(e)&&!isVoice(e)&&e.kind!=Kind::combo&&e.kind!=Kind::ammo&&!e.twoHanded;}
inline bool validPair(const Entry& right,const Entry& left) {
    if(!pairable(right)||!pairable(left)||right.hand!=Hand::right||left.hand!=Hand::left||right.kind==Kind::shield||right.kind==Kind::torch)return false;
    return !(tracksInstance(right)&&tracksInstance(left)&&right.form==left.form&&right.owner==left.owner&&right.unique==left.unique);
}
inline std::size_t wrappedIndex(std::size_t index,int delta,std::size_t count) {
    return count?std::size_t((int(index%count)+delta%int(count)+int(count))%int(count)):0;
}
struct SelectorArrow {int delta=0;bool reorder=false;};
inline SelectorArrow selectorArrow(unsigned scan,bool ctrl) {
    return {scan==203?-1:scan==205?1:0,ctrl};
}
enum class FavoriteSource : unsigned {manual,all,weapons,spells,potions,poisons,food,ammo,scrolls,voice,offhand,count};
inline bool sameFavorite(const Entry& a,const Entry& b){auto other=b;other.hand=a.hand;return a.same(other);}
struct Group {
    static constexpr std::size_t limit=16;
    static constexpr std::size_t exclusionLimit=128;
    std::vector<Entry> entries;
    std::size_t selected=0;
    std::uint64_t revision=0;
    FavoriteSource source=FavoriteSource::manual;
    Hand favoriteHand=Hand::right;
    std::vector<Entry> manualEntries,excludedFavorites;
    std::size_t manualSelected=0;
    std::optional<Entry> rememberedFavorite;
    bool automatic()const{return source!=FavoriteSource::manual;}
    auto& savedEntries(){return automatic()?manualEntries:entries;}
    const auto& savedEntries()const{return automatic()?manualEntries:entries;}
    bool sortByName();
    bool add(Entry value,bool autoSort=false) {
        if(automatic())return false;
        const auto it=std::find_if(entries.begin(),entries.end(),[&](const Entry& e){return e.shortcut&&value.shortcut?e.shortcut==value.shortcut:e.same(value);});
        if(it!=entries.end()) { selected=std::size_t(it-entries.begin());++revision;return false; }
        if(entries.size()>=limit)return false;
        entries.push_back(std::move(value));selected=entries.size()-1;++revision;
        if(autoSort)sortByName();
        return true;
    }
    bool remove() {
        if(entries.empty())return false;
        if(automatic()) {
            if(excludedFavorites.size()>=exclusionLimit)return false;
            if(std::none_of(excludedFavorites.begin(),excludedFavorites.end(),[&](const auto& e){return sameFavorite(e,entries[selected]);}))excludedFavorites.push_back(entries[selected]);
        }
        entries.erase(entries.begin()+std::ptrdiff_t(selected));selected=std::min(selected,entries.empty()?0:entries.size()-1);++revision;return true;
    }
    bool reorder(std::size_t from,std::size_t to) {
        if(automatic())return false;
        if(from>=entries.size()||to>=entries.size()||from==to)return false;
        auto entry=std::move(entries[from]);
        entries.erase(entries.begin()+std::ptrdiff_t(from));
        entries.insert(entries.begin()+std::ptrdiff_t(to),std::move(entry));
        if(selected==from)selected=to;
        else if(from<selected&&selected<=to)--selected;
        else if(to<=selected&&selected<from)++selected;
        ++revision;return true;
    }
};
struct Arsenal {
    std::string presetIdentity;
    static constexpr std::size_t count=8;
    std::array<Group,count> groups;
    static constexpr std::size_t shortcutLimit=256;
    std::vector<Entry> shortcuts;
    std::size_t active=0;
    std::size_t consumableGroup=count;
    Group& current(){return groups[active];}
    const Group& current()const{return groups[active];}
    std::size_t size()const{std::size_t total=0;for(const auto& group:groups)total+=group.entries.size();return total;}
    bool move(std::size_t source,std::size_t index,std::size_t target,bool autoSort=false) {
        if(source>=count||target>=count||source==target||index>=groups[source].entries.size())return false;
        if(groups[source].automatic()||groups[target].automatic())return false;
        auto& from=groups[source];auto& to=groups[target];const auto entry=from.entries[index];
        const bool exists=std::any_of(to.entries.begin(),to.entries.end(),[&](const Entry& e){return e.shortcut&&entry.shortcut?e.shortcut==entry.shortcut:e.same(entry);});
        if(!exists&&to.entries.size()>=Group::limit)return false;
        to.add(entry,autoSort);
        const auto remembered=from.selected;from.selected=index;from.remove();
        if(remembered!=index)from.selected=remembered>index?remembered-1:remembered;
        active=target;return true;
    }
};
template<class G,class F> void eachGroupEntry(G& group,const F& f) {
    for(auto& entry:group.entries)f(entry);
    for(auto& entry:group.manualEntries)f(entry);
    for(auto& entry:group.excludedFavorites)f(entry);
    if(group.rememberedFavorite)f(*group.rememberedFavorite);
}
template<class A,class F> void eachSavedEntry(A& arsenal,const F& f) {
    for(auto& group:arsenal.groups)eachGroupEntry(group,f);
    for(auto& entry:arsenal.shortcuts)f(entry);
}

class Selection {
    bool held_=false,browsing_=false;
    double started_=0;
    std::size_t index_=0,group_=0;
    std::uint64_t token_=0,revision_=0;
public:
    double holdSeconds=.18;
    struct Commit { std::size_t index;std::uint64_t token,revision;std::size_t group=0;bool browsed=false,controller=false,cycling=false;std::optional<Hand> hand; };
    bool held() const { return held_; }
    bool browsing() const { return browsing_; }
    bool canBrowse() const { return held_; }
    std::size_t index() const { return index_; }
    std::size_t group() const { return group_; }
    std::uint64_t token() const { return token_; }
    void press(double now,const Group& g,std::size_t group=0) {
        if(held_&&group_==group)return;
        ++token_;held_=true;browsing_=false;started_=now;index_=g.selected;revision_=g.revision;group_=group;
    }
    void tick(double now) { if(held_&&now-started_>=holdSeconds)browsing_=true; }
    void cycle(int delta,const Group& g) {
        if(!canBrowse())return;
        if(g.revision!=revision_){cancel();return;}
        browsing_=true;
        if(!g.entries.empty())index_=wrappedIndex(index_,delta,g.entries.size());
    }
    void cycleAvailable(int delta,const Group& g,const std::vector<bool>& available) {
        if(available.empty()||available.size()!=g.entries.size()){cycle(delta,g);return;}
        const auto start=index_;
        for(std::size_t i=0;i<g.entries.size();++i){cycle(delta<0?-1:1,g);if(!held_||available[index_])return;}
        index_=start;
    }
    std::optional<Commit> release(const Group& g,std::size_t group=0) {
        if(!held_||group!=group_)return {};
        const bool valid=g.revision==revision_&&index_<g.entries.size(),browsed=browsing_;held_=false;browsing_=false;
        if(!valid){++token_;return {};}
        return Commit{index_,token_,revision_,group_,browsed};
    }
    std::optional<Commit> request(const Group& g,std::size_t group,std::size_t index) {
        cancel();group_=group;index_=index;revision_=g.revision;
        if(index>=g.entries.size())return {};
        return Commit{index,token_,revision_,group};
    }
    std::optional<Commit> requestCycle(const Group& g,std::size_t group,std::size_t index) {
        auto commit=request(g,group,index);if(commit)commit->cycling=true;return commit;
    }
    void rebase(const Group& g,std::size_t index){index_=index;revision_=g.revision;browsing_=held_;}
    bool accepts(const Commit& c,const Group& g,std::size_t group=0) const { return c.group==group&&c.token==token_&&c.revision==g.revision&&c.index<g.entries.size(); }
    void cancel() { held_=false;browsing_=false;++token_; }
};
struct WeaponStatus {bool poisoned=false;int ammunition=-1;};
struct Row { Entry entry;bool available=false,equipped=false;std::string detail;int quantity=0;float cooldown=0;bool powerUsed=false;std::string feedback;WeaponStatus weapon;std::optional<Entry> iconEntry; };
inline const Entry& rowIconEntry(const Row& row){return row.iconEntry?*row.iconEntry:row.entry;}
struct GroupTile {std::optional<Entry> entry;std::size_t count=0;bool available=false,equipped=false;int quantity=0;float cooldown=0;bool voice=false,consumable=false;WeaponStatus weapon;std::optional<Entry> iconEntry;};
struct EquippedDisplay {
    std::array<std::optional<Row>,2> hands;
    std::optional<Row> consumable,voice,ammo;
};
struct Snapshot {
    std::vector<Row> rows;
    std::array<GroupTile,Arsenal::count> groups;
    std::size_t activeGroup=0;
    std::size_t selected=0,preview=0;
    bool visible=false,browsing=false,inCombat=false,editorPreview=false,selectorOnly=false,controller=false,vr=false,weaponsDrawn=false;
    std::uint64_t interaction=0;
    std::string notice;
    double noticeUntil=0;
    EquippedDisplay equipment;
    bool holding=false,placementPreview=false,equipmentOnly=false;
    float previewZoom=0;
    double revealUntil=0;
};
struct ThemeAdjustments {
    bool customAccent=false;
    std::array<float,3> accent{.62f,.25f,.21f};
    float frameOpacity=1,iconScale=1;
};
namespace keyModifier {
inline constexpr unsigned shift=1,ctrl=2,alt=4,all=shift|ctrl|alt;
}
inline unsigned modifierForScan(unsigned scan) {
    if(scan==42||scan==54)return keyModifier::shift;
    if(scan==29||scan==157)return keyModifier::ctrl;
    if(scan==56||scan==184)return keyModifier::alt;
    return 0;
}
template<class IsDown> inline unsigned keyboardModifiers(IsDown down) {
    return ((down(42)||down(54))?keyModifier::shift:0u)|
           ((down(29)||down(157))?keyModifier::ctrl:0u)|
           ((down(56)||down(184))?keyModifier::alt:0u);
}
enum class GroupMode : unsigned {browse,cycle};
struct Options {
    std::array<unsigned,Arsenal::count> keys{2,3,4,5,6,7,8,9};
    std::array<std::string,Arsenal::count> names{"Arsenal","Group 2","Group 3","Group 4","Group 5","Group 6","Group 7","Group 8"};
    unsigned editorKey=43,editorModifiers=keyModifier::shift;
    std::size_t groupCount=3;
    std::array<std::size_t,Arsenal::count> groupOrder{0,1,2,3,4,5,6,7};
    std::array<GroupMode,Arsenal::count> groupModes{};
    std::array<bool,Arsenal::count> autoSort{};
    bool skipEmpty=false,menuCapture=true;
    bool groupCaptureRightHotkey=true,groupCaptureLeftHotkey=true,groupMoveHotkey=true;
    bool groupUnequipHotkey=true,groupRemoveHotkey=true;
    bool selectorReorderHotkey=true,selectorUnequipHotkey=true,selectorRemoveHotkey=true;
    std::array<unsigned,unsigned(GroupModifier::count)> groupModifiers=defaultGroupModifiers;
    std::array<KeyBinding,unsigned(KeyboardAction::count)> selectorBindings=defaultSelectorBindings;
    std::array<int,unsigned(MouseAction::count)> mouseActions=defaultMouseActions;
    bool allowUnusedModifiers=false;
    bool voiceHotkey=true;
    bool quickLootKeyboard=false,quickLootController=false;
    float shoutCharge=1.2f;
    bool controllerEnabled=true,controllerBumpers=true;
    unsigned controllerModifiers=allPadButtons;
    unsigned controllerStyle=0,controllerStick=1;
    float controllerDeadzone=.45f;
    std::array<PadBinding,Arsenal::count> controllerGroups{{{PadButton::up},{PadButton::right},{PadButton::down},{PadButton::left},{},{},{},{}}};
    PadBinding controllerEditor{PadButton::start,PadButton::up};
    std::array<PadBinding,unsigned(ControllerAction::count)> controllerActions=defaultControllerActions;
    std::array<PadBinding,Arsenal::count> vrGroups{{{PadButton::rt,PadButton::lb},{PadButton::rs,PadButton::lb},{PadButton::lt,PadButton::rb},{PadButton::ls,PadButton::rb},{},{},{},{}}};
    PadBinding vrEditor{PadButton::a,PadButton::lb};
    unsigned vrStick=1;
    float selectionSpeed=1;

    unsigned layout=0,shape=0,selector=0,selectorShape=0;
    float spacing=14,positionX=1,positionY=1;
    float selectorSpacing=16;
    std::string themeId="minimal";
    std::string language="auto";
    std::map<std::string,ThemeAdjustments> themeAdjustments;
    unsigned selectorDock=0;
    float selectorGap=12,selectorX=.75f,selectorY=.4f;
    bool animateScroll=true,fadeIdle=false,keepCombatVisible=true,keepDrawnVisible=false;
    float fadeDelay=4,idleOpacity=.35f;
    float scale=1,textScale=1,opacity=.92f;
    bool showIdle=true,showHints=false,showLabels=true,keyIcons=true,coloredMagic=true,showKeys=true,showCardNames=true;
    bool showPoison=true,showAmmo=true;
    unsigned itemIconColor=1;
    bool placementGrid=false,placementSnap=false;
    bool showEquipment=false,equipmentDim=false,hideEmptyGroups=false,showEquipmentAmmo=false;
    float equipmentX=.02f,equipmentY=.8f;
    float equipmentScale=1,equipmentTextScale=1,equipmentSpacing=8;
    unsigned equipmentStyle=0,equipmentLayout=1,equipmentTextPosition=3;
    bool equipmentShowText=true;
    float equipmentFadeDelay=4,equipmentIdleOpacity=.35f;
    unsigned inactiveGroups=0;
    float inactiveGroupOpacity=.3f;
    unsigned placementSpacing=1,placementFrame=0;
    float placementGridSize=16;
};
inline bool editorShortcutMatches(const Options& o,unsigned scan,unsigned modifiers) {
    return o.editorKey&&scan==o.editorKey&&modifiers==o.editorModifiers;
}
inline void setEditingShortcuts(Options& o,bool enabled) {
    o.groupCaptureRightHotkey=o.groupCaptureLeftHotkey=o.groupMoveHotkey=enabled;
    o.groupUnequipHotkey=o.groupRemoveHotkey=enabled;
    o.selectorReorderHotkey=o.selectorUnequipHotkey=o.selectorRemoveHotkey=enabled;
}
inline bool selectorUsesModifier(const Options& o,unsigned modifier) {
    return (o.groupMoveHotkey&&(o.groupModifiers[unsigned(GroupModifier::move)]&modifier))||
        (o.selectorReorderHotkey&&(o.groupModifiers[unsigned(GroupModifier::reorder)]&modifier));
}
inline bool selectorUsesCtrl(const Options& o){return selectorUsesModifier(o,keyModifier::ctrl);}
template<class O> auto& groupActionEnabled(O& o,GroupModifier action) {
    switch(action) {
    case GroupModifier::captureRight:return o.groupCaptureRightHotkey;
    case GroupModifier::captureLeft:return o.groupCaptureLeftHotkey;
    case GroupModifier::move:return o.groupMoveHotkey;
    case GroupModifier::unequip:return o.groupUnequipHotkey;
    case GroupModifier::remove:return o.groupRemoveHotkey;
    default:return o.selectorReorderHotkey;
    }
}
inline bool selectorActionEnabled(const Options& o,KeyboardAction action) {
    return action==KeyboardAction::unequip?o.selectorUnequipHotkey:action==KeyboardAction::remove?o.selectorRemoveHotkey:true;
}
inline void setSelectorActionEnabled(Options& o,KeyboardAction action,bool enabled) {
    if(action==KeyboardAction::unequip)o.selectorUnequipHotkey=enabled;
    if(action==KeyboardAction::remove)o.selectorRemoveHotkey=enabled;
}
inline KeyBinding selectorBinding(const Options& o,KeyboardAction action){return selectorActionEnabled(o,action)?o.selectorBindings[unsigned(action)]:KeyBinding{};}
inline void assignGroupModifier(Options& o,GroupModifier action,unsigned modifiers) {
    if((modifiers&~keyModifier::all)||action>=GroupModifier::count)return;
    auto& enabled=groupActionEnabled(o,action);
    if(!modifiers){enabled=false;return;}
    const auto index=unsigned(action),previous=o.groupModifiers[index];
    if(action!=GroupModifier::move&&action!=GroupModifier::reorder)
        for(const auto other:{GroupModifier::captureRight,GroupModifier::captureLeft,GroupModifier::unequip,GroupModifier::remove})
            if(other!=action&&o.groupModifiers[unsigned(other)]==modifiers) {
                o.groupModifiers[unsigned(other)]=previous;
                if(groupActionEnabled(o,other))groupActionEnabled(o,other)=enabled;
            }
    o.groupModifiers[index]=modifiers;enabled=true;
}
enum class GroupKeyAction {select,captureRight,captureLeft,move,unequip,remove,pass};
inline GroupKeyAction groupKeyAction(const Options& o,unsigned modifiers,bool moving=false) {
    const auto matches=[&](GroupModifier action){const auto mask=o.groupModifiers[unsigned(action)];return mask&&(modifiers&mask)==mask;};
    const auto disabled=o.allowUnusedModifiers?GroupKeyAction::select:GroupKeyAction::pass;
    if(moving&&matches(GroupModifier::move))return o.groupMoveHotkey?GroupKeyAction::move:disabled;
    struct Choice {GroupModifier modifier;GroupKeyAction action;bool enabled;};
    GroupKeyAction result=modifiers&&!o.allowUnusedModifiers?GroupKeyAction::pass:GroupKeyAction::select;int specificity=0;
    for(const auto choice:{Choice{GroupModifier::captureLeft,GroupKeyAction::captureLeft,o.groupCaptureLeftHotkey},
        Choice{GroupModifier::captureRight,GroupKeyAction::captureRight,o.groupCaptureRightHotkey},
        Choice{GroupModifier::remove,GroupKeyAction::remove,o.groupRemoveHotkey},Choice{GroupModifier::unequip,GroupKeyAction::unequip,o.groupUnequipHotkey}}) {
        const auto bits=std::popcount(o.groupModifiers[unsigned(choice.modifier)]);
        if(matches(choice.modifier)&&bits>specificity){specificity=bits;result=choice.enabled?choice.action:disabled;}
    }
    return result;
}
inline std::optional<Hand> menuCaptureHand(const Options& o,unsigned modifiers) {
    if(!o.menuCapture)return {};
    const auto action=groupKeyAction(o,modifiers);
    if(action==GroupKeyAction::captureRight)return Hand::right;
    if(action==GroupKeyAction::captureLeft)return Hand::left;
    return {};
}
enum class SelectorKeyAction {pass,browse,reorder,unequip,remove,rightHand,leftHand};
struct SelectorKey {SelectorKeyAction action=SelectorKeyAction::pass;int delta=0;};
inline SelectorKey selectorKeyAction(const Options& o,unsigned scan,unsigned modifiers) {
    for(const auto action:{KeyboardAction::rightHand,KeyboardAction::leftHand})
        if(keyBindingMatches(o.selectorBindings[unsigned(action)],scan,modifiers))return {action==KeyboardAction::rightHand?SelectorKeyAction::rightHand:SelectorKeyAction::leftHand};
    if(keyBindingMatches(o.selectorBindings[unsigned(KeyboardAction::remove)],scan,modifiers)&&o.selectorRemoveHotkey)return {SelectorKeyAction::remove};
    if(keyBindingMatches(o.selectorBindings[unsigned(KeyboardAction::unequip)],scan,modifiers)&&o.selectorUnequipHotkey)return {SelectorKeyAction::unequip};
    const auto reorder=o.groupModifiers[unsigned(GroupModifier::reorder)];
    const bool reordering=reorder&&(modifiers&reorder)==reorder;
    for(const auto action:{KeyboardAction::previous,KeyboardAction::next}) {
        const auto binding=o.selectorBindings[unsigned(action)];
        if(!keyBindingMatches(binding,scan,reordering?modifiers&~reorder:modifiers))continue;
        if(reordering&&!o.selectorReorderHotkey)return {};
        return {reordering?SelectorKeyAction::reorder:SelectorKeyAction::browse,action==KeyboardAction::previous?-1:1};
    }
    return {};
}
inline SelectorKey selectorKeyAction(const Options& o,unsigned scan,bool ctrl){return selectorKeyAction(o,scan,ctrl?keyModifier::ctrl:0u);}
inline bool selectorKeyReserved(const Options& o,unsigned key){for(unsigned i=0;key&&i<o.selectorBindings.size();++i)if(selectorBinding(o,KeyboardAction(i)).key==key)return true;return false;}
inline bool assignSelectorBinding(Options& o,KeyboardAction action,KeyBinding binding) {
    if(action>=KeyboardAction::count||binding.key>255||modifierForScan(binding.key)||(binding.modifiers&~keyModifier::all))return false;
    if(binding.key&&(binding.key==o.editorKey||std::find(o.keys.begin(),o.keys.end(),binding.key)!=o.keys.end()))return false;
    if(!binding.key)binding.modifiers=0;
    const auto index=unsigned(action);
    for(unsigned i=0;i<o.selectorBindings.size();++i)if(!selectorActionEnabled(o,KeyboardAction(i)))o.selectorBindings[i]={};
    if(binding.key)for(unsigned i=0;i<o.selectorBindings.size();++i)if(i!=index&&o.selectorBindings[i].key==binding.key) {
        o.selectorBindings[i]=o.selectorBindings[index];setSelectorActionEnabled(o,KeyboardAction(i),o.selectorBindings[i].key!=0);
    }
    o.selectorBindings[index]=binding;setSelectorActionEnabled(o,action,binding.key!=0);return true;
}
inline bool hasConsumables(const Group& group){return std::any_of(group.entries.begin(),group.entries.end(),isConsumable);}
inline bool assignMouseAction(Options& o,MouseAction action,int button) {
    if(action>=MouseAction::count||button< -1||button>4)return false;
    const auto index=unsigned(action);
    if(button>=0)for(unsigned i=0;i<o.mouseActions.size();++i)if(i!=index&&o.mouseActions[i]==button)o.mouseActions[i]=o.mouseActions[index];
    o.mouseActions[index]=button;return true;
}
inline bool useBumpersForHands(Options& o) {
    const auto conflict=[](PadBinding b){return b.button==PadButton::lb||b.button==PadButton::rb||b.modifier==PadButton::lb||b.modifier==PadButton::rb;};
    if(conflict(o.controllerEditor))return false;
    for(std::size_t i=0;i<o.groupCount;++i)if(conflict(o.controllerGroups[o.groupOrder[i]]))return false;
    for(const auto b:o.controllerActions)if(b.modifier==PadButton::lb||b.modifier==PadButton::rb)return false;
    for(auto& b:o.controllerActions)if(b==PadBinding{PadButton::lb}||b==PadBinding{PadButton::rb})b={};
    o.controllerActions[unsigned(ControllerAction::rightHand)]={PadButton::rb};
    o.controllerActions[unsigned(ControllerAction::leftHand)]={PadButton::lb};return true;
}
inline GroupMode groupMode(const Options& o,const Arsenal& a,std::size_t group) {
    return group<Arsenal::count&&!hasConsumables(a.groups[group])?o.groupModes[group]:GroupMode::browse;
}
inline void normalizeGroupModes(Options& o,const Arsenal& a) {
    for(std::size_t g=0;g<Arsenal::count;++g)if(hasConsumables(a.groups[g]))o.groupModes[g]=GroupMode::browse;
}
inline std::size_t nextEntry(const Group& group,const std::vector<bool>& available,bool skipEmpty) {
    if(group.entries.empty())return 0;
    auto index=group.selected;
    for(std::size_t i=0;i<group.entries.size();++i) {
        index=(index+1)%group.entries.size();
        if(!skipEmpty||available.size()!=group.entries.size()||available[index])return index;
    }
    return group.selected;
}

class GroupRecall {
    std::optional<std::size_t> last_;
public:
    bool activate(std::size_t group){const bool recall=last_!=group;last_=group;return recall;}
    void reset(){last_.reset();}
};
inline std::size_t cycleEntry(const Group& group,const std::vector<bool>& available,bool skipEmpty,bool recall) {
    if(recall&&(!skipEmpty||available.size()!=group.entries.size()||(group.selected<available.size()&&available[group.selected])))return group.selected;
    return nextEntry(group,available,skipEmpty);
}
enum class SelectionAction {none,select,equip,use};
inline SelectionAction selectionAction(const Entry& e,const Selection::Commit& c,const Options& o) {
    if(c.group>=Arsenal::count)return SelectionAction::none;
    if(isConsumable(e))return c.browsed||c.cycling?SelectionAction::select:SelectionAction::use;
    if(isVoice(e))return o.voiceHotkey&&!c.browsed&&!c.cycling?SelectionAction::use:SelectionAction::equip;
    return SelectionAction::equip;
}
inline bool selectsConsumable(const Entry& e,const Selection::Commit& c,const Options& o){return isConsumable(e)&&selectionAction(e,c,o)==SelectionAction::select;}
inline bool groupVisible(const Options& o,std::size_t id){return std::find(o.groupOrder.begin(),o.groupOrder.begin()+std::min(o.groupCount,Arsenal::count),id)!=o.groupOrder.begin()+std::min(o.groupCount,Arsenal::count);}
inline void sortNewlyEnabledGroups(Arsenal& a,const Options& previous,const Options& next) {
    for(std::size_t id=0;id<Arsenal::count;++id)if(next.autoSort[id]&&!previous.autoSort[id])a.groups[id].sortByName();
}
inline void resetGroup(Arsenal& a,Options& o,std::size_t id) {
    auto& g=a.groups[id];const auto revision=g.revision+1;g={};g.revision=revision;
    o.names[id]="Group "+std::to_string(id+1);
    o.keys[id]=0;o.controllerGroups[id]={};o.vrGroups[id]={};
    o.groupModes[id]=GroupMode::browse;o.autoSort[id]=false;
    if(a.consumableGroup==id)a.consumableGroup=Arsenal::count;
}
template<class Binding> Binding nextGroupBinding(const Options& o,std::array<Binding,Arsenal::count>& bindings,
                                                const std::array<Binding,Arsenal::count>& defaults,Binding editor) {
    for(const auto candidate:defaults) {
        if(candidate==Binding{}||candidate==editor)continue;
        if(std::any_of(o.groupOrder.begin(),o.groupOrder.begin()+std::ptrdiff_t(o.groupCount),[&](auto id){return bindings[id]==candidate;}))continue;
        // Hidden slots do not reserve defaults; release any stale duplicate there.
        for(auto& binding:bindings)if(binding==candidate)binding={};
        return candidate;
    }
    return {};
}
inline bool removeGroup(Arsenal& a,Options& o,std::size_t id) {
    if(o.groupCount<=1||!groupVisible(o,id))return false;
    const auto it=std::find(o.groupOrder.begin(),o.groupOrder.end(),id);
    std::rotate(it,it+1,o.groupOrder.end());--o.groupCount;
    resetGroup(a,o,id);
    if(a.active==id)a.active=o.groupOrder[0];
    return true;
}
inline bool addGroup(Arsenal& a,Options& o) {
    if(o.groupCount>=Arsenal::count)return false;
    const auto id=o.groupOrder[o.groupCount];resetGroup(a,o,id);
    const Options defaults;
    o.keys[id]=nextGroupBinding(o,o.keys,defaults.keys,o.editorKey);
    o.controllerGroups[id]=nextGroupBinding(o,o.controllerGroups,defaults.controllerGroups,o.controllerEditor);
    o.vrGroups[id]=nextGroupBinding(o,o.vrGroups,defaults.vrGroups,o.vrEditor);
    o.names[id]="Group "+std::to_string(o.keys[id]?o.keys[id]-1:o.groupCount+1);
    ++o.groupCount;a.active=id;return true;
}
inline bool reorderGroup(Options& o,std::size_t id,int delta) {
    if(!groupVisible(o,id))return false;
    const auto from=std::find(o.groupOrder.begin(),o.groupOrder.end(),id)-o.groupOrder.begin(),to=from+delta;
    if(to<0||to>=std::ptrdiff_t(o.groupCount))return false;
    std::swap(o.groupOrder[std::size_t(from)],o.groupOrder[std::size_t(to)]);return true;
}
inline std::string handName(const Entry& e) {
    if(e.kind==Kind::unarmed)return tr("Both hands");
    if(e.kind==Kind::ammo||e.kind==Kind::smartAmmo)return tr("Ammo");
    if(e.kind==Kind::combo)return tr("Equipment combination");
    if(isVoice(e))return tr("Voice");
    if(e.kind==Kind::potion||e.kind==Kind::smartPotion)return tr("Drink");
    if(e.kind==Kind::food)return tr("Eat");
    if(e.kind==Kind::poison)return tr("Poison weapon");
    return e.twoHanded?tr("Both hands"):e.hand==Hand::left?tr("Left hand"):tr("Right hand");
}
inline std::string rowStatus(const Row& row) {
    if(!row.detail.empty())return row.detail;
    if(!row.available)return isStack(row.entry)&&!row.entry.unresolved?tr("Empty"):tr("Unavailable");
    auto text=isConsumable(row.entry)||row.entry.kind==Kind::ammo?std::string{}:handName(row.entry);
    if(isHandAction(row.entry))return text+(row.equipped?tr(" · Empty"):tr(" · Clear"));
    if(isStack(row.entry))text+=(text.empty()?"":" · ")+std::string("×")+std::to_string(row.quantity);
    if(row.equipped)text+=isVoice(row.entry)?tr(" · Selected"):tr(" · Equipped");
    if(row.cooldown>0)text+=" · "+std::to_string(int(std::ceil(row.cooldown)))+tr("s cooldown");
    if(row.powerUsed)text+=tr(" · Recharging");
    return text;
}
inline std::string displayName(const Entry& e){return isHandAction(e)||isSmartEquipment(e)||e.kind==Kind::smartPotion?std::string(tr(e.name.c_str())):e.name;}
inline bool Group::sortByName() {
    if(automatic())return false;
    std::vector<std::size_t> order;std::vector<std::string> names;
    for(std::size_t i=0;i<entries.size();++i){order.push_back(i);names.push_back(foldedText(displayName(entries[i])));}
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return names[a]<names[b];});
    bool changed=false;for(std::size_t i=0;i<order.size();++i)changed|=order[i]!=i;
    if(!changed)return false;
    std::vector<Entry> sorted;sorted.reserve(entries.size());
    const auto remembered=selected;
    for(std::size_t i=0;i<order.size();++i){if(order[i]==remembered)selected=i;sorted.push_back(std::move(entries[order[i]]));}
    entries=std::move(sorted);++revision;return true;
}
inline std::string entryLabel(const Entry& e) { return handName(e)+": "+e.name; }
inline bool validGroupKey(unsigned key) {
    return key>=2&&key<=255&&key!=14&&key!=29&&key!=42&&key!=54&&key!=56&&key!=157&&key!=184&&key!=203&&key!=205&&key!=211;
}
inline bool assignGroupKey(Options& o,std::size_t group,unsigned key) {
    if(group>=Arsenal::count||(key&&(!validGroupKey(key)||key==o.editorKey||selectorKeyReserved(o,key))))return false;
    if(key)for(std::size_t id=0;id<Arsenal::count;++id)if(id!=group&&o.keys[id]==key)
        o.keys[id]=groupVisible(o,id)?o.keys[group]:0;
    o.keys[group]=key;return true;
}
inline bool flexibleHand(const Entry& e){return !isConsumable(e)&&!isVoice(e)&&e.kind!=Kind::combo&&e.kind!=Kind::ammo&&e.kind!=Kind::smartAmmo&&e.kind!=Kind::unarmed&&!e.twoHanded&&e.kind!=Kind::shield&&e.kind!=Kind::torch&&!smartShield(e);}
inline Hand normalizedHand(const Entry& e,Hand hand){return isConsumable(e)||isVoice(e)||e.kind==Kind::ammo||e.kind==Kind::smartAmmo||e.kind==Kind::unarmed||e.twoHanded?Hand::right:e.kind==Kind::shield||e.kind==Kind::torch||smartShield(e)?Hand::left:hand;}
inline std::optional<Entry> entryForHand(const Entry& entry,Hand hand) {
    if(unsigned(hand)>1||isConsumable(entry)||isVoice(entry)||isHandAction(entry)||entry.kind==Kind::combo||entry.kind==Kind::ammo||entry.kind==Kind::smartAmmo)return {};
    if(hand==Hand::right&&(entry.kind==Kind::shield||entry.kind==Kind::torch||smartShield(entry)))return {};
    auto action=entry;action.hand=normalizedHand(entry,hand);return action;
}
inline std::optional<Selection::Commit> releaseToHand(Selection& selection,const Group& group,std::size_t groupID,Hand hand,bool controller) {
    if(!selection.held())return {};
    if(selection.index()>=group.entries.size()||!entryForHand(group.entries[selection.index()],hand)){selection.cancel();return {};}
    auto commit=selection.release(group,groupID);
    if(commit){commit->hand=hand;commit->browsed=true;commit->controller=controller;}
    return commit;
}
}
