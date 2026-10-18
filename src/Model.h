#pragma once
#include "Icons.h"
#include "ControllerTypes.h"
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
    potion, food, poison, scroll, power, lesserPower, shout, smartPotion, combo, ammo, emptyHand, unarmed };
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
    unsigned resource=0,effect=0,policy=0;
    std::vector<PotionExclusion> exclusions;
    std::vector<Entry> parts;
    bool same(const Entry& b) const {
        if(kind==Kind::emptyHand||kind==Kind::unarmed||b.kind==Kind::emptyHand||b.kind==Kind::unarmed)
            return kind==b.kind&&hand==b.hand;
        if(kind==Kind::smartPotion||b.kind==Kind::smartPotion)
            return kind==b.kind&&resource==b.resource&&effect==b.effect&&policy==b.policy&&exclusions==b.exclusions;
        if(kind==Kind::combo||b.kind==Kind::combo)
            return kind==b.kind&&parts.size()==2&&b.parts.size()==2&&parts[0].same(b.parts[0])&&parts[1].same(b.parts[1]);
        return !unresolved&&!b.unresolved&&form==b.form&&owner==b.owner&&unique==b.unique&&hand==b.hand&&spell==b.spell&&kind==b.kind&&recipe==b.recipe;
    }
};
inline bool isHandAction(const Entry& e){return e.kind==Kind::emptyHand||e.kind==Kind::unarmed;}
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
inline bool isStack(const Entry& e){return isConsumable(e)||e.kind==Kind::scroll||e.kind==Kind::ammo;}
inline bool isVoice(const Entry& e){return e.kind==Kind::power||e.kind==Kind::lesserPower||e.kind==Kind::shout;}
inline bool isInventory(const Entry& e){return !e.spell&&!isVoice(e)&&!isHandAction(e)&&e.kind!=Kind::smartPotion&&e.kind!=Kind::combo;}
inline bool tracksInstance(const Entry& e){return isInventory(e)&&!isStack(e);}
template<class E,class F> void eachLeaf(E& e,const F& f){if(e.kind==Kind::combo){for(auto& part:e.parts)f(part);}else f(e);}
inline bool pairable(const Entry& e){return e.kind==Kind::emptyHand?validHandAction(e):!isHandAction(e)&&!e.unresolved&&e.form&&!isConsumable(e)&&!isVoice(e)&&e.kind!=Kind::combo&&e.kind!=Kind::ammo&&!e.twoHanded;}
inline bool validPair(const Entry& right,const Entry& left) {
    if(!pairable(right)||!pairable(left)||right.hand!=Hand::right||left.hand!=Hand::left||right.kind==Kind::shield||right.kind==Kind::torch)return false;
    return !(tracksInstance(right)&&tracksInstance(left)&&right.form==left.form&&right.owner==left.owner&&right.unique==left.unique);
}
struct Group {
    static constexpr std::size_t limit=16;
    std::vector<Entry> entries;
    std::size_t selected=0;
    std::uint64_t revision=0;
    bool add(Entry value) {
        const auto it=std::find_if(entries.begin(),entries.end(),[&](const Entry& e){return e.same(value);});
        if(it!=entries.end()) { selected=std::size_t(it-entries.begin());++revision;return false; }
        if(entries.size()>=limit)return false;
        entries.push_back(std::move(value));selected=entries.size()-1;++revision;return true;
    }
    bool remove() {
        if(entries.empty())return false;
        entries.erase(entries.begin()+std::ptrdiff_t(selected));selected=std::min(selected,entries.empty()?0:entries.size()-1);++revision;return true;
    }
    bool reorder(std::size_t from,std::size_t to) {
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
    std::size_t active=0;
    Group& current(){return groups[active];}
    const Group& current()const{return groups[active];}
    std::size_t size()const{std::size_t total=0;for(const auto& group:groups)total+=group.entries.size();return total;}
    bool move(std::size_t source,std::size_t index,std::size_t target) {
        if(source>=count||target>=count||source==target||index>=groups[source].entries.size())return false;
        auto& from=groups[source];auto& to=groups[target];const auto entry=from.entries[index];
        const bool exists=std::any_of(to.entries.begin(),to.entries.end(),[&](const Entry& e){return e.same(entry);});
        if(!exists&&to.entries.size()>=Group::limit)return false;
        to.add(entry);
        const auto remembered=from.selected;from.selected=index;from.remove();
        if(remembered!=index)from.selected=remembered>index?remembered-1:remembered;
        active=target;return true;
    }
};
class Selection {
    bool held_=false,browsing_=false;
    double started_=0;
    std::size_t index_=0,group_=0;
    std::uint64_t token_=0,revision_=0;
public:
    double holdSeconds=.18;
    struct Commit { std::size_t index;std::uint64_t token,revision;std::size_t group=0;bool browsed=false; };
    bool held() const { return held_; }
    bool browsing() const { return browsing_; }
    std::size_t index() const { return index_; }
    std::size_t group() const { return group_; }
    std::uint64_t token() const { return token_; }
    void press(double now,const Group& g,std::size_t group=0) {
        if(held_&&group_==group)return;
        ++token_;held_=true;browsing_=false;started_=now;index_=g.selected;revision_=g.revision;group_=group;
    }
    void tick(double now) { if(held_&&now-started_>=holdSeconds)browsing_=true; }
    void cycle(int delta,const Group& g) {
        if(!held_)return;
        if(g.revision!=revision_){cancel();return;}
        browsing_=true;
        if(!g.entries.empty())index_=std::size_t((int(index_)+delta%int(g.entries.size())+int(g.entries.size()))%int(g.entries.size()));
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
    void rebase(const Group& g,std::size_t index){index_=index;revision_=g.revision;browsing_=held_;}
    bool accepts(const Commit& c,const Group& g,std::size_t group=0) const { return c.group==group&&c.token==token_&&c.revision==g.revision&&c.index<g.entries.size(); }
    void cancel() { held_=false;browsing_=false;++token_; }
};
struct Row { Entry entry;bool available=false,equipped=false;std::string detail;int quantity=0;float cooldown=0;bool powerUsed=false;std::string feedback; };
struct GroupTile {std::optional<Entry> entry;std::size_t count=0;bool available=false,equipped=false;int quantity=0;float cooldown=0;};
struct Snapshot {
    std::vector<Row> rows;
    std::array<GroupTile,Arsenal::count> groups;
    std::size_t activeGroup=0;
    std::size_t selected=0,preview=0;
    bool visible=false,browsing=false,inCombat=false,editorPreview=false,selectorOnly=false,controller=false,vr=false;
    std::string notice;
    double noticeUntil=0;
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
struct Options {
    std::array<unsigned,Arsenal::count> keys{2,3,4,5,6,7,8,9};
    std::array<std::string,Arsenal::count> names{"Arsenal","Group 2","Group 3","Group 4","Group 5","Group 6","Group 7","Group 8"};
    unsigned editorKey=43,editorModifiers=keyModifier::shift;
    std::size_t groupCount=3;
    std::array<std::size_t,Arsenal::count> groupOrder{0,1,2,3,4,5,6,7};
    bool consumableOnRelease=false,skipEmpty=false,menuCapture=true;
    bool voiceHotkey=true,voiceOnRelease=false;
    float shoutCharge=1.2f;
    bool controllerEnabled=true,controllerBumpers=true;
    unsigned controllerStyle=0,controllerStick=1;
    float controllerDeadzone=.45f;
    std::array<PadBinding,Arsenal::count> controllerGroups{{{PadButton::up},{PadButton::right},{PadButton::down},{PadButton::left},{},{},{},{}}};
    PadBinding controllerEditor{PadButton::start,PadButton::up};
    std::array<PadBinding,Arsenal::count> vrGroups{{{PadButton::rt,PadButton::lb},{PadButton::rs,PadButton::lb},{PadButton::lt,PadButton::rb},{PadButton::ls,PadButton::rb},{},{},{},{}}};
    PadBinding vrEditor{PadButton::a,PadButton::lb};
    unsigned vrStick=1;
    float selectionSpeed=1;
    unsigned layout=0,shape=0,selector=0,selectorShape=0;
    float spacing=14,positionX=1,positionY=1;
    float selectorSpacing=16;
    std::string themeId="minimal";
    std::map<std::string,ThemeAdjustments> themeAdjustments;
    unsigned selectorDock=0;
    float selectorGap=12,selectorX=.75f,selectorY=.4f;
    bool animateScroll=true,fadeIdle=false,keepCombatVisible=true;
    float fadeDelay=4,idleOpacity=.35f;
    float scale=1,opacity=.92f;
    bool showIdle=true,showHints=false,showLabels=true,keyIcons=true,coloredMagic=true,showKeys=true,showCardNames=true;
    unsigned itemIconColor=1;
    bool placementGrid=false,placementSnap=false;
};
inline bool editorShortcutMatches(const Options& o,unsigned scan,unsigned modifiers) {
    return o.editorKey&&scan==o.editorKey&&modifiers==o.editorModifiers;
}
inline bool selectsConsumable(const Entry& e,const Selection::Commit& c,const Options& o){return isConsumable(e)&&c.browsed&&!o.consumableOnRelease;}
inline bool groupVisible(const Options& o,std::size_t id){return std::find(o.groupOrder.begin(),o.groupOrder.begin()+std::min(o.groupCount,Arsenal::count),id)!=o.groupOrder.begin()+std::min(o.groupCount,Arsenal::count);}
inline bool removeGroup(Arsenal& a,Options& o,std::size_t id) {
    if(o.groupCount<=1||!groupVisible(o,id))return false;
    const auto it=std::find(o.groupOrder.begin(),o.groupOrder.end(),id);
    std::rotate(it,it+1,o.groupOrder.end());--o.groupCount;
    auto& g=a.groups[id];g.entries.clear();g.selected=0;++g.revision;
    if(a.active==id)a.active=o.groupOrder[0];
    return true;
}
inline bool addGroup(Arsenal& a,Options& o) {
    if(o.groupCount>=Arsenal::count)return false;
    a.active=o.groupOrder[o.groupCount++];return true;
}
inline bool reorderGroup(Options& o,std::size_t id,int delta) {
    if(!groupVisible(o,id))return false;
    const auto from=std::find(o.groupOrder.begin(),o.groupOrder.end(),id)-o.groupOrder.begin(),to=from+delta;
    if(to<0||to>=std::ptrdiff_t(o.groupCount))return false;
    std::swap(o.groupOrder[std::size_t(from)],o.groupOrder[std::size_t(to)]);return true;
}
inline std::string handName(const Entry& e) {
    if(e.kind==Kind::unarmed)return "Both hands";
    if(e.kind==Kind::ammo)return "Ammo";
    if(e.kind==Kind::combo)return "Equipment combination";
    if(isVoice(e))return "Voice";
    if(e.kind==Kind::potion||e.kind==Kind::smartPotion)return "Drink";
    if(e.kind==Kind::food)return "Eat";
    if(e.kind==Kind::poison)return "Poison weapon";
    return e.twoHanded?"Both hands":e.hand==Hand::left?"Left hand":"Right hand";
}
inline std::string rowStatus(const Row& row) {
    if(!row.detail.empty())return row.detail;
    if(!row.available)return isStack(row.entry)&&!row.entry.unresolved?"Empty":"Unavailable";
    auto text=isConsumable(row.entry)||row.entry.kind==Kind::ammo?std::string{}:handName(row.entry);
    if(isHandAction(row.entry))return text+(row.equipped?" · Empty":" · Clear");
    if(isStack(row.entry))text+=(text.empty()?"":" · ")+std::string("×")+std::to_string(row.quantity);
    if(row.equipped)text+=isVoice(row.entry)?" · Selected":" · Equipped";
    if(row.cooldown>0)text+=" · "+std::to_string(int(std::ceil(row.cooldown)))+"s cooldown";
    if(row.powerUsed)text+=" · Recharging";
    return text;
}
inline std::string entryLabel(const Entry& e) { return handName(e)+": "+e.name; }
inline bool validGroupKey(unsigned key) {
    return key>=2&&key<=255&&key!=14&&key!=29&&key!=42&&key!=54&&key!=56&&key!=157&&key!=184&&key!=203&&key!=205&&key!=211;
}
inline bool flexibleHand(const Entry& e){return !isConsumable(e)&&!isVoice(e)&&e.kind!=Kind::combo&&e.kind!=Kind::ammo&&e.kind!=Kind::unarmed&&!e.twoHanded&&e.kind!=Kind::shield&&e.kind!=Kind::torch;}
inline Hand normalizedHand(const Entry& e,Hand hand){return isConsumable(e)||isVoice(e)||e.kind==Kind::ammo||e.kind==Kind::unarmed||e.twoHanded?Hand::right:e.kind==Kind::shield||e.kind==Kind::torch?Hand::left:hand;}
}
