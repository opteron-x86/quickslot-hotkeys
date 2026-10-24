#include "Game.h"
#include "InputFilter.h"
#include "GameplayMenus.h"
#include "Storage.h"
#include "CallSite.h"
#include "ItemIdentity.h"
#include "Inventory.h"
#include "MenuCapture.h"
#include "Settings.h"
#include "Presets.h"
#include "MenuFramework.h"
#include "MenuAPI.h"
#include "EquippedState.h"
#include "WeaponIcons.h"
#include "IconPolicy.h"
#include "RuntimeMenu.h"
#include "RuntimePolicy.h"
#include "VRInput.h"
#include "VROverlay.h"
#include <fstream>
#include <iterator>
#include <windows.h>
#undef GetObject
#include <chrono>
#include <bit>
#include <unordered_set>

namespace quickslot {
namespace {
namespace log=SKSE::log;
constexpr char slowMenuName[]="QuickslotSelection";
constexpr char pauseMenuName[]="QuickslotSelectionPause";
constexpr std::uint32_t recordType=0x47525031;
using Dispatch=void(RE::BSTEventSource<RE::InputEvent*>*,RE::InputEvent**);
REL::Relocation<Dispatch> originalDispatch;
void inputHook(RE::BSTEventSource<RE::InputEvent*>* source,RE::InputEvent** events) { Game::get().dispatch(source,events); }
class Menu final : public RuntimeMenu {
public:
    Menu():RuntimeMenu(menuName) {
        depthPriority=11;
        menuFlags.set(RE::UI_MENU_FLAGS::kAlwaysOpen,RE::UI_MENU_FLAGS::kRequiresUpdate,
                      RE::UI_MENU_FLAGS::kCustomRendering,RE::UI_MENU_FLAGS::kAllowSaving);
    }
    void PostDisplay()override{if(!REL::Module::IsVR())Game::get().draw();}
    static RE::IMenu* create(){return new Menu;}
};

class SlowMenu final : public RuntimeMenu {
public:
    SlowMenu():RuntimeMenu(slowMenuName){depthPriority=0;menuFlags.set(static_cast<RE::UI_MENU_FLAGS>(1u<<30),RE::UI_MENU_FLAGS::kAllowSaving);}
    static RE::IMenu* create(){return new SlowMenu;}
};
class PauseMenu final : public RuntimeMenu {
public:

    PauseMenu():RuntimeMenu(pauseMenuName){depthPriority=0;menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame,RE::UI_MENU_FLAGS::kAllowSaving);}
    static RE::IMenu* create(){return new PauseMenu;}
};
class EditorMenu final : public RuntimeMenu {
public:
    EditorMenu():RuntimeMenu(editorMenuName){depthPriority=12;inputContext=RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode;menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame,RE::UI_MENU_FLAGS::kUsesMenuContext,RE::UI_MENU_FLAGS::kModal,RE::UI_MENU_FLAGS::kDisablePauseMenu,RE::UI_MENU_FLAGS::kRequiresUpdate,RE::UI_MENU_FLAGS::kCustomRendering);}
    void PostDisplay()override{if(!REL::Module::IsVR())Game::get().draw(true);}
    static RE::IMenu* create(){return new EditorMenu;}
};
struct MenuBlocker {std::string name;std::uint32_t flags=0;};
bool foreignMenu(RE::UI* ui,MenuPurpose purpose=MenuPurpose::input,MenuBlocker* blocker=nullptr) {
    static_assert(gameplayBlockingMenus==std::array{
        RE::TweenMenu::MENU_NAME,RE::InventoryMenu::MENU_NAME,RE::MagicMenu::MENU_NAME,RE::FavoritesMenu::MENU_NAME,RE::MapMenu::MENU_NAME,RE::StatsMenu::MENU_NAME,RE::JournalMenu::MENU_NAME,
        RE::BookMenu::MENU_NAME,RE::ContainerMenu::MENU_NAME,RE::BarterMenu::MENU_NAME,RE::GiftMenu::MENU_NAME,RE::CraftingMenu::MENU_NAME,RE::SleepWaitMenu::MENU_NAME,
        RE::LockpickingMenu::MENU_NAME,RE::TrainingMenu::MENU_NAME,RE::MessageBoxMenu::MENU_NAME,RE::MainMenu::MENU_NAME,RE::LoadingMenu::MENU_NAME,RE::DialogueMenu::MENU_NAME,
        RE::Console::MENU_NAME,RE::LevelUpMenu::MENU_NAME,RE::RaceSexMenu::MENU_NAME,std::string_view{"LootMenu"}});
    if(gameplayMenuOpen([&](auto name){
        if(!ui->IsMenuOpen(name))return false;
        if(blocker){blocker->name=name;if(const auto menu=ui->GetMenu(name))blocker->flags=menu->menuFlags.underlying();}
        return true;
    },purpose))return true;
    const auto editor=ui->GetMenu(editorMenuName);
    const auto selectionPause=ui->GetMenu(pauseMenuName);
    const auto own=ui->GetMenu(menuName),hud=ui->GetMenu(RE::HUDMenu::MENU_NAME),cursor=ui->GetMenu(RE::CursorMenu::MENU_NAME);
    const auto loot=purpose==MenuPurpose::hud?ui->GetMenu(quickLootMenuName):nullptr;
    for(const auto& menu:ui->menuStack)
        if(menu&&menu.get()!=own.get()&&menu.get()!=editor.get()&&menu.get()!=selectionPause.get()&&menu.get()!=hud.get()&&menu.get()!=cursor.get()&&menu.get()!=loot.get()&&
           menuFlagsBlock(menu->UsesMenuContext(),menu->UsesCursor(),menu->PausesGame(),menu->Modal(),purpose)) {
            if(blocker) {
                blocker->name="unregistered";blocker->flags=menu->menuFlags.underlying();
                for(const auto& [name,entry]:ui->menuMap)if(entry.menu.get()==menu.get()){blocker->name=name.c_str();break;}
            }
            return true;
        }
    return false;
}
bool pressed(RE::BSWin32KeyboardDevice* device,unsigned key) {
    return device&&(device->GetRuntimeData().curState[key]&0x80)!=0;
}
PadCodeSet padCodeSet() {
    const auto* map=RE::ControlMap::GetSingleton();
    return map&&map->GetGamePadType()==RE::PC_GAMEPAD_TYPE::kOrbis?PadCodeSet::orbis:PadCodeSet::xinput;
}

using OrbisKey=RE::BSPCOrbisGamepadDevice::Key;
static_assert(padOrbisCodes==std::array<unsigned,17>{0,OrbisKey::kUp,OrbisKey::kRight,OrbisKey::kDown,OrbisKey::kLeft,
    OrbisKey::kPS3_A,OrbisKey::kPS3_B,OrbisKey::kPS3_X,OrbisKey::kPS3_Y,OrbisKey::kPS3_LB,OrbisKey::kPS3_RB,
    OrbisKey::kPS3_LT,OrbisKey::kPS3_RT,OrbisKey::kPS3_L3,OrbisKey::kPS3_R3,OrbisKey::kPS3_Back,OrbisKey::kPS3_Start});
PadState readPadState(RE::BSPCGamepadDeviceDelegate* device) {
    if(!device)return {};
    if(auto* pad=skyrim_cast<RE::BSWin32GamepadDevice*>(device)) {
        const auto& state=pad->GetRuntimeData().currentState.gamepad;
        return padPhysicalState(PadCodeSet::xinput,state.buttons,state.leftTrigger,state.rightTrigger);
    }
    if(auto* pad=skyrim_cast<RE::BSPCOrbisGamepadDevice*>(device)) {
        const auto& state=pad->GetRuntimeData().currentPadState;

        return padPhysicalState(PadCodeSet::orbis,state.buttonState,std::to_integer<unsigned>(state.rawLeftTrigger),std::to_integer<unsigned>(state.rawRightTrigger));
    }
    return {};
}
PadState readVRState(RE::BSInputDeviceManager* devices) {
    const auto bits=[](RE::BSTrackedControllerDevice* device)->std::uint64_t {
        if(!device||device->GetRuntimeData().trackedDeviceIndex==vr::k_unTrackedDeviceIndexInvalid)return 0;
        return static_cast<RE::BSOpenVRControllerDevice*>(device)->GetRuntimeData().currentState.ulButtonPressed;
    };
    return devices?vrPhysicalState(bits(devices->GetVRControllerRight()),bits(devices->GetVRControllerLeft()),RE::BSOpenVRControllerDevice::IsLeftHandedMode()):PadState{};
}
RE::NiPoint2 readVRAxis(RE::BSInputDeviceManager* devices,bool main) {
    if(!devices)return {};
    const bool right=main!=RE::BSOpenVRControllerDevice::IsLeftHandedMode();
    auto* device=right?devices->GetVRControllerRight():devices->GetVRControllerLeft();
    if(!device||device->GetRuntimeData().trackedDeviceIndex==vr::k_unTrackedDeviceIndexInvalid)return {};
    return static_cast<RE::BSOpenVRControllerDevice*>(device)->GetThumbstick();
}
bool voiceButton(RE::INPUT_DEVICE device,unsigned code,float value,float held) {
    auto* controls=RE::PlayerControls::GetSingleton();auto* events=RE::UserEvents::GetSingleton();
    if(!controls||!controls->shoutHandler||!events||!controls->shoutHandler->IsInputEventHandlingEnabled())return false;
    auto* event=RE::ButtonEvent::Create(device,events->shout,code,value,held);if(!event)return false;
    struct Free {RE::ButtonEvent* p;~Free(){p->SetUserEvent(RE::BSFixedString{});RE::free(p);}} cleanup{event};
    if(!controls->shoutHandler->CanProcess(event))return false;
    controls->shoutHandler->ProcessButton(event,&controls->data);return true;
}
std::string gameName(const char* text){return cleanName(gameTextUTF8(text?text:"",localization().codepage()));}
bool worn(RE::ExtraDataList* extra,const Entry& entry) {
    if(!extra)return false;
    return wornForEntry(entry,extra->HasType<RE::ExtraWorn>(),extra->HasType<RE::ExtraWornLeft>());
}
Icon visualIcon(RE::TESForm* form) {
    if(!form)return Icon::automatic;
    if(auto* ammo=form->As<RE::TESAmmo>())return ammo->IsBolt()?Icon::bolt:Icon::arrow;
    if(form->As<RE::TESShout>())return Icon::shout;
    if(form->As<RE::ScrollItem>())return Icon::scroll;
    if(auto* alchemy=form->As<RE::AlchemyItem>()) {
        if(alchemy->IsPoison())return Icon::poison;
        if(alchemy->IsFood())return Icon::food;
        const auto* effect=alchemy->GetCostliestEffectItem();
        if(effect&&effect->baseEffect)switch(effect->baseEffect->data.primaryAV) {
        case RE::ActorValue::kHealth:case RE::ActorValue::kHealRate:case RE::ActorValue::kHealRateMult:return Icon::health;
        case RE::ActorValue::kMagicka:case RE::ActorValue::kMagickaRate:case RE::ActorValue::kMagickaRateMult:return Icon::magicka;
        case RE::ActorValue::kStamina:case RE::ActorValue::kStaminaRate:case RE::ActorValue::kStaminaRateMult:return Icon::stamina;
        default:break;
        }
        return Icon::potion;
    }
    if(auto* spell=form->As<RE::SpellItem>()) {
        if(spell->GetSpellType()==RE::MagicSystem::SpellType::kPower)return Icon::greater_power;
        if(spell->GetSpellType()==RE::MagicSystem::SpellType::kLesserPower)return Icon::lesser_power;
        const auto* effect=spell->GetCostliestEffectItem();
        if(!effect||!effect->baseEffect)return Icon::spell;
        const auto& data=effect->baseEffect->data;
        switch(data.resistVariable) {
        case RE::ActorValue::kResistFire:return Icon::fire;
        case RE::ActorValue::kResistFrost:return Icon::frost;
        case RE::ActorValue::kResistShock:return Icon::shock;
        default:break;
        }
        switch(data.associatedSkill) {
        case RE::ActorValue::kAlteration:return Icon::alteration;
        case RE::ActorValue::kConjuration:return Icon::conjuration;
        case RE::ActorValue::kDestruction:return Icon::destruction;
        case RE::ActorValue::kIllusion:return Icon::illusion;
        case RE::ActorValue::kRestoration:return Icon::restoration;
        default:return Icon::spell;
        }
    }
    if(auto* weapon=form->As<RE::TESObjectWEAP>()) {
        if(weapon->IsCrossbow())return Icon::crossbow;
        WeaponIconSelection selected;
        const bool twoHanded=weapon->IsTwoHandedSword()||weapon->IsTwoHandedAxe();
        for(const auto* keyword:weapon->GetKeywords())
            if(keyword)if(const auto* id=keyword->GetFormEditorID();id&&*id)selected.consider(id,twoHanded);
        if(selected.icon!=Icon::automatic)return selected.icon;
        if(weapon->IsTwoHandedAxe())return weapon->HasKeywordString("WeapTypeWarhammer")?Icon::warhammer:Icon::battleaxe;
    }
    return Icon::automatic;
}
std::uint64_t recipeSignature(RE::TESForm* form) {
    if(!form->IsDynamicForm())return 0;
    auto* magic=form->As<RE::MagicItem>();if(!magic)return 0;

    std::uint64_t hash=14695981039346656037ull;
    const auto number=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){hash^=(value>>(i*8))&255;hash*=1099511628211ull;}};
    const auto string=[&](std::string_view value){for(const auto ch:value){hash^=static_cast<unsigned char>(ch);hash*=1099511628211ull;}number(0);};
    const auto* name=form->GetName();string(name?name:"");number(unsigned(magic->effects.size()));
    for(const auto* effect:magic->effects) {
        if(!effect||!effect->baseEffect){number(0);continue;}
        const auto* file=effect->baseEffect->GetFile(0);string(file?file->GetFilename():"");
        number(effect->baseEffect->GetLocalFormID());number(std::bit_cast<std::uint32_t>(effect->effectItem.magnitude));
        number(effect->effectItem.duration);number(effect->effectItem.area);
    }
    return hash?hash:1;
}
bool identify(RE::TESForm* form,Entry& entry) {
    if(!form)return false;
    if(auto* ammo=form->As<RE::TESAmmo>()) {
        if(!ammo->GetPlayable())return false;entry.kind=Kind::ammo;
    } else if(auto* scroll=form->As<RE::ScrollItem>()) {
        entry.kind=Kind::scroll;entry.twoHanded=scroll->IsTwoHanded();
    } else if(auto* alchemy=form->As<RE::AlchemyItem>()) {
        entry.kind=alchemy->IsPoison()?Kind::poison:alchemy->IsFood()?Kind::food:Kind::potion;
    } else if(form->As<RE::TESShout>())entry.kind=Kind::shout;
    else if(auto* spell=form->As<RE::SpellItem>()) {
        switch(spell->GetSpellType()) {
        case RE::MagicSystem::SpellType::kSpell:entry.kind=Kind::spell;entry.twoHanded=spell->IsTwoHanded();break;
        case RE::MagicSystem::SpellType::kPower:entry.kind=Kind::power;break;
        case RE::MagicSystem::SpellType::kLesserPower:entry.kind=Kind::lesserPower;break;
        default:return false;
        }
        entry.spell=true;
    } else if(auto* weapon=form->As<RE::TESObjectWEAP>()) {
        if(weapon->IsBound())return false;
        entry.twoHanded=weapon->IsTwoHandedSword()||weapon->IsTwoHandedAxe()||weapon->IsBow()||weapon->IsCrossbow();
        if(entry.twoHanded)entry.hand=Hand::right;
        entry.kind=weapon->IsBow()||weapon->IsCrossbow()?Kind::bow:weapon->IsStaff()?Kind::staff:
            weapon->IsOneHandedDagger()?Kind::dagger:weapon->IsOneHandedAxe()||weapon->IsTwoHandedAxe()?Kind::axe:
            weapon->IsOneHandedMace()?Kind::mace:entry.twoHanded?Kind::greatsword:Kind::sword;
    } else if(auto* armor=form->As<RE::TESObjectARMO>();armor&&armor->IsShield()) {
        entry.kind=Kind::shield;entry.hand=Hand::left;
    } else if(form->As<RE::TESObjectLIGH>()) {entry.kind=Kind::torch;entry.hand=Hand::left;}
    else return false;
    entry.hand=normalizedHand(entry,entry.hand);entry.icon=visualIcon(form);
    if(isStack(entry))entry.recipe=recipeSignature(form);
    return true;
}
bool shoutUnlocked(RE::TESShout* shout) {

    return shout&&shout->variations[0].word&&shout->variations[0].word->GetKnown();
}
struct Resolved {
    RE::TESBoundObject* object=nullptr;RE::ExtraDataList* extra=nullptr;RE::SpellItem* spell=nullptr;
    bool equipped=false,otherEquipped=false;
    RE::TESShout* shout=nullptr;int quantity=0;float cooldown=0;bool powerUsed=false;std::string detail;
    bool handAction=false;
    bool available()const{return object||spell||shout||handAction;}
};
bool usesBothHands(RE::TESForm* form) {
    if(!form)return false;
    if(auto* spell=form->As<RE::SpellItem>())return spell->IsTwoHanded();
    if(auto* scroll=form->As<RE::ScrollItem>())return scroll->IsTwoHanded();
    if(auto* weapon=form->As<RE::TESObjectWEAP>())return weapon->IsTwoHandedSword()||weapon->IsTwoHandedAxe()||weapon->IsBow()||weapon->IsCrossbow();
    return false;
}
std::optional<Hand> occupiedHand(RE::PlayerCharacter* player,Hand hand) {
    std::array<HandOccupancy,2> hands{};
    for(unsigned i=0;i<2;++i){auto* form=player->GetEquippedObject(i==1);hands[i]={form!=nullptr,usesBothHands(form)};}
    return handClearTarget(hands,hand);
}
std::vector<PotionCandidate> potionCandidates(const RE::TESObjectREFR::InventoryItemMap& inventory) {
    std::vector<PotionCandidate> result;
    for(const auto& [object,data]:inventory) {
        auto* potion=object->As<RE::AlchemyItem>();if(!potion||data.first<=0||potion->IsFood()||potion->IsPoison())continue;
        PotionCandidate c;c.entry.form=object->GetFormID();identify(object,c.entry);c.entry.name=gameName(object->GetName());c.count=data.first;
        for(const auto* effect:potion->effects) {
            if(!effect||!effect->baseEffect||effect->baseEffect->IsDetrimental()||!std::isfinite(effect->effectItem.magnitude)||effect->effectItem.magnitude<=0)continue;
            const auto& base=effect->baseEffect->data;
            if(base.archetype!=RE::EffectArchetype::kValueModifier&&base.archetype!=RE::EffectArchetype::kPeakValueModifier)continue;
            const float magnitude=effect->effectItem.magnitude;
            unsigned resource=3;
            switch(base.primaryAV){case RE::ActorValue::kHealth:resource=0;break;case RE::ActorValue::kMagicka:resource=1;break;case RE::ActorValue::kStamina:resource=2;break;default:break;}

            if(resource<3&&base.archetype==RE::EffectArchetype::kValueModifier&&!base.flags.all(RE::EffectSetting::EffectSettingData::Flag::kRecover))
                c.restore[resource]+=magnitude*float(std::max(1u,effect->effectItem.duration));
            resource=3;
            switch(base.primaryAV){
            case RE::ActorValue::kHealRateMult:resource=0;break;case RE::ActorValue::kMagickaRateMult:resource=1;break;case RE::ActorValue::kStaminaRateMult:resource=2;break;default:break;}

            if(resource<3)c.regeneration[resource]=std::max(c.regeneration[resource],magnitude);
        }
        result.push_back(std::move(c));
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.entry.name<b.entry.name;});return result;
}
struct NativeEquipment {EquipmentCandidate choice;RE::TESBoundObject* object=nullptr;RE::ExtraDataList* extra=nullptr;};
std::vector<NativeEquipment> equipmentCandidates(RE::PlayerCharacter* player,const RE::TESObjectREFR::InventoryItemMap& inventory) {
    std::vector<NativeEquipment> result;
    for(const auto& [object,data]:inventory) {
        if(data.first<=0||!data.second)continue;
        auto* weapon=object->As<RE::TESObjectWEAP>();auto* ammo=object->As<RE::TESAmmo>();auto* armor=object->As<RE::TESObjectARMO>();
        const bool shield=armor&&armor->IsShield()&&armor->GetPlayable();
        if((!weapon&&!ammo&&!shield)||(weapon&&(weapon->IsBound()||weapon->IsStaff()||!weapon->GetPlayable()))||(ammo&&!ammo->GetPlayable()))continue;
        Entry base;base.form=object->GetFormID();if(!identify(object,base))continue;
        base.name=gameName(object->GetName());
        WeaponFamily family=shield?WeaponFamily::shield:WeaponFamily::sword;
        if(weapon)family=weapon->IsOneHandedDagger()?WeaponFamily::dagger:weapon->IsOneHandedAxe()?WeaponFamily::axe:
            weapon->IsOneHandedMace()?WeaponFamily::mace:weapon->IsTwoHandedSword()?WeaponFamily::greatsword:
            weapon->IsTwoHandedAxe()?(weapon->HasKeywordString("WeapTypeWarhammer")?WeaponFamily::warhammer:WeaponFamily::battleaxe):
            weapon->IsCrossbow()?WeaponFamily::crossbow:weapon->IsBow()?WeaponFamily::bow:WeaponFamily::sword;
        const auto append=[&](RE::ExtraDataList* extra,int count) {
            if(count<=0||(extra&&count!=1&&!ammo))return;
            EquipmentCandidate c;c.entry=base;c.family=family;c.count=count;
            if(extra) {
                if(const auto* id=extra->GetByType<RE::ExtraUniqueID>()){c.entry.owner=id->baseID;c.entry.unique=id->uniqueID;}
                if(const char* name=extra->GetDisplayName(object);name&&*name)c.entry.name=gameName(name);
                auto right=base;right.hand=Hand::right;auto left=base;left.hand=Hand::left;
                c.rightEquipped=worn(extra,right)&&player->GetEquippedObject(false)==object;
                c.leftEquipped=worn(extra,left)&&player->GetEquippedObject(true)==object;
            }
            if(ammo){c.damage=ammo->GetRuntimeData().data.damage;c.rightEquipped=player->GetCurrentAmmo()==object;}
            else {RE::InventoryEntryData item(object,1);if(extra)item.AddExtraList(extra);c.damage=shield?player->GetArmorValue(&item):player->GetDamage(&item);}
            if(std::isfinite(c.damage)&&c.damage>0)result.push_back({std::move(c),object,extra});
        };
        if(ammo){append(nullptr,data.first);continue;}
        int plain=data.first;
        if(data.second->extraLists)for(auto* extra:*data.second->extraLists)if(extra){const int count=extra->GetCount();plain-=count;append(extra,count);}
        if(plain>0)append(nullptr,plain);
    }
    return result;
}
std::vector<EquipmentCandidate> equipmentChoices(const std::vector<NativeEquipment>& native) {
    std::vector<EquipmentCandidate> result;result.reserve(native.size());for(const auto& c:native)result.push_back(c.choice);return result;
}
std::array<float,3> actorDeficits(RE::PlayerCharacter* player) {
    std::array<float,3> result{};
    constexpr RE::ActorValue values[]{RE::ActorValue::kHealth,RE::ActorValue::kMagicka,RE::ActorValue::kStamina};
    for(unsigned i=0;i<3;++i)result[i]=std::max(0.f,-player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage,values[i]));
    return result;
}
struct ShortcutResolution {Entry action;Resolved item;Row row;std::array<Resolved,2> parts;};
Resolved resolveEquipment(const Entry& rule,const NativeEquipment& pick) {
    const auto& chosen=pick.choice;Resolved item;
    item.object=pick.object;item.extra=pick.extra;item.quantity=chosen.count;
    item.equipped=rule.hand==Hand::left?chosen.leftEquipped:chosen.rightEquipped;
    item.otherEquipped=!chosen.entry.twoHanded&&chosen.entry.kind!=Kind::ammo&&chosen.entry.kind!=Kind::shield&&
        (rule.hand==Hand::left?chosen.rightEquipped:chosen.leftEquipped);
    return item;
}
Row makeRow(const Entry& entry,const Resolved& item){return {entry,item.available(),item.equipped,item.detail,item.quantity,item.cooldown,item.powerUsed};}
Resolved resolve(const Entry& entry,RE::PlayerCharacter* player,const RE::TESObjectREFR::InventoryItemMap& inventory) {
    if(isHandAction(entry)) {
        Resolved result;if(!validHandAction(entry))return result;
        result.handAction=true;result.equipped=entry.kind==Kind::unarmed?
            !occupiedHand(player,Hand::right)&&!occupiedHand(player,Hand::left):!occupiedHand(player,entry.hand);
        return result;
    }
    if(entry.unresolved||!entry.form)return {};
    auto* form=RE::TESForm::LookupByID(entry.form);if(!form)return {};
    Entry verified;verified.hand=entry.hand;
    if(!identify(form,verified)||verified.spell!=entry.spell||verified.kind!=entry.kind)return {};
    if(verified.recipe!=entry.recipe){Resolved result;result.detail=tr("Potion changed · add it again");return result;}
    if(entry.kind==Kind::shout) {
        auto* shout=form->As<RE::TESShout>();
        if(!player->HasShout(shout))return {};
        Resolved result;
        if(!shoutUnlocked(shout)){result.detail=tr("Unlock a word in the Magic menu");return result;}
        result.shout=shout;result.equipped=player->GetActorRuntimeData().selectedPower==form;
        result.cooldown=std::max(0.f,player->GetVoiceRecoveryTime());return result;
    }
    if(entry.spell) {
        auto* spell=form->As<RE::SpellItem>();
        if(!player->HasSpell(spell))return {};
        if(isVoice(entry)) {
            Resolved result;result.spell=spell;result.equipped=player->GetActorRuntimeData().selectedPower==form;
            result.powerUsed=entry.kind==Kind::power&&player->IsInCastPowerList(spell);return result;
        }
        const bool equipped=entry.twoHanded?
            player->GetEquippedObject(true)==form||player->GetEquippedObject(false)==form:
            player->GetEquippedObject(entry.hand==Hand::left)==form;
        return {nullptr,nullptr,spell,equipped};
    }
    auto* object=form->As<RE::TESBoundObject>();
    const auto it=inventory.find(object);
    if(isStack(entry)) {
        Resolved result;
        if(it==inventory.end()||it->second.first<=0)return result;
        result.object=object;result.quantity=it->second.first;
        if(entry.kind==Kind::ammo)result.equipped=player->GetCurrentAmmo()==object;
        if(entry.kind==Kind::torch) {
            result.equipped=player->GetEquippedObject(true)==form;

            if(result.equipped&&it->second.second&&it->second.second->extraLists)
                for(auto* extra:*it->second.second->extraLists)if(worn(extra,entry)){result.extra=extra;break;}
        }
        if(entry.kind==Kind::scroll)result.equipped=entry.twoHanded?
            player->GetEquippedObject(true)==form||player->GetEquippedObject(false)==form:
            player->GetEquippedObject(entry.hand==Hand::left)==form;
        return result;
    }
    if(it==inventory.end()||it->second.first<=0||!it->second.second||!it->second.second->extraLists)return {};
    RE::ExtraDataList* found=nullptr;
    for(auto* extra:*it->second.second->extraLists) {
        if(!extra)continue;
        auto* id=extra->GetByType<RE::ExtraUniqueID>();
        if(id&&id->uniqueID==entry.unique&&id->baseID==entry.owner) {
            if(found&&found!=extra)return {};
            found=extra;
        }
    }
    if(!found)return {};
    Entry other=entry;other.hand=entry.hand==Hand::left?Hand::right:Hand::left;
    const bool inOther=!entry.twoHanded&&entry.kind!=Kind::shield&&entry.kind!=Kind::torch&&
        worn(found,other)&&player->GetEquippedObject(other.hand==Hand::left)==form;
    const bool inHand=entry.twoHanded?player->GetEquippedObject(true)==form||player->GetEquippedObject(false)==form:
        player->GetEquippedObject(entry.hand==Hand::left)==form;
    return {object,found,nullptr,worn(found,entry)&&inHand,inOther};
}
ShortcutResolution resolveShortcut(const Entry& entry,RE::PlayerCharacter* player,const RE::TESObjectREFR::InventoryItemMap& inventory,
                                    const std::vector<PotionCandidate>& potions,const std::array<float,3>& deficits,const std::vector<NativeEquipment>* equipment=nullptr) {
    ShortcutResolution result;result.action=entry;
    if(isSmartEquipment(entry)) {
        result.row.entry=entry;result.row.entry.name=smartEquipmentName(entry);result.row.entry.icon=smartEquipmentIcon(entry);
        const auto generated=equipment?std::vector<NativeEquipment>{}:equipmentCandidates(player,inventory);const auto& native=equipment?*equipment:generated;const auto choices=equipmentChoices(native);
        if(const auto* chosen=chooseEquipment(entry,choices)) {
            const auto& pick=native[std::size_t(chosen-choices.data())];result.action=chosen->entry;result.action.hand=entry.hand;
            result.item=resolveEquipment(entry,pick);
            result.row=makeRow(entry,result.item);result.row.entry.name=smartEquipmentName(entry);result.row.entry.icon=baseIcon(chosen->entry);
            result.row.detail=chosen->entry.name+(entry.kind==Kind::smartAmmo?" ×"+std::to_string(chosen->count):"");
        }else result.row.detail=entry.kind==Kind::smartAmmo?tr("No matching ammo"):smartShield(entry)?tr("No matching shield"):tr("No matching weapon");
    } else if(entry.kind==Kind::smartPotion) {
        result.row.entry=entry;result.row.entry.icon=smartIcon(entry.resource);
        if(const auto* bottle=choosePotion(entry,potions,deficits[std::min(entry.resource,2u)])) {
            result.action=bottle->entry;result.item=resolve(result.action,player,inventory);result.row=makeRow(entry,result.item);result.row.entry.icon=smartIcon(entry.resource);
            const auto feedback=potionFeedback(entry,bottle,deficits[std::min(entry.resource,2u)]);
            result.row.detail=bottle->entry.name+" ×"+std::to_string(result.item.quantity);
            result.row.feedback=feedback.reason+tr("\n")+feedback.estimate;
        }else {const auto feedback=potionFeedback(entry,nullptr,deficits[std::min(entry.resource,2u)]);result.row.detail=feedback.reason;result.row.feedback=feedback.estimate;}
    } else if(entry.kind==Kind::combo) {
        result.row.entry=entry;result.row.available=entry.parts.size()==2;result.row.equipped=result.row.available;
        const bool smart=hasSmartEquipment(entry);
        const auto generated=smart&&!equipment?equipmentCandidates(player,inventory):std::vector<NativeEquipment>{};
        const auto& native=equipment?*equipment:generated;
        const auto plan=smart?chooseCombination(entry,equipmentChoices(native)):std::optional{CombinationChoices{noEquipmentCandidate,noEquipmentCandidate}};
        for(unsigned hand=0;hand<entry.parts.size()&&hand<2;++hand) {
            auto& part=result.row.entry.parts[hand];auto& item=result.parts[hand];
            if(isSmartEquipment(part)) {
                part.icon=smartEquipmentIcon(part);
                if(plan&&(*plan)[hand]!=noEquipmentCandidate) {
                    const auto& pick=native[(*plan)[hand]];item=resolveEquipment(part,pick);
                    auto& action=result.action.parts[hand];action=pick.choice.entry;action.hand=Hand(hand);part.icon=baseIcon(action);
                }
            }else {part.icon=visualIcon(part.unresolved?nullptr:RE::TESForm::LookupByID(part.form));item=resolve(part,player,inventory);}
            result.row.available&=item.available();result.row.equipped&=item.equipped;
            if(!item.available())result.row.detail=tr("Missing ")+handName(part)+": "+displayName(part);
        }
        if(entry.parts.size()==2&&entry.parts[0].kind==Kind::scroll&&entry.parts[1].kind==Kind::scroll&&entry.parts[0].form==entry.parts[1].form&&resolve(entry.parts[0],player,inventory).quantity<2){result.row.available=false;result.row.detail=tr("Two scrolls needed for both hands");}
        if(result.row.detail.empty())result.row.detail=smart&&result.action.parts.size()==2?
            result.action.parts[0].name+" + "+result.action.parts[1].name:result.row.equipped?tr("Combination active"):tr("Right + left hand");
        result.row.equipped&=result.row.available;
        if(!entry.parts.empty())result.row.entry.icon=baseIcon(result.row.entry.parts[0]);
    } else {result.item=resolve(entry,player,inventory);result.row=makeRow(entry,result.item);result.row.entry.icon=visualIcon(entry.unresolved?nullptr:RE::TESForm::LookupByID(entry.form));}
    if(!isSmartEquipment(entry)&&entry.kind!=Kind::smartPotion&&entry.kind!=Kind::combo&&!isHandAction(entry)) {
        if(auto* form=RE::TESForm::LookupByID(entry.form)) {
            const char* name=result.item.extra?result.item.extra->GetDisplayName(result.item.object):form->GetName();
            if(name&&*name)result.row.entry.name=result.action.name=gameName(name);
        }
    }
    return result;
}
RE::TESObjectREFR::InventoryItemMap inventoryFor(const Arsenal& arsenal,RE::PlayerCharacter* player) {
    std::unordered_set<RE::FormID> forms;
    bool smart=false,weapons=false,ammo=false;
    for(const auto& group:arsenal.groups)for(const auto& saved:group.entries)eachLeaf(saved,[&](const Entry& e){
        smart|=e.kind==Kind::smartPotion;weapons|=e.kind==Kind::smartWeapon;ammo|=e.kind==Kind::smartAmmo;
        if(isInventory(e)&&!e.unresolved)forms.insert(e.form);
    });
    for(bool left:{false,true})if(auto* form=player->GetEquippedObject(left))forms.insert(form->GetFormID());
    if(forms.empty()&&!smart&&!weapons&&!ammo)return {};
    return player->GetInventory([&](RE::TESBoundObject& object){return forms.contains(object.GetFormID())||(smart&&object.As<RE::AlchemyItem>())||
        (weapons&&(object.As<RE::TESObjectWEAP>()||(object.As<RE::TESObjectARMO>()&&object.As<RE::TESObjectARMO>()->IsShield())))||(ammo&&object.As<RE::TESAmmo>());});
}
// Read the equipped hands and voice slot for the equipment display.
EquippedDisplay readEquipment(RE::PlayerCharacter* player,const RE::TESObjectREFR::InventoryItemMap& inventory) {
    EquippedDisplay state;
    const auto read=[&](RE::TESForm* form,Hand hand)->std::optional<Row> {
        if(!form)return {};
        Entry e;e.form=form->GetFormID();e.hand=hand;e.name=gameName(form->GetName()?form->GetName():"");

        if(!identify(form,e)){e.icon=visualIcon(form);if(auto* w=form->As<RE::TESObjectWEAP>())e.twoHanded=w->IsTwoHandedSword()||w->IsTwoHandedAxe()||w->IsBow()||w->IsCrossbow();}
        Row row{e,true,true};
        if(auto* object=form->As<RE::TESBoundObject>();object)if(auto it=inventory.find(object);it!=inventory.end()&&it->second.second) {
            row.quantity=it->second.first;
            if(auto* lists=it->second.second->extraLists)for(auto* extra:*lists)if(extra&&worn(extra,e)) {
                if(const char* name=extra->GetDisplayName(object);name&&*name)row.entry.name=gameName(name);
                if(tracksInstance(e))if(auto* id=extra->GetByType<RE::ExtraUniqueID>()){row.entry.owner=id->baseID;row.entry.unique=id->uniqueID;}
                break;
            }
        }
        if(e.kind==Kind::shout)row.cooldown=std::max(0.f,player->GetVoiceRecoveryTime());
        if(e.kind==Kind::power)if(auto* spell=form->As<RE::SpellItem>())row.powerUsed=player->IsInCastPowerList(spell);
        return row;
    };
    for(unsigned hand=0;hand<2;++hand)state.hands[hand]=read(player->GetEquippedObject(hand==1),Hand(hand));
    for(unsigned hand=0;hand<2;++hand)if(state.hands[hand]&&state.hands[hand]->entry.twoHanded) {
        state.hands[1-hand]=state.hands[hand];break;
    }
    state.voice=read(player->GetActorRuntimeData().selectedPower,Hand::right);
    return state;
}
RE::BGSEquipSlot* handSlot(Hand hand) {
    auto* defaults=RE::BGSDefaultObjectManager::GetSingleton();
    return defaults?defaults->GetObject<RE::BGSEquipSlot>(hand==Hand::left?RE::DEFAULT_OBJECTS::kLeftHandEquip:RE::DEFAULT_OBJECTS::kRightHandEquip):nullptr;
}
void clearHeldHand(RE::PlayerCharacter*,Hand);
template<class... Args>
// Dispatch an Actor method through Skyrim VR's script VM.
void vrActorCall(RE::PlayerCharacter* player,const char* function,Args... args) {

    auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
    auto* policy=vm?vm->GetObjectHandlePolicy():nullptr;
    if(!policy)throw std::runtime_error("Skyrim script VM is unavailable");
    const auto handle=policy->GetHandleForObject(player->GetFormType(),player);
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    if(!vm->DispatchMethodCall(handle,"Actor",function,RE::MakeFunctionArguments(std::move(args)...),callback))
        throw std::runtime_error(std::string("Skyrim could not dispatch ")+function);
}
void changeEquipment(RE::PlayerCharacter* player,const Entry& entry,const Resolved& item,bool remove,bool otherHand=false) {
    if(entry.kind==Kind::emptyHand){clearHeldHand(player,entry.hand);return;}
    auto* manager=RE::ActorEquipManager::GetSingleton();
    const auto hand=otherHand?(entry.hand==Hand::left?Hand::right:Hand::left):entry.hand;
    auto* defaults=RE::BGSDefaultObjectManager::GetSingleton();
    auto* slot=isVoice(entry)?(defaults?defaults->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECTS::kVoiceEquip):nullptr):handSlot(hand);
    if(!manager||!slot)throw std::runtime_error("Equipment system is not ready");
    if(item.shout) {
        if(!remove)manager->EquipShout(player,item.shout);
        else if(REL::Module::IsVR())vrActorCall(player,"UnequipShout",item.shout);
        else {
            using UnequipShout=void(RE::ActorEquipManager*,RE::Actor*,RE::TESShout*);
            static REL::Relocation<UnequipShout> unequip{RELOCATION_ID(37948,38904)};
            unequip(manager,player,item.shout);
        }
    } else if(item.spell) {
        if(!remove)manager->EquipSpell(player,item.spell,entry.twoHanded?item.spell->GetEquipSlot():slot);
        else if(REL::Module::IsVR()) {
            if(entry.twoHanded) {
                if(player->GetEquippedObject(true)==item.spell)vrActorCall(player,"UnequipSpell",item.spell,std::int32_t(0));
                if(player->GetEquippedObject(false)==item.spell)vrActorCall(player,"UnequipSpell",item.spell,std::int32_t(1));
            }else vrActorCall(player,"UnequipSpell",item.spell,std::int32_t(isVoice(entry)?2:hand==Hand::left?0:1));
        }
        else {

            using UnequipSpell=void(RE::ActorEquipManager*,RE::Actor*,RE::SpellItem*,int);
            static REL::Relocation<UnequipSpell> unequip{RELOCATION_ID(37947,38903)};
            if(entry.twoHanded) {
                if(player->GetEquippedObject(true)==item.spell)unequip(manager,player,item.spell,0);
                if(player->GetEquippedObject(false)==item.spell)unequip(manager,player,item.spell,1);
            } else unequip(manager,player,item.spell,isVoice(entry)?2:hand==Hand::left?0:1);
        }
    } else if(item.object) {
        const auto objectHand=objectEquipHand(entry,remove,otherHand);
        const auto* objectSlot=objectHand?handSlot(*objectHand):nullptr;
        if(remove)manager->UnequipObject(player,item.object,item.extra,1,objectSlot,false,false,true,false);
        else manager->EquipObject(player,item.object,item.extra,1,objectSlot,false,false,true,false);
    }
}
void clearHeldHand(RE::PlayerCharacter* player,Hand hand) {
    const auto target=occupiedHand(player,hand);if(!target)return;
    auto* form=player->GetEquippedObject(*target==Hand::left);if(!form)return;
    Entry held;held.form=form->GetFormID();held.hand=*target;held.twoHanded=usesBothHands(form);
    Resolved item;
    if(auto* spell=form->As<RE::SpellItem>()) {
        held.kind=Kind::spell;held.spell=true;item.spell=spell;
        log::info("Clear hand requested: target {}, held slot {}, spell {:08X}, both hands {}",unsigned(hand),unsigned(*target),held.form,held.twoHanded);
        changeEquipment(player,held,item,true);return;
    }
    auto* object=form->As<RE::TESBoundObject>();if(!object)throw std::runtime_error("Unsupported held form");
    if(auto* armor=form->As<RE::TESObjectARMO>();armor&&armor->IsShield())held.kind=Kind::shield;
    else if(form->As<RE::TESObjectLIGH>())held.kind=Kind::torch;
    else if(form->As<RE::ScrollItem>())held.kind=Kind::scroll;

    auto inventory=player->GetInventory([&](RE::TESBoundObject& candidate){return &candidate==object;});
    auto* data=player->GetEquippedEntryData(*target==Hand::left);
    if(!data||data->object!=object){const auto it=inventory.find(object);data=it==inventory.end()?nullptr:it->second.second.get();}
    if(data&&data->extraLists)for(auto* extra:*data->extraLists)if(worn(extra,held)) {
        if(item.extra&&item.extra!=extra)throw std::runtime_error("Ambiguous equipped copy while clearing hand");
        item.extra=extra;
    }
    if(!item.extra&&held.kind!=Kind::scroll)throw std::runtime_error("Equipped copy not found while clearing hand");
    log::info("Clear hand requested: target {}, held slot {}, object {:08X}, both hands {}",unsigned(hand),unsigned(*target),held.form,held.twoHanded);
    item.object=object;changeEquipment(player,held,item,true);
}
}
double clockNow() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::filesystem::path dataDirectory() {
    std::wstring file(32768,L'\0');const auto count=GetModuleFileNameW(nullptr,file.data(),DWORD(file.size()));
    if(!count||count==file.size())throw std::runtime_error("Cannot find Skyrim directory");
    file.resize(count);return std::filesystem::path(file).parent_path()/"Data";
}
Game& Game::get(){static auto* game=new Game;return *game;}
bool Game::initialize() {
    std::scoped_lock lock(mutex_);if(initialized_)return true;
    if(!initializeVROverlay())return false;
    auto* ui=RE::UI::GetSingleton();if(!ui||!RE::BSInputDeviceManager::GetSingleton())return false;
    std::string gameLanguage="english";
    if(auto* settings=RE::INISettingCollection::GetSingleton())if(auto* value=settings->GetSetting("sLanguage:General"))if(const char* language=value->GetString())gameLanguage=language;
    localization().load(dataDirectory()/"Quickslot/translations",gameLanguage);
    for(const auto& warning:localization().warnings())log::warn("Translation: {}",warning);
    const auto ini=dataDirectory()/"SKSE/Plugins/Quickslot.ini";
    for(std::size_t i=0;i<Arsenal::count;++i) {
        const auto key=i?L"Group"+std::to_wstring(i+1)+L"Key":std::wstring(L"GroupKey");
        options_.keys[i]=GetPrivateProfileIntW(L"Controls",key.c_str(),unsigned(i+2),ini.c_str());
        char name[256]{};const auto field="Name"+std::to_string(i+1);
        GetPrivateProfileStringA("Groups",field.c_str(),options_.names[i].c_str(),name,sizeof(name),ini.string().c_str());
        if(*name)options_.names[i]=gameName(name);
    }
    options_.editorKey=GetPrivateProfileIntW(L"Controls",L"EditorKey",options_.editorKey,ini.c_str());
    options_.editorModifiers=GetPrivateProfileIntW(L"Controls",L"EditorModifiers",options_.editorModifiers,ini.c_str());
    selection_.holdSeconds=std::clamp(GetPrivateProfileIntW(L"Controls",L"HoldMilliseconds",180,ini.c_str()),100u,800u)/1000.;
    options_.scale=std::clamp(GetPrivateProfileIntW(L"Appearance",L"Scale",100,ini.c_str()),70u,150u)/100.f;
    options_.opacity=std::clamp(GetPrivateProfileIntW(L"Appearance",L"Opacity",92,ini.c_str()),40u,100u)/100.f;
    options_.showIdle=GetPrivateProfileIntW(L"Appearance",L"ShowIdle",1,ini.c_str())!=0;
    sanitizeOptions(options_);
    try {
        const auto path=dataDirectory()/"SKSE/Plugins/Quickslot.settings.json";
        if(std::filesystem::exists(path)) {
            if(std::filesystem::file_size(path)>65536)throw std::runtime_error("Settings file too large");
            std::ifstream file(path);if(!file)throw std::runtime_error("Cannot open settings");
            options_=decodeOptions(std::string(std::istreambuf_iterator<char>(file),{}));
            log::info("Loaded editor settings");
        }
    }catch(const std::exception& e){log::warn("Editor settings ignored; using INI defaults: {}",e.what());}
    localization().select(options_.language);

    const REL::Relocation<std::uintptr_t> poll{RELOCATION_ID(67315,68617)};
    const auto address=poll.address()+runtimeVersion(REL::Module::get().version()).inputCallOffset();
    const auto* bytes=reinterpret_cast<const std::uint8_t*>(address);
    const auto call=decodeCall(address,std::span(bytes,6),[](std::uintptr_t slot)->std::optional<std::uintptr_t> {
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<void*>(slot),&region,sizeof(region))||region.State!=MEM_COMMIT||
           (region.Protect&(PAGE_NOACCESS|PAGE_GUARD))||slot+sizeof(std::uintptr_t)>reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize)return {};
        std::uintptr_t target=0;std::memcpy(&target,reinterpret_cast<void*>(slot),sizeof(target));return target;
    });
    MEMORY_BASIC_INFORMATION destination{};
    if(!call||!VirtualQuery(reinterpret_cast<void*>(call->target),&destination,sizeof(destination))||destination.State!=MEM_COMMIT||
       !(destination.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||
       (destination.Protect&PAGE_GUARD)) {
        log::error("Input dispatch signature/target mismatch at {:X}; Quickslot disabled",address);return false;
    }
    originalDispatch=call->target;
    auto& trampoline=SKSE::GetTrampoline();
    if(call->size==5)trampoline.write_call<5>(address,inputHook);
    else trampoline.write_call<6>(address,inputHook);
    if(!installInventoryHooks())log::error("Inventory hook targets rejected; pickup recovery unavailable");
    else log::info("Exact-copy pickup/container tracking installed");
    ui->Register(slowMenuName,SlowMenu::create);ui->Register(pauseMenuName,PauseMenu::create);ui->Register(editorMenuName,EditorMenu::create);
    ui->Register(menuName,Menu::create);ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
    auto* events=RE::ScriptEventSourceHolder::GetSingleton();
    events->AddEventSink<RE::TESContainerChangedEvent>(this);events->AddEventSink<RE::TESEquipEvent>(this);
    events->AddEventSink<RE::TESUniqueIDChangeEvent>(this);
    initialized_=true;
    if(registerMenuFramework({[]{return Game::get().frameworkStatus();},[]{return Game::get().requestEditorFromFramework();}}))
        log::info("Registered SKSE Menu Framework page: Quickslot / Equipment & widget");
    else log::info("SKSE Menu Framework unavailable; keyboard and controller editor shortcuts remain available");
    log::info("Quickslot initialized; group scans {}/{}/{}, hold {}ms. Native cursorless HUD.",
              options_.keys[0],options_.keys[1],options_.keys[2],int(selection_.holdSeconds*1000));
    log::info("Editor scan {}, modifiers {}; {} visible groups. Editor pauses; optional selection slowdown is configurable.",options_.editorKey,options_.editorModifiers,options_.groupCount);
    return true;
}
bool Game::allowed(MenuPurpose purpose)const {
    auto* ui=RE::UI::GetSingleton();auto* player=RE::PlayerCharacter::GetSingleton();auto* controls=RE::ControlMap::GetSingleton();
    const auto ownPause=ui?ui->GetMenu(pauseMenuName):nullptr;
    return initialized_&&ready_&&!editor_.open&&ui&&player&&controls&&player->Is3DLoaded()&&!player->IsDead()&&
        !player->IsInKillMove()&&selectionPauseAllowsInput(ui->numPausesGame,ownPause&&ownPause->PausesGame()&&ui->IsMenuOpen(pauseMenuName))&&ui->IsShowingMenus()&&ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)&&
        !foreignMenu(ui,purpose)&&!frameworkCapturingInput()&&!vrOtherOverlayOpen()&&controls->GetRuntimeData().textEntryCount<=0&&controls->IsMovementControlsEnabled()&&renderHasFocus();
}
void Game::selectionTime(bool selecting) {
    if(!initialized_)return;
    const auto mode=selectionClock(options_.selectionSpeed);
    const auto menu=[&](const char* name,bool wants,bool& requested){
        if(wants!=requested)if(auto* queue=RE::UIMessageQueue::GetSingleton()) {
            requested=wants;queue->AddMessage(name,wants?RE::UI_MESSAGE_TYPE::kShow:RE::UI_MESSAGE_TYPE::kHide,nullptr);
        }
    };
    menu(pauseMenuName,selecting&&mode==SelectionClock::pause,pauseRequested_);
    const bool wants=selecting&&mode==SelectionClock::slow;
    if(GetModuleHandleW(L"SkyrimSoulsRE.dll")) {
        menu(slowMenuName,wants,soulsRequested_);
        return;
    }

    static REL::Relocation<float*> current{RELOCATION_ID(511882,388442)},target{RELOCATION_ID(511883,388443)};
    if(const auto change=selectionTime_.update(wants,options_.selectionSpeed,*current,*target)){*current=change->first;*target=change->second;}
}
void Game::cancel(){selectionTime(false);selection_.cancel();controller_.cancel();pending_.reset();released_.reset();stopVoice();}
void Game::notice(std::string text,bool visible){log::info("{}",text);snapshot_.notice=visible?std::move(text):"";snapshot_.noticeUntil=visible?clockNow()+3.5:0;}
void Game::publish(){
    snapshot_.vr=REL::Module::IsVR();snapshot_.controller=controllerLast_&&options_.controllerEnabled;snapshot_.activeGroup=arsenal_.active;
    snapshot_.holding=selection_.held();snapshot_.selected=arsenal_.current().selected;snapshot_.preview=selection_.index();snapshot_.browsing=selection_.browsing();snapshot_.visible=allowed(MenuPurpose::hud);
    snapshot_.inCombat=snapshot_.weaponsDrawn=false;
    if(snapshot_.visible)if(auto* player=RE::PlayerCharacter::GetSingleton()) {
        snapshot_.inCombat=player->IsInCombat();snapshot_.weaponsDrawn=player->AsActorState()->IsWeaponDrawn();
    }
}
void Game::loaded(){std::scoped_lock lock(mutex_);closeEditor("game loaded");cancel();use_.reset();groupRecall_.reset();
    if(arsenal_.presetIdentity.empty()){GUID id{};if(SUCCEEDED(CoCreateGuid(&id))){char key[40];std::snprintf(key,sizeof(key),"%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",id.Data1,id.Data2,id.Data3,id.Data4[0],id.Data4[1],id.Data4[2],id.Data4[3],id.Data4[4],id.Data4[5],id.Data4[6],id.Data4[7]);arsenal_.presetIdentity=key;}}
    if(auto* devices=RE::BSInputDeviceManager::GetSingleton())controllerLast_=controllerConnected_=REL::Module::IsVR()||devices->IsGamepadEnabled();
    revealSavedGroups(options_,arsenal_);ready_=true;menuAPI().publish(true,false);dirty_=true;menuRequested_=false;log::info("Game ready; {} saved entries across {} groups",arsenal_.size(),Arsenal::count);}
void Game::reset(bool clear) {
    std::scoped_lock lock(mutex_);++menuCaptureEpoch_;closeEditor("game reset");cancel();use_.reset();stopAnimationWatch();ready_=false;menuAPI().publish(false,false);dirty_=true;menuRequested_=false;
    if(clear)arsenal_=Arsenal{};
    snapshot_=Snapshot{};groupRecall_.reset();keyboardOwned_.fill(false);mouseOwned_.fill(false);controller_.reset();stickOwned_.fill(false);nativeVoiceDown_=false;
}
void Game::queueEdit(int action,std::optional<std::size_t> index) {
    cancel();const auto token=selection_.token();const auto groupID=arsenal_.active;
    SKSE::GetTaskInterface()->AddTask([this,token,action,index,groupID] {
        std::scoped_lock lock(mutex_);
        if(token!=selection_.token()||groupID!=arsenal_.active||!allowed())return;
        try {
            auto& group=arsenal_.current();
            if(action==2) {
                if(index&&*index<group.entries.size())group.selected=*index;
                if(group.remove())notice(tr("Removed from ")+options_.names[groupID]);else notice(tr("This group is empty"));dirty_=true;
            }
            else capture(action?Hand::left:Hand::right);
            refresh();publish();
        }catch(const std::exception& e){log::error("Edit failed: {}",e.what());notice(tr("Could not update Arsenal"));}
    });
}
bool Game::accepts(const Selection::Commit& commit)const {
    return commit.group<Arsenal::count&&commit.group==arsenal_.active&&
        selection_.accepts(commit,arsenal_.groups[commit.group],commit.group);
}
void Game::activate(std::size_t group) {
    ++snapshot_.interaction;snapshot_.revealUntil=clockNow()+2;
    if(group==arsenal_.active)return;
    cancel();arsenal_.active=group;snapshot_.rows.clear();dirty_=true;
}
void Game::queueUnequip(std::optional<std::size_t> index) {
    pending_.reset();released_.reset();selectionTime(false);const auto& group=arsenal_.current();
    if(auto commit=selection_.request(group,arsenal_.active,index.value_or(group.selected)))
        released_=Released{*commit,true};
    else notice(tr("This group is empty"));
}
void Game::queueReorder(int delta) {
    const auto token=selection_.token(),revision=arsenal_.current().revision;
    const auto groupID=arsenal_.active,index=selection_.index();
    SKSE::GetTaskInterface()->AddTask([this,token,revision,groupID,index,delta] {
        std::scoped_lock lock(mutex_);
        if(token!=selection_.token()||!selection_.held()||groupID!=arsenal_.active||!allowed())return;
        auto& group=arsenal_.current();if(group.revision!=revision)return;
        const auto target=wrappedIndex(index,delta,group.entries.size());
        if(group.reorder(index,target)) {
            selection_.rebase(group,target);dirty_=true;refresh();publish();
            notice(tr("Moved to position ")+std::to_string(target+1));
        }
    });
}
void Game::queueMove(std::size_t target) {
    if(target>=Arsenal::count)return;
    const auto source=arsenal_.active,index=selection_.index();cancel();const auto token=selection_.token();
    SKSE::GetTaskInterface()->AddTask([this,source,index,target,token] {
        std::scoped_lock lock(mutex_);
        if(token!=selection_.token()||!allowed())return;
        if(arsenal_.move(source,index,target,options_.autoSort[target])) {
            dirty_=true;refresh();publish();notice(tr("Moved to ")+options_.names[target]);
        } else notice(tr("Could not move entry — destination is full or entry is missing"));
    });
}
void Game::beginSelection(std::size_t group,bool controller) {
    selectionController_=controller;
    const bool recall=groupRecall_.activate(group);
    if(groupMode(options_,arsenal_,group)==GroupMode::cycle) {
        if(dirty_)refresh();
        std::vector<bool> available;for(const auto& row:snapshot_.rows)available.push_back(row.available);
        const auto index=cycleEntry(arsenal_.current(),available,options_.skipEmpty,recall);
        if(auto commit=selection_.requestCycle(arsenal_.current(),group,index)) {
            commit->controller=controller;
            arsenal_.current().selected=index;released_=Released{*commit};
            log::info("Cycle {}: group {}, entry {}",recall?"recall":"advance",group+1,index+1);
        }

        controller_.cancel();
    } else selection_.press(clockNow(),arsenal_.current(),group);
    dirty_=true;
}
void Game::finishSelection() {
    selection_.tick(clockNow());
    if(auto commit=selection_.release(arsenal_.current(),arsenal_.active)) {
        commit->controller=selectionController_;
        released_=Released{*commit};snapshot_.revealUntil=clockNow()+2;
    }
    selectionTime(false);
}
void Game::cycleSelection(int delta) {
    if(!selection_.canBrowse())return;
    if(options_.skipEmpty){if(dirty_)refresh();std::vector<bool> available;for(const auto& row:snapshot_.rows)available.push_back(row.available);selection_.cycleAvailable(delta,arsenal_.current(),available);}
    else selection_.cycle(delta,arsenal_.current());
    selectionTime(true);
}
bool Game::consumeController(RE::InputEvent* event) {
    const bool vr=REL::Module::IsVR()&&vrDevice(unsigned(event->GetDevice()));

    if(vr&&(editor_.open||editorRequested_))return true;
    if(vr&&vrOtherOverlayOpen())return false;
    auto* stick=event->AsThumbstickEvent();
    const bool touchpad=vr&&(event->GetEventType()==RE::INPUT_EVENT_TYPE::kVrTouchpadPosition||event->GetEventType()==RE::INPUT_EVENT_TYPE::kVrTouchpadSwipe);
    if(stick||touchpad) {
        const bool right=vr?vrMainHand(unsigned(event->GetDevice()),RE::BSOpenVRControllerDevice::IsLeftHandedMode()):stick->IsRight();const unsigned index=right?1:0;
        const auto axis=touchpad?readVRAxis(RE::BSInputDeviceManager::GetSingleton(),right):RE::NiPoint2{stick->xValue,stick->yValue};
        const bool neutral=std::abs(axis.x)<.15f&&std::abs(axis.y)<.15f;
        if(!neutral){controllerLast_=true;editor_.controller=true;}
        if(editor_.open||editorRequested_) {
            editorInput_.push_back({EditorInputType::stick,index,false,axis.x,axis.y});
            stickOwned_[index]=!neutral;return true;
        }
        auto* ui=RE::UI::GetSingleton();
        const bool menuOpen=ui&&foreignMenu(ui);
        const bool browsing=!menuOpen&&options_.controllerEnabled&&selection_.canBrowse()&&allowed()&&(vr?options_.vrStick:options_.controllerStick)==index;
        const bool captured=captureGameplayStick(stickOwned_[index],neutral,browsing,menuOpen);
        if(browsing) {

            if(!vr)if(const auto delta=controller_.stick(axis.x,axis.y,clockNow(),options_.controllerDeadzone))cycleSelection(delta);

            if(auto* controls=RE::PlayerControls::GetSingleton()) {
                if(right)controls->data.lookInputVec={};else controls->data.moveInputVec={};
            }
        }
        return captured;
    }
    auto* eventButton=event->AsButtonEvent();if(!eventButton)return editor_.open||editorRequested_;
    const auto button=vr?vrButton(unsigned(event->GetDevice()),eventButton->GetIDCode(),RE::BSOpenVRControllerDevice::IsLeftHandedMode()):padButton(eventButton->GetIDCode(),padCodeSet());
    const bool down=eventButton->IsPressed()||(vr&&readVRState(RE::BSInputDeviceManager::GetSingleton())[unsigned(button)]);
    const bool fresh=eventButton->IsDown()&&(!vr||!controller_.down(button));
    if(fresh){controllerLast_=true;editor_.controller=true;}
    if(editor_.open||editorRequested_) {
        const bool captured=controller_.editorButton(button,down,fresh);
        if(fresh||!down)editorInput_.push_back({EditorInputType::pad,unsigned(button),down});
        return captured;
    }
    const auto result=controller_.button(button,down,fresh,options_,allowed(),selection_.held(),vr,selection_.canBrowse(),menuAPI().hotkeyEnabled());
    switch(result.action) {
    case PadAction::press: {
        const auto binding=controller_.binding();
        if(voice_.active()){controller_.cancel();notice(tr("Voice action in progress"));break;}
        cancel();activate(result.group);controller_.begin(binding,result.group);beginSelection(result.group,true);break;
    }
    case PadAction::release:
        if(selectionController_&&selection_.held()&&selection_.group()==result.group) {
            finishSelection();
        }break;
    case PadAction::cancel:cancel();notice(tr("Selection cancelled"),false);break;
    case PadAction::previous:cycleSelection(-1);break;
    case PadAction::next:cycleSelection(1);break;
    case PadAction::unequip:queueUnequip(selection_.index());break;
    case PadAction::editor:cancel();editorRequested_=true;SKSE::GetTaskInterface()->AddTask([this]{openEditor();});break;
    default:break;
    }
    return result.consumed;
}
bool Game::consume(RE::InputEvent* event) {
    if(!event)return false;
    if(event->device==RE::INPUT_DEVICE::kGamepad||(REL::Module::IsVR()&&vrDevice(unsigned(event->GetDevice()))))return consumeController(event);
    if(auto* button=event->AsButtonEvent();button&&button->IsDown()){controllerLast_=false;editor_.controller=false;}
    if(auto* move=event->AsMouseMoveEvent();move&&(std::abs(move->mouseInputX)>1||std::abs(move->mouseInputY)>1)){controllerLast_=false;editor_.controller=false;}
    if(editor_.open||editorRequested_) {
        if(!event)return false;
        bool captured=true;
        if(auto* character=event->AsCharEvent())editorInput_.push_back({EditorInputType::character,character->keyCode});
        else if(auto* move=event->AsMouseMoveEvent())editorInput_.push_back({EditorInputType::move,0,false,float(move->mouseInputX),float(move->mouseInputY)});
        else if(auto* button=event->AsButtonEvent()) {
            const auto code=button->GetIDCode();const bool down=button->Value()!=0;
            if(event->device==RE::INPUT_DEVICE::kKeyboard&&code<keyboardOwned_.size()) {
                captured=captureEditorButton(keyboardOwned_[code],down,button->IsDown());
                if(button->IsDown()||!down)editorInput_.push_back({EditorInputType::key,code,down});
            } else if(event->device==RE::INPUT_DEVICE::kMouse&&code<mouseOwned_.size()) {
                if(code<8)captured=captureEditorButton(mouseOwned_[code],down,button->IsDown());
                if(code<5&&(button->IsDown()||!down))editorInput_.push_back({EditorInputType::button,code,down});
                else if(down&&(code==8||code==9))editorInput_.push_back({EditorInputType::wheel,0,false,0,code==8?1.f:-1.f});
            }
        }
        if(editorInput_.size()>2048){editorInput_.clear();SKSE::GetTaskInterface()->AddTask([this]{std::scoped_lock lock(mutex_);closeEditor("input queue overflow");});}
        return captured;
    }
    auto* button=event?event->AsButtonEvent():nullptr;if(!button)return false;
    const auto code=button->GetIDCode();const bool keyboard=event->device==RE::INPUT_DEVICE::kKeyboard,mouse=event->device==RE::INPUT_DEVICE::kMouse;
    bool* owned=keyboard&&code<keyboardOwned_.size()?&keyboardOwned_[code]:mouse&&code<mouseOwned_.size()?&mouseOwned_[code]:nullptr;
    if(!owned)return false;
    const bool up=button->Value()==0;

    if(*owned&&button->IsDown()) {
        *owned=false;if(keyboard&&!selectionController_&&selection_.held()&&code==options_.keys[selection_.group()])cancel();
    }
    if(*owned) {
        if(up) {
            *owned=false;
            if(keyboard&&!selectionController_&&selection_.held()&&code==options_.keys[selection_.group()]) {
                if(!allowed())cancel();
                else {finishSelection();}
            }
        }
        return true;
    }
    if(keyboard&&code&&button->IsDown()&&options_.menuCapture&&captureMenuOpen()) {
        const auto it=std::find(options_.keys.begin(),options_.keys.end(),code);
        auto* device=RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
        const auto hand=menuCaptureHand(options_,keyboardModifiers([&](unsigned scan){return pressed(device,scan);}));
        if(it!=options_.keys.end()&&groupVisible(options_,std::size_t(it-options_.keys.begin()))&&hand) {
            *owned=true;menuCapture(std::size_t(it-options_.keys.begin()),*hand);return true;
        }
    }
    if(!allowed())return false;
    if(menuAPI().hotkeyEnabled()&&keyboard&&options_.editorKey&&code==options_.editorKey&&button->IsDown()) {
        auto* device=RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
        if(editorShortcutMatches(options_,code,keyboardModifiers([&](unsigned scan){return pressed(device,scan);}))) {
            *owned=true;cancel();editorRequested_=true;
            SKSE::GetTaskInterface()->AddTask([this]{openEditor();});return true;
        }
    }
    const auto key=std::find(options_.keys.begin(),options_.keys.end(),code);
    if(keyboard&&code&&key!=options_.keys.end()&&groupVisible(options_,std::size_t(key-options_.keys.begin()))&&button->IsDown()) {
        auto* device=RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
        const auto modifiers=keyboardModifiers([&](unsigned scan){return pressed(device,scan);});
        const auto groupID=std::size_t(key-options_.keys.begin());
        const auto action=groupKeyAction(options_,modifiers,selection_.held()&&groupID!=arsenal_.active);
        if(action==GroupKeyAction::pass)return false;
        *owned=true;
        if(voice_.active()){notice(tr("Voice action in progress"));return true;}
        if(action==GroupKeyAction::move){queueMove(groupID);return true;}
        activate(groupID);
        if(action==GroupKeyAction::captureRight||action==GroupKeyAction::captureLeft)queueEdit(action==GroupKeyAction::captureLeft?1:0);
        else if(action==GroupKeyAction::remove)queueEdit(2);
        else if(action==GroupKeyAction::unequip)queueUnequip();
        else {cancel();beginSelection(groupID,false);}
        return true;
    }
    if(selection_.held()) {

        if(keyboard&&(code==29||code==157)&&button->IsDown()&&selectorUsesCtrl(options_)){*owned=true;return true;}
        if(((keyboard&&code==1)||(mouse&&code==1))&&button->IsDown()) {*owned=true;cancel();notice(tr("Selection cancelled"),false);return true;}
        if(keyboard&&button->IsDown()) {
            auto* device=RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
            const auto key=selectorKeyAction(options_,code,pressed(device,29)||pressed(device,157));
            if(key.action!=SelectorKeyAction::pass) {
                *owned=true;
                switch(key.action) {
                case SelectorKeyAction::browse:cycleSelection(key.delta);break;
                case SelectorKeyAction::reorder:queueReorder(key.delta);break;
                case SelectorKeyAction::unequip:queueUnequip(selection_.index());break;
                case SelectorKeyAction::remove:queueEdit(2,selection_.index());break;
                default:break;
                }
                return true;
            }
        }
        if(selection_.canBrowse()&&mouse&&(code==8||code==9)) {
            if(button->IsPressed())cycleSelection(code==8?-1:1);

            return true;
        }
    }
    return false;
}
void Game::dispatch(RE::BSTEventSource<RE::InputEvent*>* source,RE::InputEvent** events) {

    try {
        if(REL::Module::IsVR())for(auto* event=events?*events:nullptr;event;event=event->next)feedVROverlay(event);
        {
            std::scoped_lock lock(mutex_);
            if(!allowed())cancel();
            if(!REL::Module::IsVR())if(auto* devices=RE::BSInputDeviceManager::GetSingleton();devices&&devices->IsGamepadEnabled())
                if(auto* pad=devices->GetGamepad())controller_.observe(readPadState(pad));
            selection_.tick(clockNow());selectionTime(selection_.browsing());
        }
        filterEvents(events?*events:nullptr,[&](RE::InputEvent* e){
                std::scoped_lock lock(mutex_);const bool consumed=consume(e);
                if(!consumed)if(auto* b=e->AsButtonEvent();b&&b->GetUserEvent()=="Shout") {
                    if(b->IsDown()&&voice_.active())stopVoice();
                    nativeVoiceDown_=b->IsPressed();
                }
                return consumed;
            },
            [&](RE::InputEvent** filtered){originalDispatch(source,filtered);});
        {
            std::scoped_lock lock(mutex_);

            if(auto* keyboard=RE::BSInputDeviceManager::GetSingleton()->GetKeyboard()) {
                if(!selectionController_&&selection_.held()&&!pressed(keyboard,options_.keys[selection_.group()]))cancel();
                for(unsigned i=0;i<keyboardOwned_.size();++i)if(!pressed(keyboard,i))keyboardOwned_[i]=false;
            }
            auto* devices=RE::BSInputDeviceManager::GetSingleton();
            auto* pad=devices?devices->GetGamepad():nullptr;
            if(REL::Module::IsVR()) {
                const auto connected=[](RE::BSTrackedControllerDevice* d){return d&&d->GetRuntimeData().trackedDeviceIndex!=vr::k_unTrackedDeviceIndexInvalid;};
                controllerConnected_=devices&&(connected(devices->GetVRControllerLeft())||connected(devices->GetVRControllerRight()));
                if(controller_.synchronize(readVRState(devices)))cancel();
                if(!controllerConnected_){if(selectionController_)cancel();controller_.reset();stickOwned_.fill(false);}
                else if(options_.controllerEnabled&&selection_.canBrowse()&&options_.vrStick<2&&allowed()) {
                    const auto axis=readVRAxis(devices,options_.vrStick==1);
                    if(const auto delta=controller_.stick(axis.x,axis.y,clockNow(),options_.controllerDeadzone))cycleSelection(delta);
                }
            } else if(!pad||!devices->IsGamepadEnabled()) {
                if(selectionController_&&(selection_.held()||released_||pending_||voice_.active()))cancel();
                if(controllerConnected_&&editor_.open){for(unsigned b=1;b<unsigned(PadButton::count);++b)editorInput_.push_back({EditorInputType::pad,b,false});for(unsigned s=0;s<2;++s)editorInput_.push_back({EditorInputType::stick,s,false,0,0});}
                if(controllerConnected_)log::info("Controller disconnected");
                controllerConnected_=false;
                controller_.reset();stickOwned_.fill(false);
            } else {
                if(!controllerConnected_)log::info("Controller connected: {}",padCodeSet()==PadCodeSet::orbis?"native PlayStation (Orbis)":"XInput");
                controllerConnected_=true;
                if(controller_.synchronize(readPadState(pad)))cancel();
            }
            publish();
        }
        if(!tickQueued_.exchange(true))SKSE::GetTaskInterface()->AddTask([this]{tickQueued_=false;tick();});
    }catch(const std::exception& e) {
        std::scoped_lock lock(mutex_);cancel();snapshot_.visible=false;
        log::error("Input processing failed: {}",e.what());

    }
}
void Game::tick() {
    std::scoped_lock lock(mutex_);
    flushSettings();
    try {
        const auto apiCommand=menuAPI().take();
        if(apiCommand==MenuAPIState::Command::close){closeEditor("menu API close request");publish();}
        else if(apiCommand==MenuAPIState::Command::open&&initialized_&&ready_&&!editor_.open&&!editorLaunch_.pending()) {
            cancel();editorLaunch_.request(clockNow());frameworkMessage_.clear();
        }
        if(ready_){watchAnimation();drainAnimation();}
        if(ready_&&use_.active()&&clockNow()>=nextUsePoll_)progressUse();
        if(editorLaunch_.pending()) {
            auto* ui=RE::UI::GetSingleton();auto* player=RE::PlayerCharacter::GetSingleton();auto* main=RE::Main::GetSingleton();
            const bool safe=initialized_&&ready_&&ui&&player&&player->Is3DLoaded()&&!player->IsDead()&&!player->IsInKillMove()&&
                ui->IsShowingMenus()&&ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)&&!ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)&&!ui->IsMenuOpen(RE::MainMenu::MENU_NAME)&&renderHasFocus();
            const auto launch=editorLaunch_.poll(clockNow(),safe,allowed()&&main&&!main->GetRuntimeData().freezeTime);
            if(launch==EditorLaunch::Result::open)openEditor();
            else if(launch==EditorLaunch::Result::cancel){frameworkMessage_=tr("Editor opening cancelled. Return to gameplay and try again.");notice(frameworkMessage_);}
            if(launch!=EditorLaunch::Result::idle)return;
        }
        if(editor_.open) {
            if(const auto reason=editorBlockReason();!reason.empty()){closeEditor(reason);publish();return;}
            updateEditorTextInput();
            if(catalogDirty_)buildCatalog();
            if(dirty_)refreshEditor();
            return;
        }
        const bool inputAllowed=allowed();
        if(!inputAllowed) {
            cancel();
            if(!allowed(MenuPurpose::hud)){publish();return;}
        }
        auto* ui=RE::UI::GetSingleton();
        if(!ui->IsMenuOpen(menuName)&&!menuRequested_) {
            RE::UIMessageQueue::GetSingleton()->AddMessage(menuName,RE::UI_MESSAGE_TYPE::kShow,nullptr);menuRequested_=true;
        }
        if(inputAllowed) {
            if(voice_.active())progressVoice();
            selection_.tick(clockNow());selectionTime(selection_.browsing());

            if(released_&&!ui->IsMenuOpen(pauseMenuName)) {const auto action=*released_;released_.reset();equip(action.commit,action.unequip);}
        }

        if(clockNow()>=nextStatus_) {
            nextStatus_=clockNow()+.25;if(options_.showEquipment||options_.inactiveGroups)dirty_=true;
            for(const auto& group:arsenal_.groups)if(std::any_of(group.entries.begin(),group.entries.end(),[](const Entry& e){return isVoice(e)||e.kind==Kind::smartPotion||isSmartEquipment(e);})){dirty_=true;break;}
        }
        if(dirty_||pending_)refresh();
        if(inputAllowed&&pending_)progressEquipment();
        publish();
    }catch(const std::exception& e){cancel();log::error("Update failed: {}",e.what());}
}
bool Game::captureMenuOpen()const {
    auto* ui=RE::UI::GetSingleton();auto* player=RE::PlayerCharacter::GetSingleton();auto* controls=RE::ControlMap::GetSingleton();
    if(!ready_||editor_.open||!ui||!player||!controls||player->IsDead()||!renderHasFocus()||frameworkCapturingInput()||controls->GetRuntimeData().textEntryCount>0)return false;
    if(ui->IsMenuOpen(RE::Console::MENU_NAME)||ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME)||ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))return false;
    return ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)||ui->IsMenuOpen(RE::MagicMenu::MENU_NAME);
}
void Game::menuCapture(std::size_t groupID,Hand hand) {
    if(!captureMenuOpen()||!groupVisible(options_,groupID))return;
    auto* ui=RE::UI::GetSingleton();
    RE::TESForm* form=nullptr;RE::InventoryEntryData* descriptor=nullptr;
    const bool magic=ui->IsMenuOpen(RE::MagicMenu::MENU_NAME);
    if(magic) {
        const auto menu=ui->GetMenu<RE::MagicMenu>();auto* list=menu?menu->GetRuntimeData().itemList:nullptr;
        if(auto* selected=list?list->GetSelectedItem():nullptr)form=selected->data.baseForm;
    } else {
        const auto menu=ui->GetMenu<RE::InventoryMenu>();auto* list=menu?menu->GetRuntimeData().itemList:nullptr;
        if(auto* selected=list?list->GetSelectedItem():nullptr){descriptor=selected->data.objDesc;if(descriptor)form=descriptor->object;}
    }
    if(!form)return;
    const auto epoch=menuCaptureEpoch_;const auto formID=form->GetFormID();const auto descriptorToken=reinterpret_cast<std::uintptr_t>(descriptor);

    SKSE::GetTaskInterface()->AddTask([this,groupID,hand,magic,formID,descriptorToken,epoch] {
        std::scoped_lock lock(mutex_);if(epoch!=menuCaptureEpoch_||!captureMenuOpen()||!groupVisible(options_,groupID))return;
        auto* ui=RE::UI::GetSingleton();auto* player=RE::PlayerCharacter::GetSingleton();
        const auto report=[&](const std::string& text){notice(text);RE::SendHUDMessage::ShowHUDMessage(text.c_str());};
        try {
            RE::TESForm* selectedForm=nullptr;RE::InventoryEntryData* data=nullptr;
            if(magic) {
                const auto menu=ui->GetMenu<RE::MagicMenu>();auto* list=menu?menu->GetRuntimeData().itemList:nullptr;
                if(auto* row=list?list->GetSelectedItem():nullptr)selectedForm=row->data.baseForm;
            } else {
                const auto menu=ui->GetMenu<RE::InventoryMenu>();auto* list=menu?menu->GetRuntimeData().itemList:nullptr;
                if(auto* row=list?list->GetSelectedItem():nullptr){data=row->data.objDesc;if(data)selectedForm=data->object;}
            }
            if(!selectedForm||selectedForm->GetFormID()!=formID||(!magic&&reinterpret_cast<std::uintptr_t>(data)!=descriptorToken)){report("Selection moved — choose the item again");return;}
            Entry e;e.form=formID;e.hand=hand;
            if(!identify(selectedForm,e)){report("This item cannot be added to Quickslot");return;}
            e.name=gameName(selectedForm->GetName());
            if(arsenal_.groups[groupID].entries.size()>=Group::limit){report("This group is full (16 shortcuts)");return;}
            if(tracksInstance(e)) {
                auto* object=selectedForm->As<RE::TESBoundObject>();const auto sample=sampleInventory(player,object);
                std::vector<std::uintptr_t> row;
                if(data&&data->extraLists)for(auto* extra:*data->extraLists)if(extra)row.push_back(reinterpret_cast<std::uintptr_t>(extra));
                const auto choice=matchMenuCopy(sample,row);
                if(!choice.valid){log::warn("Menu capture: ambiguous or stale row for {:08X}",formID);report("Choose this item in Quickslot");return;}
                auto* exact=choice.plain?materializePlainCopy(player,object):reinterpret_cast<RE::ExtraDataList*>(choice.token);
                if(!ensureIdentity(player,exact,arsenal_,nullptr)){log::warn("Menu capture: identity unavailable for {:08X}",formID);report("Could not add item · try the editor");return;}
                const auto* id=exact->GetByType<RE::ExtraUniqueID>();e.owner=id->baseID;e.unique=id->uniqueID;
                if(const auto* name=exact->GetDisplayName(object);name&&*name)e.name=gameName(name);
                auto inventory=player->GetInventory([&](RE::TESBoundObject& obj){return &obj==object;});
                if(resolve(e,player,inventory).extra!=exact){log::warn("Menu capture: ambiguous identity for {:08X}",formID);report("Could not distinguish this item");return;}
            }else {
                auto inventory=player->GetInventory([&](RE::TESBoundObject& obj){return obj.GetFormID()==formID;});
                if(!resolve(e,player,inventory).available()){report("This item or ability is not available");return;}
            }
            cancel();arsenal_.active=groupID;const bool added=arsenal_.groups[groupID].add(e,options_.autoSort[groupID]);dirty_=true;
            report((added?"Added to ":"Already in ")+options_.names[groupID]+" — "+e.name+" · "+handName(e));
            if(!magic)if(const auto menu=ui->GetMenu<RE::InventoryMenu>())menu->RefreshItemList();
        }catch(const std::exception& error){log::error("Menu capture failed: {}",error.what());report("Could not add this item — try the Quickslot editor");}
    });
}
void Game::watchAnimation() {
    auto* player=RE::PlayerCharacter::GetSingleton();if(!player||!player->Is3DLoaded())return;
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if(!player->GetAnimationGraphManager(manager)||!manager||manager->graphs.empty())return;
    const auto graph=manager->graphs.front();if(!graph||graph==animationGraph_)return;
    stopAnimationWatch();animationGraph_=graph;
    animationGraph_->GetEventSource<RE::BSAnimationGraphEvent>()->AddEventSink(this);
}
void Game::stopAnimationWatch() {
    if(animationGraph_){animationGraph_->GetEventSource<RE::BSAnimationGraphEvent>()->RemoveEventSink(this);animationGraph_.reset();}
    std::scoped_lock lock(animationMutex_);animationSignals_.clear();
}
void Game::drainAnimation() {
    std::vector<AnimationSignal> signals;
    {std::scoped_lock lock(animationMutex_);signals.swap(animationSignals_);}
    for(const auto& signal:signals){const bool before=use_.animating();use_.animation(signal.time,signal.tag,signal.payload);if(before!=use_.animating())log::info("Potion animation {}: {}",use_.animating()?"started":"finished",signal.tag);}
}
RE::BSEventNotifyControl Game::ProcessEvent(const RE::BSAnimationGraphEvent* event,RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {
    if(!event||!event->holder||event->holder->GetFormID()!=0x14)return RE::BSEventNotifyControl::kContinue;
    const std::string_view tag=event->tag.c_str(),payload=event->payload.c_str();
    if((tag=="AnimObjLoad"&&payload=="AnimObjectDrinkPotion")||tag=="DrinkPotionStart"||tag=="DrinkPotion_Slow"||tag=="DrinkPotion_NoMove"||tag=="DrinkPotionStop"||tag=="EnableBumper"||tag=="MTState") {

        std::scoped_lock lock(animationMutex_);if(animationSignals_.size()<128)animationSignals_.push_back({clockNow(),std::string(tag),std::string(payload)});
    }
    return RE::BSEventNotifyControl::kContinue;
}
void Game::capture(Hand hand) {
    auto* player=RE::PlayerCharacter::GetSingleton();auto* form=player->GetEquippedObject(hand==Hand::left);
    if(!form){notice(hand==Hand::left?tr("Left hand is empty — equip a one-handed weapon or spell there first"):tr("Equip a weapon or spell first"));return;}
    Entry entry;entry.hand=hand;entry.form=form->GetFormID();
    if(!identify(form,entry)){notice(tr("This equipped item is not supported"));return;}
    entry.name=gameName(form->GetName());
    if(tracksInstance(entry)) {
        auto* object=form->As<RE::TESBoundObject>();
        auto inventory=player->GetInventory([&](RE::TESBoundObject& item){return &item==object;});
        const auto it=inventory.find(object);
        if(it==inventory.end()||it->second.first<=0||!it->second.second||!it->second.second->extraLists){notice(tr("Could not identify the equipped item"));return;}
        RE::ExtraDataList* target=nullptr;
        for(auto* extra:*it->second.second->extraLists)if(worn(extra,entry)) {
            if(target&&target!=extra){log::warn("Capture: multiple worn copies for {:08X}",entry.form);notice(tr("Equip one item, then try again"));return;}target=extra;
        }
        if(!target){notice(tr("Could not identify the equipped copy"));return;}
        auto* id=target->GetByType<RE::ExtraUniqueID>();
        if(!id) {
            auto* changes=player->GetInventoryChanges();if(!changes){notice(tr("Inventory is not ready"));return;}
            const auto owner=player->GetFormID();
            UsedItemIDs used;
            if(changes->entryList)for(auto* inventoryEntry:*changes->entryList) {
                if(!inventoryEntry||!inventoryEntry->extraLists)continue;
                for(auto* extra:*inventoryEntry->extraLists)if(extra) {
                    const auto* other=extra->GetByType<RE::ExtraUniqueID>();
                    if(other&&other->baseID==owner)used.set(other->uniqueID);
                }
            }
            eachSavedEntry(arsenal_,[&](const auto& saved){eachLeaf(saved,[&](const Entry& leaf){
                if(tracksInstance(leaf)&&!leaf.unresolved&&leaf.owner==owner)used.set(leaf.unique);});});
            const auto next=allocateItemID(used,changes->GetNextUniqueID());
            if(!next) {
                log::error("Capture: no free instance ID for {:08X} ({})",entry.form,entry.name);
                notice(tr("Could not add item"));return;
            }

            target->Add(new RE::ExtraUniqueID(owner,*next));
            changes->changed=true;
            player->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
            id=target->GetByType<RE::ExtraUniqueID>();
            if(!id||id->baseID!=owner||id->uniqueID!=*next) {
                log::error("Capture: ExtraUniqueID attachment failed for {:08X} ({}), expected {:08X}:{:04X}, present {}",
                           entry.form,entry.name,owner,*next,id!=nullptr);
                notice(tr("Could not add item"));return;
            }
            log::info("Capture: assigned instance {:08X}:{:04X} to {:08X} ({})",id->baseID,id->uniqueID,entry.form,entry.name);
        }

        entry.owner=id->baseID;entry.unique=id->uniqueID;
        if(resolve(entry,player,inventory).extra!=target) {
            log::error("Capture: ambiguous instance {:08X}:{:04X} for {:08X} ({})",entry.owner,entry.unique,entry.form,entry.name);
            notice(tr("Could not distinguish this item"));return;
        }
        log::info("Capture: verified instance {:08X}:{:04X}, form {:08X}, hand {}",entry.owner,entry.unique,entry.form,unsigned(entry.hand));
        if(const auto* name=target->GetDisplayName(object);name&&*name)entry.name=gameName(name);
    }
    entry.name=cleanName(entry.name);
    auto& group=arsenal_.current();
    const bool duplicate=std::any_of(group.entries.begin(),group.entries.end(),[&](const Entry& e){return e.same(entry);});
    if(!duplicate&&group.entries.size()>=Group::limit){notice(tr("This group is full (16 items); Alt + group key removes one"));return;}
    const auto label=entryLabel(entry);group.add(std::move(entry),options_.autoSort[arsenal_.active]);dirty_=true;
    notice((duplicate?tr("Already in "):tr("Added to "))+options_.names[arsenal_.active]+" — "+label);
}
void Game::refresh() {
    auto* player=RE::PlayerCharacter::GetSingleton();if(!player)return;
    const auto modes=options_.groupModes;normalizeGroupModes(options_,arsenal_);
    if(modes!=options_.groupModes)autosave_.change(clockNow());
    auto inventory=inventoryFor(arsenal_,player);const auto potions=potionCandidates(inventory);const auto deficits=actorDeficits(player);snapshot_.rows.clear();snapshot_.equipment=readEquipment(player,inventory);
    const auto consumableGroup=selectedConsumableGroup(arsenal_);
    const bool smart=std::any_of(arsenal_.groups.begin(),arsenal_.groups.end(),[](const auto& g){return std::any_of(g.entries.begin(),g.entries.end(),hasSmartEquipment);});
    const auto equipment=smart?equipmentCandidates(player,inventory):std::vector<NativeEquipment>{};
    for(std::size_t i=0;i<Arsenal::count;++i) {
        auto& group=arsenal_.groups[i];auto& tile=snapshot_.groups[i];tile={};tile.count=group.entries.size();
        for(std::size_t j=0;j<group.entries.size();++j) {
            auto& entry=group.entries[j];const auto resolved=resolveShortcut(entry,player,inventory,potions,deficits,&equipment);const auto& item=resolved.item;
            entry.icon=resolved.row.entry.icon;if(entry.kind==Kind::combo)entry.parts=resolved.row.entry.parts;
            const bool available=resolved.row.available;
            if(!isSmartEquipment(entry)&&item.extra)if(const char* name=item.extra->GetDisplayName(item.object);name&&*name)entry.name=gameName(name);
            if(i==arsenal_.active)snapshot_.rows.push_back(resolved.row);
            if(j==group.selected){
                tile.entry=resolved.row.entry;tile.available=available;tile.equipped=resolved.row.equipped;tile.quantity=resolved.row.quantity;tile.cooldown=resolved.row.cooldown;
                tile.voice=isVoice(entry)&&tile.equipped;
                tile.consumable=i==consumableGroup;
                if(tile.consumable){snapshot_.equipment.consumable=resolved.row;if(resolved.item.available())snapshot_.equipment.consumable->entry=resolved.action;}
            }
        }
    }
    dirty_=false;
}
void Game::startVoice(const Entry& entry) {
    if(!allowed()||voice_.active()||nativeVoiceDown_){notice(tr("Voice action unavailable"));return;}
    auto* player=RE::PlayerCharacter::GetSingleton();auto* map=RE::ControlMap::GetSingleton();auto* events=RE::UserEvents::GetSingleton();
    auto* form=RE::TESForm::LookupByID(entry.form);
    if(!player||!form||!map||!events||player->GetActorRuntimeData().selectedPower!=form||!map->IsFightingControlsEnabled())return;
    bool shouting=false;player->GetGraphVariableBool("IsShouting",shouting);
    if(shouting||player->IsAttacking()||player->IsStaggering()){notice(tr("Finish your current action, then try again"));return;}
    if(entry.kind==Kind::shout&&player->GetVoiceRecoveryTime()>0){notice(tr("Shout recharging"));return;}
    if(entry.kind==Kind::power&&player->IsInCastPowerList(form->As<RE::SpellItem>())){notice(tr("Power recharging"));return;}
    auto* devices=RE::BSInputDeviceManager::GetSingleton();
    voiceDevice_=devices&&devices->IsGamepadEnabled()?RE::INPUT_DEVICE::kGamepad:RE::INPUT_DEVICE::kKeyboard;
    voiceCode_=map->GetMappedKey(events->shout.c_str(),voiceDevice_);
    if(voiceCode_==0xff||voiceCode_==0xffffffff){notice(tr("Assign a shout control in Skyrim first"));return;}
    voiceForm_=entry.form;voice_.begin(clockNow(),entry.kind==Kind::shout?options_.shoutCharge:.05f);
    log::info("Native voice press: {:08X}, kind {}, charge {:.2f}s",entry.form,unsigned(entry.kind),entry.kind==Kind::shout?options_.shoutCharge:.05f);
    if(!voiceButton(voiceDevice_,voiceCode_,1,0)){voice_.cancel();voiceForm_=0;notice(tr("Voice action unavailable"));return;}
    notice(tr("Activating — ")+entry.name,false);dirty_=true;
}
void Game::stopVoice() {
    if(!voice_.cancel())return;
    const auto id=voiceForm_;voiceForm_=0;
    auto* player=RE::PlayerCharacter::GetSingleton();auto* form=RE::TESForm::LookupByID(id);
    if(player&&form)if(auto* caster=player->GetMagicCaster(RE::MagicSystem::CastingSource::kOther)) {
        bool ours=caster->currentSpell==form;
        if(auto* shout=form->As<RE::TESShout>())for(const auto& variation:shout->variations)ours|=variation.spell&&variation.spell==caster->currentSpell;
        if(ours&&caster->state!=RE::MagicCaster::State::kNone)caster->InterruptCast(false);
    }

    log::info("Native voice cancelled: {:08X}",id);dirty_=true;
}
void Game::progressVoice() {
    auto* player=RE::PlayerCharacter::GetSingleton();auto* selected=player?player->GetActorRuntimeData().selectedPower:nullptr;
    if(!allowed()||!selected||selected->GetFormID()!=voiceForm_||nativeVoiceDown_||player->IsStaggering()){stopVoice();return;}
    const auto now=clockNow();const float elapsed=std::max(.001f,voice_.elapsed(now));
    const auto step=voice_.tick(now);if(step==VoiceAction::Step::idle)return;
    const bool released=step==VoiceAction::Step::release;
    const bool accepted=voiceButton(voiceDevice_,voiceCode_,released?0.f:1.f,elapsed);
    if(released){log::info("Native voice release: {:08X}, {:.3f}s, handler accepted {}",voiceForm_,elapsed,accepted);if(!accepted)stopVoice();else {voice_.cancel();voiceForm_=0;}dirty_=true;}
    else if(!accepted){stopVoice();notice(tr("Voice action interrupted"));}
}
void Game::equip(const Selection::Commit& commit,bool unequip) {
    std::scoped_lock lock(mutex_);
    if(!accepts(commit)||!allowed())return;
    try {
        auto* player=RE::PlayerCharacter::GetSingleton();
        const auto saved=arsenal_.current().entries[commit.index];
        const auto action=selectionAction(saved,commit,options_);
        if(!unequip&&action==SelectionAction::none)return;
        if(!unequip&&action==SelectionAction::select) {
            arsenal_.current().selected=commit.index;rememberConsumable(arsenal_,commit.group,commit.index);pending_.reset();notice(tr("Selected — ")+saved.name,false);dirty_=true;refresh();publish();return;
        }
        if(player->IsAttacking()||player->IsStaggering()){notice(tr("Finish your current action, then try again"));return;}
        drainAnimation();
        if(use_.animating()){snapshot_.notice=tr("Drinking…");snapshot_.noticeUntil=clockNow()+1;return;}
        if(equipmentSequence(saved)) {
            equipPairStep(commit,0,unequip);return;
        }
        auto inventory=inventoryFor(arsenal_,player);const auto deficits=actorDeficits(player);
        auto resolved=resolveShortcut(saved,player,inventory,potionCandidates(inventory),deficits);
        auto entry=resolved.action;auto item=resolved.item;
        if(saved.kind==Kind::smartWeapon&&item.available()) {
            auto* extra=item.extra?item.extra:materializePlainCopy(player,item.object);
            if(!extra||!ensureIdentity(player,extra,arsenal_)){notice(tr("Could not distinguish this item"));return;}
            const auto* id=extra->GetByType<RE::ExtraUniqueID>();entry.owner=id->baseID;entry.unique=id->uniqueID;
            inventory=inventoryFor(arsenal_,player);item=resolve(entry,player,inventory);
            if(item.extra!=extra){notice(tr("Could not distinguish this item"));return;}
        }
        const bool castVoice=!unequip&&activatesVoice(entry,commit,options_);
        if(isHandAction(entry))unequip=false;
        if(unequip&&isConsumable(entry)){notice(tr("Consumable selected"),false);return;}
        if(!resolved.row.available){notice(resolved.row.detail.empty()?(isStack(entry)?tr("Out of stock — "):tr("Unavailable — "))+entryLabel(entry):resolved.row.detail);dirty_=true;return;}
        if(castVoice&&(item.cooldown>0||item.powerUsed)){notice(item.cooldown>0?tr("Shout recharging"):tr("Power recharging"));dirty_=true;return;}
        if(saved.kind==Kind::smartPotion&&!saved.effect&&saved.policy==2&&deficits[saved.resource]<.01f){notice(tr("Already full — ")+saved.name);return;}
        if(isConsumable(entry)) {
            auto* manager=RE::ActorEquipManager::GetSingleton();
            if(!manager)throw std::runtime_error("Equipment system is not ready");
            if(!use_.begin(clockNow(),item.quantity,entry.kind==Kind::poison,entry.kind==Kind::potion&&GetModuleHandleW(L"UAPNG.dll"))){snapshot_.notice=use_.animating()?tr("Drinking…"):tr("Waiting for consumable use…");snapshot_.noticeUntil=clockNow()+1;return;}
            if(saved.kind==Kind::smartPotion)log::info("Smart choice: {} -> {} ({:08X}), missing {:.1f}",saved.name,entry.name,entry.form,deficits[saved.resource]);
            usedEntry_=entry;nextUsePoll_=clockNow()+.1;pending_.reset();arsenal_.current().selected=commit.index;rememberConsumable(arsenal_,commit.group,commit.index);

            notice((entry.kind==Kind::poison?tr("Confirm poisoning — "):tr("Using — "))+entry.name,entry.kind==Kind::poison);
            log::info("Native use requested: {:08X}, kind {}, count {}",entry.form,unsigned(entry.kind),item.quantity);
            manager->EquipObject(player,item.object,nullptr,1,nullptr,false,false,true,false);
            dirty_=true;refresh();publish();return;
        }
        if(unequip&&!item.equipped){notice(tr("Already unequipped — ")+entryLabel(entry),false);return;}
        if(!unequip&&item.equipped){arsenal_.current().selected=commit.index;if(castVoice)startVoice(entry);else notice(isHandAction(entry)?handName(entry)+tr(" already empty"):tr("Ready — ")+entryLabel(entry),false);dirty_=true;refresh();publish();return;}
        const bool move=!unequip&&item.otherEquipped;

        changeEquipment(player,entry,item,unequip||move,move);
        if(!accepts(commit))return;
        pending_=Pending{commit,clockNow()+1.25,move?EquipmentPhase::movingOut:unequip?EquipmentPhase::unequipping:EquipmentPhase::equipping,-1,false,castVoice,entry};
        dirty_=true;notice(isHandAction(entry)?tr("Clearing ")+handName(entry):(move?tr("Moving to "):unequip?tr("Unequipping — "):isVoice(entry)?tr("Selecting — "):tr("Equipping — "))+entryLabel(entry),false);refresh();publish();
    }catch(const std::exception& e){pending_.reset();log::error("Equipment action failed: {}",e.what());notice(tr("Could not change equipment"));}
}
void Game::equipPairStep(const Selection::Commit& commit,int step,bool remove,std::optional<Entry> plan) {
    if(!accepts(commit)||!allowed())return;
    const auto combo=arsenal_.current().entries[commit.index];
    if(!equipmentSequence(combo)||(combo.kind==Kind::combo&&combo.parts.size()!=2))return;
    if(combo.kind==Kind::unarmed)remove=false;
    auto* player=RE::PlayerCharacter::GetSingleton();
    if(player->IsAttacking()||player->IsStaggering()||use_.animating()){pending_.reset();notice(tr("Combination stopped — finish your current action"));return;}
    auto inventory=inventoryFor(arsenal_,player);
    if(!plan) {
        const auto chosen=resolveShortcut(combo,player,inventory,{},{});
        plan=chosen.action;
        if(hasSmartEquipment(combo)) {
            if(!chosen.row.available){pending_.reset();notice(tr("Combination stopped — ")+chosen.row.detail);return;}

            for(unsigned hand=0;hand<2;++hand)if(isSmartEquipment(combo.parts[hand])) {
                auto& part=plan->parts[hand];const auto& item=chosen.parts[hand];
                if(remove&&!item.equipped)continue;
                auto* extra=item.extra?item.extra:materializePlainCopy(player,item.object);
                if(!extra||!ensureIdentity(player,extra,arsenal_)){pending_.reset();notice(tr("Could not distinguish this item"));return;}
                const auto* id=extra->GetByType<RE::ExtraUniqueID>();part.owner=id->baseID;part.unique=id->uniqueID;
                inventory=inventoryFor(arsenal_,player);
                if(resolve(part,player,inventory).extra!=extra){pending_.reset();notice(tr("Could not distinguish this item"));return;}
                log::info("Smart combination choice: {} -> {} ({:08X}), hand {}, copy {}",combo.parts[hand].name,part.name,part.form,hand,part.unique);
            }
            if(!remove&&!validPair(plan->parts[0],plan->parts[1])){pending_.reset();notice(tr("Choose compatible items for both hands"));return;}
        }
    }
    if(!accepts(commit)||!allowed())return;
    if(!remove) {
        const auto state=resolveShortcut(*plan,player,inventory,{},{}).row;
        if(!state.available){pending_.reset();notice(tr("Combination stopped — ")+state.detail);return;}
    }
    if(step>=2) {
        if(!remove)arsenal_.current().selected=commit.index;
        pending_.reset();dirty_=true;refresh();
        const auto state=resolveShortcut(*plan,player,inventory,{},{}).row;
        if(combo.kind==Kind::unarmed)log::info("Unarmed complete: both hands empty {}",state.equipped);
        notice(combo.kind==Kind::unarmed?(state.equipped?tr("Unarmed"):tr("Hands not cleared")):remove?tr("Unequipped combination — ")+combo.name:state.equipped?tr("Combination active — ")+combo.name:tr("Combination incomplete — check both hands"),!remove&&!state.equipped);publish();return;
    }
    const auto part=equipmentStep(*plan,std::size_t(step));const auto item=resolve(part,player,inventory);

    if(skipEquipmentStep(part,remove,item.equipped)){equipPairStep(commit,step+1,remove,plan);return;}
    if(!item.available()){pending_.reset();notice(tr("Combination stopped — ")+part.name+tr(" is unavailable"));return;}
    const bool move=!remove&&item.otherEquipped;
    changeEquipment(player,part,item,remove||move,move);
    if(!accepts(commit))return;
    pending_=Pending{commit,clockNow()+1.25,move?EquipmentPhase::movingOut:remove?EquipmentPhase::unequipping:EquipmentPhase::equipping,step,remove,false,plan};
    dirty_=true;notice(combo.kind==Kind::unarmed?tr("Clearing both hands"):(remove?tr("Unequipping combination — "):tr("Equipping combination — "))+combo.name,false);refresh();publish();
}
void Game::progressEquipment() {
    const auto pending=*pending_;const auto& commit=pending.commit;
    if(!accepts(commit)){pending_.reset();return;}
    auto* player=RE::PlayerCharacter::GetSingleton();
    auto inventory=inventoryFor(arsenal_,player);const auto saved=arsenal_.current().entries[commit.index];
    const auto entry=pending.pairStep>=0?equipmentStep(pending.action.value_or(saved),std::size_t(pending.pairStep)):pending.action.value_or(saved);
    const auto item=resolve(entry,player,inventory);
    switch(equipmentProgress(pending.phase,true,item.available(),item.equipped,item.otherEquipped,clockNow()>=pending.deadline)) {
    case EquipmentProgress::complete:
        if(isHandAction(entry))log::info("Clear hand confirmed: {}",unsigned(entry.hand));
        if(pending.pairStep>=0){pending_.reset();equipPairStep(commit,pending.pairStep+1,pending.remove,pending.action);break;}
        if(pending.phase!=EquipmentPhase::unequipping)arsenal_.current().selected=commit.index;
        notice(isHandAction(entry)?handName(entry)+tr(" empty"):(pending.phase==EquipmentPhase::unequipping?tr("Unequipped — "):isVoice(entry)?tr("Selected — "):tr("Equipped — "))+entryLabel(entry),false);
        pending_.reset();if(pending.castVoice)startVoice(entry);dirty_=true;refresh();break;
    case EquipmentProgress::equipAfterMove:
        if(player->IsAttacking()||player->IsStaggering()){pending_.reset();notice(tr("Hand cleared; finish your action before equipping"));break;}
        changeEquipment(player,entry,item,false);

        if(accepts(commit))pending_=Pending{commit,clockNow()+1.25,EquipmentPhase::equipping,pending.pairStep,pending.remove,pending.castVoice,pending.action};
        else pending_.reset();
        dirty_=true;refresh();break;
    case EquipmentProgress::unavailable:pending_.reset();notice(tr("Unavailable — ")+entryLabel(entry));break;
    case EquipmentProgress::timeout:
        log::warn("Equipment timeout: phase {}, form {:08X}, hand {}, target equipped {}, other equipped {}",
                  unsigned(pending.phase),entry.form,unsigned(entry.hand),item.equipped,item.otherEquipped);
        log::warn("Equipment state: right {:08X}, left {:08X}, worn {}, worn-left {}, exact copy {}",
            player->GetEquippedObject(false)?player->GetEquippedObject(false)->GetFormID():0,
            player->GetEquippedObject(true)?player->GetEquippedObject(true)->GetFormID():0,
            item.extra&&item.extra->HasType<RE::ExtraWorn>(),item.extra&&item.extra->HasType<RE::ExtraWornLeft>(),item.extra!=nullptr);
        pending_.reset();notice(tr("Equipment change did not complete — try again when ready"));break;
    case EquipmentProgress::cancelled:pending_.reset();break;
    case EquipmentProgress::wait:break;
    }
}
void Game::progressUse() {
    auto* player=RE::PlayerCharacter::GetSingleton();auto* ui=RE::UI::GetSingleton();
    if(!player||!ui||!player->Is3DLoaded()||ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))return;
    nextUsePoll_=clockNow()+.1;
    int count=0;

    if(auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(usedEntry_.form)) {
        auto inventory=player->GetInventory([&](RE::TESBoundObject& candidate){return &candidate==object;});
        if(const auto it=inventory.find(object);it!=inventory.end())count=std::max(0,it->second.first);
    }
    if(player->IsDead()){use_.reset();return;}
    const auto result=use_.observe(clockNow(),count,ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME),RE::BSTimer::QGlobalTimeMultiplier(),ui->GameIsPaused());
    if(result==ConsumableUse::Result::decreased) {
        notice(usedEntry_.name+" ×"+std::to_string(count),false);dirty_=true;
        log::info("Native use inventory decrease observed: {:08X}, remaining {}",usedEntry_.form,count);
    } else if(result==ConsumableUse::Result::cancelled)notice(tr("Poison not applied — ")+usedEntry_.name);
    else if(result==ConsumableUse::Result::unconfirmed) {
        notice(tr("Use not confirmed — try again when ready"));
        log::info("Native use unconfirmed (no retry): {:08X}, remaining {}",usedEntry_.form,count);
    }
}
void Game::draw(bool editing) {
    Snapshot snapshot;Options options;EditorState editor;std::vector<EditorInput> input;
    {std::scoped_lock lock(mutex_);if(!initialized_||editing!=editor_.open)return;snapshot=snapshot_;options=options_;
        if(editing){editor=editor_;input.swap(editorInput_);}}
    if(snapshot.visible||editing||REL::Module::IsVR()) {
        auto frame=renderHud(snapshot,options,editing?&editor:nullptr,input);
        for(auto& command:frame.commands)SKSE::GetTaskInterface()->AddTask([this,c=std::move(command)]{edit(c);});
        if(editing) {
            std::scoped_lock lock(mutex_);
            if(editorTextInput_.focus(editor.session,frame.wantsTextInput)&&!tickQueued_.exchange(true))
                SKSE::GetTaskInterface()->AddTask([this]{tickQueued_=false;tick();});
        }
    }
}
void Game::drawVR() {
    bool editing=false;{std::scoped_lock lock(mutex_);editing=editor_.open;}
    draw(editing);
}
void Game::rendererFailed(){SKSE::GetTaskInterface()->AddTask([this]{std::scoped_lock lock(mutex_);cancel();closeEditor("renderer failure");snapshot_.visible=false;});}
std::string Game::editorBlockReason()const {
    auto* ui=RE::UI::GetSingleton();auto* player=RE::PlayerCharacter::GetSingleton();
    if(!initialized_||!ready_||!ui||!player||!player->Is3DLoaded())return "game not ready";
    if(player->IsDead())return "player died";
    if(player->IsInKillMove())return "kill move started";
    if(!ui->IsShowingMenus()||!ui->IsMenuOpen(RE::HUDMenu::MENU_NAME))return "HUD hidden";
    MenuBlocker blocker;
    if(foreignMenu(ui,MenuPurpose::editor,&blocker))return fmt::format("foreign menu '{}' (flags 0x{:08X})",blocker.name,blocker.flags);
    if(frameworkCapturingInput())return "SKSE Menu Framework captured input";
    if(!renderHasFocus())return "game window lost focus";
    return {};
}
FrameworkStatus Game::frameworkStatus() {

    std::scoped_lock lock(mutex_);
    return {initialized_&&ready_,editor_.open,editorLaunch_.pending()||menuAPI().pending(),options_.groupCount,arsenal_.size(),options_.editorKey,frameworkMessage_};
}
bool Game::requestEditorFromFramework() {
    return menuAPI().requestOpen();
}
void Game::requestVREditor(bool open) {
    SKSE::GetTaskInterface()->AddTask([this,open]{
        std::scoped_lock lock(mutex_);
        if(open==vrEditorWanted()){if(open){if(!editor_.open)openEditor();}else closeEditor("VR overlay close request");}
        vrEditorResult(editor_.open);
    });
}
void Game::openEditor() {
    std::scoped_lock lock(mutex_);editorRequested_=false;
    if(!allowed()||RE::UI::GetSingleton()->IsMenuOpen(editorMenuName))return;
    try {
        log::info("Editor opening: building equipment catalog");
        cancel();groupRecall_.reset();const auto session=editor_.session+1;editor_=EditorState{};editor_.session=session;editor_.open=true;
        editorTextInput_.beginSession(session);
        editor_.vr=REL::Module::IsVR();editor_.controller=controllerLast_&&!editor_.vr;editor_.draft=arsenal_;editor_.options=options_;editor_.soulsSlowdown=GetModuleHandleW(L"SkyrimSoulsRE.dll")!=nullptr;revealSavedGroups(editor_.options,editor_.draft);
        collectShortcuts(editor_.draft);
        editHistory_.begin(editor_.draft,editor_.options);
        editor_.settingsPending=autosave_.pending();editor_.saveError=settingsError_;
        editorInput_.clear();buildCatalog();refreshEditor();refreshPresets();

        auto* keyboard=RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
        for(const unsigned scan:{42u,54u,29u,157u,56u,184u})
            if(pressed(keyboard,scan))editorInput_.push_back({EditorInputType::key,scan,true});
        textInputRecoveryLogged_=false;
        RE::UIMessageQueue::GetSingleton()->AddMessage(editorMenuName,RE::UI_MESSAGE_TYPE::kShow,nullptr);
        menuAPI().publish(ready_,true);log::info("Editor opened: {} choices, {} groups",editor_.catalog.size(),editor_.options.groupCount);
    }catch(const std::exception& e){closeEditor("opening failed");log::error("Editor opening failed: {}",e.what());notice(tr("Could not open editor"));}
}
void Game::closeEditor(std::string_view reason) {
    const bool wasOpen=editor_.open;
    if(wasOpen)flushSettings(true);
    editor_.open=false;menuAPI().publish(initialized_&&ready_,false);editorRequested_=false;editorLaunch_.cancel();++editor_.session;editorInput_.clear();candidates_.clear();
    updateEditorTextInput();
    if(wasOpen) {
        if(auto* queue=RE::UIMessageQueue::GetSingleton())queue->AddMessage(editorMenuName,RE::UI_MESSAGE_TYPE::kHide,nullptr);
        cancel();dirty_=true;if(!settingsError_.empty())notice(tr("Settings could not be saved"));log::info("Editor closed; live edits retained; reason: {}",reason);
    }
}
void Game::updateEditorTextInput() {
    auto* controls=RE::ControlMap::GetSingleton();auto* ui=RE::UI::GetSingleton();
    const int before=controls?controls->GetRuntimeData().textEntryCount:-1;
    const auto change=editorTextInput_.update(editor_.open,ui&&ui->IsMenuOpen(editorMenuName),before);
    if(!controls||change==EditorTextInput::Change::none)return;
    controls->AllowTextInput(change!=EditorTextInput::Change::release);
    const int after=controls->GetRuntimeData().textEntryCount;
    if(change==EditorTextInput::Change::acquire)log::info("Editor text input acquired for focused field ({} -> {})",before,after);
    else if(change==EditorTextInput::Change::release)log::info("Editor text input released ({} -> {})",before,after);
    else if(change==EditorTextInput::Change::restore&&!textInputRecoveryLogged_) {
        textInputRecoveryLogged_=true;
        log::warn("Editor text-entry count cleared externally; restored editor claim ({} -> {})",before,after);
    }
}
void Game::saveSettings(const Options& options) {
    const auto path=dataDirectory()/"SKSE/Plugins/Quickslot.settings.json";
    const auto temporary=path.wstring()+L".tmp";
    {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);const auto bytes=encodeOptions(options);file<<bytes<<'\n';file.flush();if(!file)throw std::runtime_error("Cannot write Quickslot.settings.json");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace Quickslot.settings.json (Windows error "+std::to_string(GetLastError())+")");
}
void Game::buildCatalog() {
    auto* player=RE::PlayerCharacter::GetSingleton();editor_.catalog.clear();candidates_.clear();editor_.equippedCandidates={};editor_.equippedPair=true;++editor_.catalogVersion;
    auto inventory=player->GetInventory([](RE::TESBoundObject& object){Entry e;return identify(&object,e);});
    editor_.potions=potionCandidates(inventory);editor_.equipment=equipmentChoices(equipmentCandidates(player,inventory));editor_.deficits=actorDeficits(player);
    for(auto& [object,data]:inventory) {
        if(data.first<=0||!data.second)continue;
        Entry base;base.form=object->GetFormID();if(!identify(object,base))continue;
        base.name=gameName(object->GetName());int plain=data.first;
        if(isStack(base)) {
            candidates_.push_back({object,nullptr,false,base});
            editor_.catalog.push_back({base,candidates_.size(),data.first,base.kind==Kind::ammo&&player->GetCurrentAmmo()==object,true,{}});
            if(base.kind==Kind::torch&&player->GetEquippedObject(true)==object) {
                editor_.catalog.back().equipped=true;editor_.equippedCandidates[1]=candidates_.size();
            }
            if(base.kind==Kind::scroll)for(unsigned hand=0;hand<2;++hand)if(player->GetEquippedObject(hand==1)==object) {
                editor_.catalog.back().equipped=true;if(pairable(base))editor_.equippedCandidates[hand]=candidates_.size();else editor_.equippedPair=false;
            }
            continue;
        }
        const auto append=[&](RE::ExtraDataList* extra,int count,bool untagged) {
            Entry entry=base;bool equipped=false;std::string detail;
            if(extra) {
                if(const char* name=extra->GetDisplayName(object);name&&*name)entry.name=gameName(name);
                if(const auto* id=extra->GetByType<RE::ExtraUniqueID>()){entry.owner=id->baseID;entry.unique=id->uniqueID;}
                equipped=extra->HasType<RE::ExtraWorn>()||extra->HasType<RE::ExtraWornLeft>();

                if(const auto* health=extra->GetByType<RE::ExtraHealth>();health&&health->health>1)detail=tr("Tempered");
                if(extra->HasType<RE::ExtraEnchantment>())detail+=detail.empty()?tr("Enchanted"):tr(" · Enchanted");
            }
            const bool addable=untagged||count==1;
            if(!addable)detail=tr("Equip one first");
            candidates_.push_back({object,extra,untagged,entry});
            editor_.catalog.push_back({entry,candidates_.size(),count,equipped,addable,detail});
            if(equipped&&extra) {
                if(!pairable(entry))editor_.equippedPair=false;
                else {if(extra->HasType<RE::ExtraWorn>())editor_.equippedCandidates[entry.kind==Kind::shield||entry.kind==Kind::torch?1:0]=candidates_.size();if(extra->HasType<RE::ExtraWornLeft>())editor_.equippedCandidates[1]=candidates_.size();}
            }
        };
        if(data.second->extraLists)for(auto* extra:*data.second->extraLists)if(extra) {
            const auto count=extra->GetCount();plain-=count;if(count>0)append(extra,count,false);
        }
        if(plain>0)append(nullptr,plain,true);
    }
    std::unordered_set<RE::FormID> seen;
    const auto spell=[&](RE::SpellItem* form) {
        if(!form||!seen.insert(form->GetFormID()).second||!player->HasSpell(form))return;
        Entry e;e.form=form->GetFormID();if(!identify(form,e))return;e.name=gameName(form->GetName()?form->GetName():"");
        const bool equipped=isVoice(e)?player->GetActorRuntimeData().selectedPower==form:player->GetEquippedObject(true)==form||player->GetEquippedObject(false)==form;
        candidates_.push_back({nullptr,nullptr,false,e});
        editor_.catalog.push_back({e,candidates_.size(),1,equipped,true,{}});
        if(!isVoice(e)&&equipped) {
            if(!pairable(e))editor_.equippedPair=false;
            else for(unsigned hand=0;hand<2;++hand)if(player->GetEquippedObject(hand==1)==form)editor_.equippedCandidates[hand]=candidates_.size();
        }
    };
    const auto list=[&](RE::TESSpellList::SpellData* data){if(data&&data->spells)for(unsigned i=0;i<data->numSpells;++i)spell(data->spells[i]);};
    if(auto* base=player->GetActorBase())list(base->actorEffects);
    if(auto* race=player->GetRace())list(race->actorEffects);
    for(auto* form:player->GetActorRuntimeData().addedSpells)spell(form);
    if(auto* data=RE::TESDataHandler::GetSingleton())for(auto* form:data->GetFormArray<RE::TESShout>()) {
        if(!form||!player->HasShout(form))continue;
        Entry e;e.form=form->GetFormID();if(!identify(form,e))continue;e.name=gameName(form->GetName()?form->GetName():"");
        const bool unlocked=shoutUnlocked(form);candidates_.push_back({nullptr,nullptr,false,e});
        editor_.catalog.push_back({e,candidates_.size(),1,player->GetActorRuntimeData().selectedPower==form,unlocked,
            unlocked?"":tr("Unlock a word first")});
    }
    std::stable_sort(editor_.catalog.begin(),editor_.catalog.end(),[](const auto& a,const auto& b){return a.entry.name<b.entry.name;});
    log::info("Editor catalog ready: {} choices",editor_.catalog.size());
    catalogDirty_=false;
}
void Game::refreshEditor() {
    auto* player=RE::PlayerCharacter::GetSingleton();auto inventory=inventoryFor(editor_.draft,player);const auto potions=potionCandidates(inventory);editor_.deficits=actorDeficits(player);
    const auto equipment=equipmentCandidates(player,inventory);editor_.equippedDisplay=readEquipment(player,inventory);
    const auto consumableGroup=selectedConsumableGroup(editor_.draft);
    for(std::size_t i=0;i<Arsenal::count;++i) {
        auto& rows=editor_.rows[i];rows.clear();
        for(auto& entry:editor_.draft.groups[i].entries) {
            const auto resolved=resolveShortcut(entry,player,inventory,potions,editor_.deficits,&equipment);const auto& item=resolved.item;
            entry.icon=resolved.row.entry.icon;if(entry.kind==Kind::combo)entry.parts=resolved.row.entry.parts;
            if(!isSmartEquipment(entry)&&item.extra)if(const char* name=item.extra->GetDisplayName(item.object);name&&*name)entry.name=gameName(name);
            rows.push_back(resolved.row);
            if(i==consumableGroup&&rows.size()-1==editor_.draft.groups[i].selected){editor_.equippedDisplay.consumable=resolved.row;if(resolved.item.available())editor_.equippedDisplay.consumable->entry=resolved.action;}
        }
    }
    dirty_=false;
}
std::optional<Entry> Game::captureCandidate(std::uint64_t candidate,Hand hand) {
    if(!candidate||candidate>candidates_.size()||catalogDirty_) {editor_.message=tr("Select the item again");return {};}
    const auto native=candidates_[candidate-1];auto entry=native.entry;entry.hand=normalizedHand(entry,hand);
    auto* player=RE::PlayerCharacter::GetSingleton();
    if(entry.spell) {
        auto* spell=RE::TESForm::LookupByID<RE::SpellItem>(entry.form);
        if(!spell||!player->HasSpell(spell)){editor_.message=tr("Spell unavailable");return {};}
    } else if(entry.kind==Kind::shout) {
        auto* shout=RE::TESForm::LookupByID<RE::TESShout>(entry.form);
        if(!shout||!player->HasShout(shout)||!shoutUnlocked(shout)){editor_.message=tr("Unlock a word first");return {};}
    } else if(isStack(entry)) {
        auto inventory=player->GetInventory([&](RE::TESBoundObject& object){return object.GetFormID()==entry.form;});
        if(!resolve(entry,player,inventory).available()){editor_.message=tr("Item unavailable");return {};}
    } else {
        auto sample=sampleInventory(player,native.object);RE::ExtraDataList* extra=nullptr;
        if(native.plain)extra=materializePlainCopy(player,native.object);
        else {

            for(const auto& stack:sample.stacks)if(stack.token==reinterpret_cast<std::uintptr_t>(native.extra)&&
                stack.count==1&&(!stack.hasID||stack.owner==entry.owner&&stack.unique==entry.unique)){extra=native.extra;break;}
        }
        if(!extra||!ensureIdentity(player,extra,arsenal_,&editor_.draft)){
            log::warn("Editor capture: identity unavailable for {:08X}",entry.form);
            editor_.message=tr("Equip the item, then refresh");return {};
        }
        const auto* id=extra->GetByType<RE::ExtraUniqueID>();entry.owner=id->baseID;entry.unique=id->uniqueID;
        auto inventory=player->GetInventory([&](RE::TESBoundObject& object){return &object==native.object;});
        if(resolve(entry,player,inventory).extra!=extra){
            log::warn("Editor capture: ambiguous identity {:08X}:{:04X} for {:08X}",entry.owner,entry.unique,entry.form);
            editor_.message=tr("Could not distinguish this item");return {};
        }
    }
    return entry;
}
bool Game::addCandidate(const EditCommand& c) {
    if(c.catalogVersion!=editor_.catalogVersion||catalogDirty_){editor_.message=tr("Select the item again");return false;}
    auto& group=editor_.draft.groups[c.group];
    if(group.entries.size()>=Group::limit) {
        const auto* choice=catalogItem(editor_,c.candidate);
        if(!choice){editor_.message=tr("Select the item again");return false;}
        auto entry=choice->entry;entry.hand=normalizedHand(entry,c.hand);
        if(std::none_of(group.entries.begin(),group.entries.end(),[&](const auto& saved){return saved.same(entry);})) {editor_.message=tr("Group full (16 / 16)");return false;}
    }
    const auto entry=captureCandidate(c.candidate,c.hand);if(!entry)return false;
    const bool added=group.add(*entry,editor_.options.autoSort[c.group]);editor_.draft.active=c.group;++editor_.revision;
    editor_.message=added?"":tr("Already in group");
    log::info("Editor {} to {}: {} ({:08X}:{:04X})",added?"added":"selected",editor_.options.names[c.group],entryLabel(*entry),entry->owner,entry->unique);
    buildCatalog();refreshEditor();return true;
}
bool Game::saveCombination(const EditCommand& c) {
    if(catalogDirty_){buildCatalog();editor_.message=tr("Select the items again");return false;}
    if(!c.replace&&!c.libraryID&&editor_.draft.groups[c.group].entries.size()>=Group::limit){editor_.message=tr("Group full (16 / 16)");return false;}
    const auto pair=composeCombination(editor_,c,[&](std::uint64_t id,Hand hand){return captureCandidate(id,hand);});
    if(!pair){if(editor_.message.empty())editor_.message=tr("Choose compatible equipment");buildCatalog();return false;}
    auto save=c;save.action=EditAction::saveShortcut;save.shortcut=*pair;
    if(!editStructure(editor_.draft,editor_.options,save))return false;
    ++editor_.revision;editor_.message.clear();buildCatalog();refreshEditor();return true;
}
void Game::flushSettings(bool force) {
    if(!autosave_.pending()||(!force&&!autosave_.due(clockNow())))return;
    try {saveSettings(options_);autosave_.saved();settingsError_.clear();}
    catch(const std::exception& e){if(settingsError_!=e.what())log::error("Settings autosave failed: {}",e.what());settingsError_=e.what();autosave_.failed(clockNow());}
    editor_.settingsPending=autosave_.pending();editor_.saveError=settingsError_;
}
void Game::applyEditor() {
    localization().select(editor_.options.language);
    sanitizeOptions(editor_.options);revealSavedGroups(editor_.options,editor_.draft);
    if(encodeOptions(options_)!=encodeOptions(editor_.options))autosave_.change(clockNow());
    arsenal_=editor_.draft;options_=editor_.options;cancel();dirty_=true;
    editor_.canUndo=editHistory_.canUndo();editor_.canRevert=editHistory_.canRevert();
    editor_.settingsPending=autosave_.pending();editor_.saveError=settingsError_;
    refreshEditor();publish();
}
void Game::refreshPresets() {
    try {editor_.presets=listPresets(dataDirectory()/"Quickslot/Presets");}
    catch(const std::exception& e){log::warn("Preset list: {}",e.what());editor_.message=tr("Could not read presets");}
}
bool Game::presetCommand(const EditCommand& c) {
    const auto folder=dataDirectory()/"Quickslot/Presets";
    if(c.action==EditAction::refreshPresets){refreshPresets();return false;}
    try {
        if(c.action==EditAction::exportPreset) {
            const auto path=presetPath(folder,c.presetName);
            if(std::filesystem::exists(path)&&!c.replace){editor_.message=tr("A preset with that name already exists");return false;}
            if(!c.appearanceOnly&&editor_.draft.presetIdentity.empty()){editor_.message=tr("Save and reload your game before exporting a complete setup");return false;}
            const auto bytes=exportPreset(editor_.draft,editor_.options,c.appearanceOnly,[](std::uint32_t id)->Json {
                auto* form=RE::TESForm::LookupByID(id);if(!form)return nullptr;
                if(form->IsDynamicForm())return {{"dynamic",id}};
                const auto* file=form->GetFile(0);if(!file)return nullptr;
                return {{"file",file->GetFilename()},{"local",form->GetLocalFormID()}};
            });
            std::filesystem::create_directories(folder);
            const auto temp=path.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L".tmp";

            const auto file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Could not create preset temporary file");
            DWORD written=0;const bool wrote=WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size();
            const bool flushed=FlushFileBuffers(file);CloseHandle(file);
            if(!wrote||!flushed){DeleteFileW(temp.c_str());throw std::runtime_error("Could not write preset");}
            if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH|(c.replace?MOVEFILE_REPLACE_EXISTING:0))){DeleteFileW(temp.c_str());throw std::runtime_error("Could not finish preset export");}
            refreshPresets();editor_.message=tr("Exported: ")+c.presetName;log::info("Preset exported: {} ({})",path.string(),c.appearanceOnly?"appearance":"complete");return false;
        }
        const auto bytes=readPreset(folder,c.presetName);
        auto result=importPreset(bytes,editor_.draft,editor_.options,c.appearanceOnly,[](const std::string& file,std::uint32_t local){
            auto* data=RE::TESDataHandler::GetSingleton();auto* form=data?data->LookupForm(local,file):nullptr;return form?form->GetFormID():0;
        });

        editHistory_.prepare(editor_.draft,editor_.options);
        editor_.draft=std::move(result.arsenal);editor_.options=std::move(result.options);++editor_.revision;++editor_.restoreSerial;
        editHistory_.finish(true,false,clockNow());applyEditor();
        std::size_t unavailable=0;for(const auto& group:editor_.rows)for(const auto& row:group)unavailable+=!row.available;
        editor_.message=tr("Imported: ")+c.presetName;if(!result.appearanceOnly&&unavailable)editor_.message+=" · "+std::to_string(unavailable)+tr(" unavailable slots");
        log::info("Preset imported: {} (appearance {}, unresolved {}, unavailable {})",c.presetName,result.appearanceOnly,result.unresolved,unavailable);return true;
    }catch(const std::exception& e){log::warn("Preset action failed: {}",e.what());editor_.message=tr("Could not ")+std::string(c.action==EditAction::exportPreset?tr("export"):tr("import"))+tr(" preset");return false;}
}
void Game::edit(const EditCommand& c) {
    std::scoped_lock lock(mutex_);
    if(!editor_.open||c.session!=editor_.session)return;
    if(c.action==EditAction::close&&!c.hasSettings){closeEditor();return;}
    if(const auto reason=editorBlockReason();!reason.empty()){closeEditor(reason);return;}
    if(!matchingEdit(editor_,c)){editor_.message=tr("List updated; try again");return;}
    try {
        if(c.action==EditAction::exportPreset||c.action==EditAction::importPreset||c.action==EditAction::refreshPresets){presetCommand(c);return;}
        if(c.action==EditAction::undo||c.action==EditAction::revert) {
            const bool restored=c.action==EditAction::undo?editHistory_.undo(editor_.draft,editor_.options):editHistory_.revert(editor_.draft,editor_.options);
            if(restored){++editor_.revision;++editor_.restoreSerial;editor_.message=c.action==EditAction::undo?tr("Undone"):tr("Session reverted");applyEditor();}
            return;
        }
        if(c.action==EditAction::refresh){buildCatalog();refreshEditor();editor_.message.clear();return;}
        if(!groupVisible(editor_.options,c.group))return;
        editHistory_.prepare(editor_.draft,editor_.options);bool changed=false;
        if(c.action==EditAction::settings||(c.action==EditAction::close&&c.hasSettings)) {
            auto options=c.options;sanitizeOptions(options);revealSavedGroups(options,editor_.draft);
            changed=encodeOptions(options)!=encodeOptions(editor_.options);
            sortNewlyEnabledGroups(editor_.draft,editor_.options,options);
            editor_.options=std::move(options);if(changed)++editor_.revision;
            editor_.message.clear();
        } else if(c.action==EditAction::add)changed=addCandidate(c);
        else if(c.action==EditAction::saveCombination)changed=saveCombination(c);
        else if(editStructure(editor_.draft,editor_.options,c)||editGroup(editor_.draft,editor_.options,c)){changed=true;++editor_.revision;editor_.message.clear();}
        else editor_.message=tr(c.action==EditAction::sortGroup?"Already sorted":"Could not change item");
        editHistory_.finish(changed&&c.action!=EditAction::select,c.continuous,clockNow());
        if(changed) {
            if(c.action==EditAction::addGroup||c.action==EditAction::removeGroup)++editor_.restoreSerial;
            if(c.action==EditAction::removeGroup)editor_.message=tr("Group removed");
            if(c.action==EditAction::clearGroup)editor_.message=tr("Group cleared");
            if(unsigned(c.action)>=unsigned(EditAction::addGroup))log::info("Editor structure action {}, group {}, visible {}, saved entries {}",unsigned(c.action),c.group+1,editor_.options.groupCount,editor_.draft.size());
            applyEditor();
        }
        if(c.action==EditAction::close)closeEditor();
    }catch(const std::exception& e){editHistory_.finish(false,false,clockNow());log::error("Editor action failed: {}",e.what());editor_.message=tr("Could not apply change");}
}
std::optional<TransferTicket> Game::beforeTransfer(RE::PlayerCharacter* player,RE::TESBoundObject* object,RE::TESObjectREFR* source,RE::ExtraDataList* extra) {
    std::scoped_lock lock(mutex_);if(!ready_||!player||player!=RE::PlayerCharacter::GetSingleton())return {};
    const auto* id=extra?extra->GetByType<RE::ExtraUniqueID>():nullptr;
    std::optional<Entry> tracked;bool ambiguous=false;
    const auto find=[&](const Arsenal& a){eachSavedEntry(a,[&](const auto& saved){eachLeaf(saved,[&](const Entry& e) {
        if(e.form!=object->GetFormID()||!tracksInstance(e)||e.unresolved)return;
        const bool exact=id&&id->baseID==e.owner&&id->uniqueID==e.unique;
        const bool world=source&&source->GetBaseObject()==object&&e.owner==source->GetFormID();
        if(exact||world){if(tracked&&(tracked->owner!=e.owner||tracked->unique!=e.unique))ambiguous=true;else tracked=e;}
    });});};
    find(arsenal_);if(editor_.open)find(editor_.draft);
    if(!tracked||ambiguous)return {};
    log::info("Tracked incoming copy: form {:08X}, identity {:08X}:{:04X}, source {:08X}",tracked->form,tracked->owner,tracked->unique,source?source->GetFormID():0);
    auto before=sampleInventory(player,object);
    if(std::any_of(before.stacks.begin(),before.stacks.end(),[&](const auto& stack){return stack.hasID&&stack.owner==tracked->owner&&stack.unique==tracked->unique;}))return {};
    return TransferTicket{{tracked->form,tracked->owner,0,tracked->unique,0},std::move(before)};
}
void Game::afterTransfer(RE::PlayerCharacter* player,RE::TESBoundObject* object,const TransferTicket& ticket) {
    std::scoped_lock lock(mutex_);if(!ready_)return;
    const auto after=sampleInventory(player,object);const auto match=matchIncomingCopy(ticket.before,after);
    if(!match.valid){log::warn("Pickup recovery declined: inventory delta was not one unambiguous copy ({:08X}, {} -> {})",ticket.identity.form,ticket.before.count,after.count);return;}
    auto* extra=match.plain?materializePlainCopy(player,object):reinterpret_cast<RE::ExtraDataList*>(match.token);
    if(!ensureIdentity(player,extra,arsenal_,editor_.open?&editor_.draft:nullptr)){log::warn("Pickup recovery could not attach identity");return;}
    const auto* id=extra->GetByType<RE::ExtraUniqueID>();auto change=ticket.identity;change.newOwner=id->baseID;change.newID=id->uniqueID;
    const auto verified=sampleInventory(player,object);
    if(std::count_if(verified.stacks.begin(),verified.stacks.end(),[&](const auto& stack){return stack.hasID&&stack.owner==id->baseID&&stack.unique==id->uniqueID;})!=1) {
        log::warn("Pickup recovery declined: duplicate destination identity");return;
    }
    const auto count=followIdentityChange(arsenal_,change);
    if(editor_.open){editHistory_.identity(change);if(followIdentityChange(editor_.draft,change))++editor_.revision;catalogDirty_=true;++editor_.catalogVersion;}
    cancel();dirty_=true;
    log::info("Pickup recovery: {:08X}, {:08X}:{:04X} -> {:08X}:{:04X}; {} shortcuts",change.form,change.oldOwner,change.oldID,change.newOwner,change.newID,count);
}
void Game::save(SKSE::SerializationInterface* serial) {
    std::scoped_lock lock(mutex_);
    try {
        const auto bytes=encodeArsenal(arsenal_);
        if(!serial->WriteRecord(recordType,arsenalSchema,bytes.data(),std::uint32_t(bytes.size())))log::error("Could not write Quickslot save record");
        else log::info("Saved {} entries across {} groups",arsenal_.size(),Arsenal::count);
    }catch(const std::exception& e){log::error("Save failed: {}",e.what());}
}
void Game::load(SKSE::SerializationInterface* serial) {
    std::scoped_lock lock(mutex_);closeEditor("loading saved groups");cancel();use_.reset();stopAnimationWatch();arsenal_=Arsenal{};dirty_=true;
    std::uint32_t type,version,length;bool found=false;
    while(serial->GetNextRecordInfo(type,version,length)) {
        if(type!=recordType)continue;
        try {
            if(found||version!=arsenalSchema||length>8388608)throw std::runtime_error("Invalid or duplicate group record");
            found=true;std::string bytes(length,'\0');
            if(serial->ReadRecordData(bytes.data(),length)!=length)throw std::runtime_error("Truncated group record");
            arsenal_=decodeArsenal(bytes,[&](std::uint32_t oldID,std::uint32_t& newID){return serial->ResolveFormID(oldID,newID);});
        }catch(const std::exception& e){arsenal_=Arsenal{};log::error("Load failed: {}",e.what());break;}
    }
}
RE::BSEventNotifyControl Game::ProcessEvent(const RE::MenuOpenCloseEvent* e,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
    std::scoped_lock lock(mutex_);
    if(e) {
        ++menuCaptureEpoch_;
        if(e->menuName==menuName)menuRequested_=e->opening;
        if(e->menuName==slowMenuName&&e->opening&&(!selection_.browsing()||!allowed()||selectionClock(options_.selectionSpeed)!=SelectionClock::slow)) {
            soulsRequested_=false;RE::UIMessageQueue::GetSingleton()->AddMessage(slowMenuName,RE::UI_MESSAGE_TYPE::kHide,nullptr);
        }
        if(e->menuName==pauseMenuName&&e->opening&&(!selection_.browsing()||!allowed()||selectionClock(options_.selectionSpeed)!=SelectionClock::pause)) {
            pauseRequested_=false;RE::UIMessageQueue::GetSingleton()->AddMessage(pauseMenuName,RE::UI_MESSAGE_TYPE::kHide,nullptr);
        }
        if(e->menuName==editorMenuName&&!e->opening&&editor_.open)closeEditor("editor menu removed from stack");
        if(editor_.open) {
            if(const auto reason=editorBlockReason();!reason.empty())closeEditor(reason);
            else updateEditorTextInput();
        }
        if(e->opening&&e->menuName==RE::MainMenu::MENU_NAME){ready_=false;menuAPI().publish(false,false);cancel();use_.reset();stopAnimationWatch();}
        else if((e->opening&&gameplayMenuOpen([&](auto name){return e->menuName==name;}))||!allowed())cancel();
        dirty_=true;publish();
    }
    return RE::BSEventNotifyControl::kContinue;
}
RE::BSEventNotifyControl Game::ProcessEvent(const RE::TESContainerChangedEvent* e,RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
    if(e&&(e->oldContainer==0x14||e->newContainer==0x14)) {
        std::scoped_lock lock(mutex_);++menuCaptureEpoch_;dirty_=true;if(editor_.open){catalogDirty_=true;++editor_.catalogVersion;}
        if(std::any_of(arsenal_.groups.begin(),arsenal_.groups.end(),[&](const Group& group){
            return std::any_of(group.entries.begin(),group.entries.end(),[&](const Entry& entry){return entry.form==e->baseObj&&!entry.spell;});}))
            log::info("Inventory transfer: form {:08X}, {:08X} -> {:08X}, count {}, ID {:04X}",
                      e->baseObj,e->oldContainer,e->newContainer,e->itemCount,e->uniqueID);
    }
    return RE::BSEventNotifyControl::kContinue;
}
RE::BSEventNotifyControl Game::ProcessEvent(const RE::TESUniqueIDChangeEvent* e,RE::BSTEventSource<RE::TESUniqueIDChangeEvent>*) {
    if(!e)return RE::BSEventNotifyControl::kContinue;
    std::scoped_lock lock(mutex_);

    if(!ready_)return RE::BSEventNotifyControl::kContinue;
    const auto count=followIdentityChange(arsenal_,{e->objectID,e->oldBaseID,e->newBaseID,e->oldUniqueID,e->newUniqueID});
    if(editor_.open) {
        editHistory_.identity({e->objectID,e->oldBaseID,e->newBaseID,e->oldUniqueID,e->newUniqueID});
        if(followIdentityChange(editor_.draft,{e->objectID,e->oldBaseID,e->newBaseID,e->oldUniqueID,e->newUniqueID}))++editor_.revision;
        catalogDirty_=true;++editor_.catalogVersion;
    }
    if(count) {
        cancel();dirty_=true;
        log::info("Identity change: form {:08X}, {:08X}:{:04X} -> {:08X}:{:04X}; updated {} shortcuts{}",
                  e->objectID,e->oldBaseID,e->oldUniqueID,e->newBaseID,e->newUniqueID,count,
                  e->newBaseID?"":" (identity removed; recapture required)");
    }
    return RE::BSEventNotifyControl::kContinue;
}
RE::BSEventNotifyControl Game::ProcessEvent(const RE::TESEquipEvent* e,RE::BSTEventSource<RE::TESEquipEvent>*) {
    if(e&&e->actor&&e->actor->GetFormID()==0x14){std::scoped_lock lock(mutex_);dirty_=true;}
    return RE::BSEventNotifyControl::kContinue;
}
}
