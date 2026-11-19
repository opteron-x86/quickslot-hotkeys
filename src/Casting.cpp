#include "Casting.h"
#include "Localization.h"
#include "Version.h"
#include <SKSE/SKSE.h>
// The API headers expect the string literal suffixes from their own precompiled header.
using namespace std::literals;
#include "../third_party/OpenAnimationReplacer/OpenAnimationReplacerAPI-Conditions.h"
#include <atomic>
#include <cmath>
#include <filesystem>
#include <format>
#include <windows.h>
#undef GetObject

// Open Animation Replacer's condition API keeps its interface here; the definitions below are the ones its own source
// files provide, so the condition classes in the copied headers link.
OAR_API::Conditions::IConditionsInterface* g_oarConditionsInterface=nullptr;
namespace Conditions {
CustomCondition::CustomCondition() {
    const auto factory=g_oarConditionsInterface->GetWrappedConditionFactory();
    _wrappedCondition=std::unique_ptr<ICondition>(factory());
    _wrappedCondition->PreInitialize();
}
bool CustomCondition::Evaluate(RE::TESObjectREFR* refr,RE::hkbClipGenerator* clip,void* subMod)const {
    if(IsDisabled())return true;
    return IsNegated()?!EvaluateImpl(refr,clip,subMod):EvaluateImpl(refr,clip,subMod);
}
IConditionComponent* CustomCondition::AddBaseComponent(ConditionComponentType type,const char* name,const char* description) {
    return AddComponent(g_oarConditionsInterface->GetConditionComponentFactory(type),name,description);
}
ConditionType ICondition::GetConditionType()const {
    const auto version=GetConditionAPIVersion();
    return version<ConditionAPIVersion::V3?static_cast<ConditionType>(version):GetConditionTypeImpl();
}
EssentialState ICondition::GetEssential()const {
    return GetConditionAPIVersion()<ConditionAPIVersion::V3?EssentialState::kEssential:GetEssentialImpl();
}
}

namespace quickslot {
namespace {
namespace log=SKSE::log;
constexpr char conditionName[]="Quickslot_Casting";
constexpr char animationPack[]="Data/meshes/actors/character/OpenAnimationReplacer/Quickslot/config.json";
// The running cast, read by Open Animation Replacer while it picks clips, on its own threads. shoutState: the graph is
// in the shout state for this cast. phase: 1 charging or channeling, 2 the release clip plays, 3 it is about to end:
// first person walk and run slots loop, so they hold the pose the release ends in instead of starting the clip again.
std::atomic<int> runningAnimation{0},runningSide{0},runningPhase{0};
std::atomic<bool> shoutState{false};
bool conditionMatches(RE::TESObjectREFR* refr,int animation,int hand,bool needsShoutState,int phase) {
    if(!refr||!refr->IsPlayerRef())return false;
    const int running=runningAnimation.load();
    if(running==0||(animation!=0&&animation!=running))return false;
    if(needsShoutState&&!shoutState.load())return false;
    if(phase!=0&&phase!=runningPhase.load())return false;
    return (hand!=1&&hand!=2)||hand==runningSide.load();
}
void clearCondition(){runningAnimation=0;runningSide=0;runningPhase=0;shoutState=false;}

class CastingCondition final:public Conditions::CustomCondition {
    using Numeric=Conditions::INumericConditionComponent;
    Numeric* animation_=nullptr;
    Numeric* hand_=nullptr;
    Numeric* shoutState_=nullptr;
    Numeric* phase_=nullptr;
    Numeric* add(const char* name,const char* description) {
        return static_cast<Numeric*>(AddBaseComponent(Conditions::ConditionComponentType::kNumeric,name,description));
    }
public:
    CastingCondition() {
        animation_=add("Animation","Casting animation of the running Quickslot cast. 0 any, 1 aimed, 2 self, 3 aimed concentration, 4 self concentration, "
            "5 dual aimed, 6 dual self, 7 dual concentration, 8 two-handed (ritual), 9 dual self concentration.");
        hand_=add("Hand","Hand of the running cast. 0 any, 1 right, 2 left. Two-handed spells only match 0.");
        shoutState_=add("ShoutState","1: only while the cast keeps the graph in the shout state. 0: also while the release clip plays afterwards.");
        phase_=add("Phase","0 any, 1 charging or channeling, 2 the release clip plays, 3 the release clip is about to end.");
    }
    RE::BSString GetName()const override{return conditionName;}
    RE::BSString GetDescription()const override{return "True while Quickslot plays a casting animation of the given type.";}
    REL::Version GetRequiredVersion()const override{return {release::major,release::minor,release::patch};}
    RE::BSString GetRequiredPluginName()const override{return "Quickslot";}
    RE::BSString GetRequiredPluginAuthor()const override{return "Quickslot";}
    RE::BSString GetArgument()const override {
        return std::format("{} / hand {}",animation_->GetArgument().c_str(),hand_->GetArgument().c_str()).c_str();
    }
    RE::BSString GetCurrent(RE::TESObjectREFR*)const override {
        return std::format("{} / hand {}{}",runningAnimation.load(),runningSide.load(),shoutState.load()?" / shout state":"").c_str();
    }
protected:
    bool EvaluateImpl(RE::TESObjectREFR* refr,RE::hkbClipGenerator*,void*)const override {
        return conditionMatches(refr,int(animation_->GetNumericValue(refr)),int(hand_->GetNumericValue(refr)),
            shoutState_->GetNumericValue(refr)>=1.f,int(phase_->GetNumericValue(refr)));
    }
};

RE::PlayerCharacter* player(){return RE::PlayerCharacter::GetSingleton();}
bool firstPerson(){auto* camera=RE::PlayerCamera::GetSingleton();return camera&&camera->IsInFirstPerson();}
bool notify(std::string_view event){auto* p=player();return p&&p->NotifyAnimationGraph(event);}
float magicka(){auto* p=player();return p?p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka):0.f;}
void failFeedback(bool magickaMeter) {
    if(magickaMeter)RE::HUDMenu::FlashMeter(RE::ActorValue::kMagicka);
    RE::PlaySound("MAGFailSD");
}
bool concentrationSpell(const RE::MagicItem* item){return item&&item->GetCastingType()==RE::MagicSystem::CastingType::kConcentration;}
bool aimsAtCrosshair(const RE::MagicItem* item) {
    const auto delivery=item?item->GetDelivery():RE::MagicSystem::Delivery::kSelf;
    return delivery==RE::MagicSystem::Delivery::kAimed||delivery==RE::MagicSystem::Delivery::kTargetLocation;
}
// A real shout or power of the player is running in the voice caster.
bool playerShouting() {
    auto* p=player();auto* voice=p?p->GetMagicCaster(RE::MagicSystem::CastingSource::kOther):nullptr;
    return voice&&voice->state!=RE::MagicCaster::State::kNone;
}
void stopSprinting(RE::PlayerCharacter* p) {
    if(!p->AsActorState()->IsSprinting())return;
    auto* objects=RE::BGSDefaultObjectManager::GetSingleton();
    auto* action=objects?objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionSprintStop):nullptr;
    auto* data=action?RE::TESActionData::Create():nullptr;
    if(!data)return;
    data->source=RE::NiPointer<RE::TESObjectREFR>(p);data->action=action;data->Process();
    data->~TESActionData();RE::free(data);
}
RE::ThirdPersonState* thirdPerson() {
    auto* camera=RE::PlayerCamera::GetSingleton();
    return camera&&camera->IsInThirdPerson()?static_cast<RE::ThirdPersonState*>(camera->currentState.get()):nullptr;
}
float wrapAngle(float angle){constexpr float turn=6.2831853f;angle=std::fmod(angle,turn);return angle<0?angle+turn:angle;}
// Third person: turns the character toward where the camera looks, by at most maxStep radians (all the way below 0).
void faceCamera(float maxStep) {
    auto* third=thirdPerson();auto* p=player();
    if(!third||!p)return;
    const float offset=third->freeRotation.x;
    if(std::abs(offset)<.0001f)return;
    const float step=maxStep<0?offset:std::clamp(offset,-maxStep,maxStep);
    p->SetHeading(wrapAngle(p->GetAngleZ()+step));third->freeRotation.x=offset-step;
}
// What the crosshair points at: the first surface the camera's line of sight meets past the character, or a point far away.
std::optional<RE::NiPoint3> crosshairPoint() {
    auto* camera=RE::PlayerCamera::GetSingleton();auto* p=player();
    if(!camera||!camera->cameraRoot||!p)return {};
    auto* third=thirdPerson();
    const float yaw=p->GetAngleZ()+(third?third->freeRotation.x:0.f),pitch=p->GetAngleX();
    const RE::NiPoint3 forward{std::sin(yaw)*std::cos(pitch),std::cos(yaw)*std::cos(pitch),-std::sin(pitch)};
    constexpr float reach=10000;
    const auto from=camera->cameraRoot->world.translate;
    // The line starts past the character's own collision, which surrounds the first person camera too.
    const auto start=from+forward*((third?from.GetDistance(p->GetPosition()):0.f)+60.f);
    auto point=from+forward*reach;
    auto* cell=p->GetParentCell();auto* world=cell?cell->GetbhkWorld():nullptr;
    if(!world)return point;
    const float scale=RE::bhkWorld::GetWorldScale();
    RE::bhkPickData pick{};
    pick.rayInput.from=RE::hkVector4(start.x*scale,start.y*scale,start.z*scale,0);
    pick.rayInput.to=RE::hkVector4(point.x*scale,point.y*scale,point.z*scale,0);
    pick.rayInput.filterInfo.SetCollisionLayer(RE::COL_LAYER::kLOS);
    if(world->PickObject(pick)&&pick.rayOutput.HasHit())point=start+(point-start)*pick.rayOutput.hitFraction;
    return point;
}
// Points the character's aim from the casting hand at the crosshair while the spell leaves the hand.
class CrosshairAim {
    std::optional<RE::NiPoint3> saved_;
public:
    explicit CrosshairAim(RE::MagicCaster* caster) {
        auto* p=player();const auto point=crosshairPoint();
        if(!p||!point)return;
        auto origin=p->GetPosition();
        if(auto* node=caster?caster->GetMagicNode():nullptr)origin=node->world.translate;
        else origin.z+=(p->GetBoundMax().z-p->GetBoundMin().z)*.7f;
        const auto d=*point-origin;const float horizontal=std::sqrt(d.x*d.x+d.y*d.y);
        if(horizontal<1)return;
        saved_=p->data.angle;
        p->data.angle.x=-std::atan2(d.z,horizontal);p->data.angle.z=wrapAngle(std::atan2(d.x,d.y));
    }
    ~CrosshairAim(){if(auto* p=player();p&&saved_){p->data.angle.x=saved_->x;p->data.angle.z=saved_->z;}}
    CrosshairAim(const CrosshairAim&)=delete;
    CrosshairAim& operator=(const CrosshairAim&)=delete;
};
RE::BGSSoundDescriptorForm* effectSound(RE::MagicItem* item,RE::MagicSystem::SoundID id) {
    auto* effect=item?item->GetCostliestEffectItem():nullptr;auto* setting=effect?effect->baseEffect:nullptr;
    if(!setting)return nullptr;
    for(const auto& pair:setting->effectSounds)if(pair.id==id)return pair.sound;
    return nullptr;
}
void playSound(RE::MagicItem* item,RE::MagicSystem::SoundID id,RE::BSSoundHandle* keep=nullptr) {
    auto* sound=effectSound(item,id);auto* audio=RE::BSAudioManager::GetSingleton();auto* p=player();
    if(!sound||!audio||!p)return;
    RE::BSSoundHandle handle;
    if(!audio->GetSoundHandle(handle,sound))return;
    handle.SetObjectToFollow(p->Get3D());handle.Play();
    if(keep)*keep=handle;
}
// The hand's magic node on the skeleton the camera shows.
RE::NiAVObject* magicNode(bool left) {
    auto* p=player();auto* root=p?p->Get3D(firstPerson()):nullptr;
    return root?root->GetObjectByName(left?"NPC L MagicNode [LMag]":"NPC R MagicNode [RMag]"):nullptr;
}
// The spell's glow in the casting hand or hands while it charges.
void handArt(RE::MagicItem* item,CastSide side,float seconds) {
    auto* effect=item?item->GetCostliestEffectItem():nullptr;
    auto* art=effect&&effect->baseEffect?effect->baseEffect->data.castingArt:nullptr;auto* p=player();
    if(!art||!p)return;
    for(const bool left:{false,true}) {
        if((side==CastSide::right&&left)||(side==CastSide::left&&!left))continue;
        if(auto* node=magicNode(left))p->ApplyArtObject(art,seconds,nullptr,false,false,node);
    }
}
}

SpellCasting& SpellCasting::get(){static SpellCasting casting;return casting;}

void SpellCasting::registerCondition() {
    using namespace OAR_API::Conditions;
    auto* module=GetModuleHandleW(L"OpenAnimationReplacer.dll");
    const auto request=module?reinterpret_cast<_RequestPluginAPI_Conditions>(GetProcAddress(module,"RequestPluginAPI_Conditions")):nullptr;
    if(!request){log::info("Open Animation Replacer not found; spell casting stays off");return;}
    const REL::Version version{release::major,release::minor,release::patch};
    g_oarConditionsInterface=request(InterfaceVersion::V3,"Quickslot",version);
    if(!g_oarConditionsInterface){log::warn("Open Animation Replacer did not provide its condition interface 3; spell casting stays off");return;}
    const auto result=g_oarConditionsInterface->AddCustomCondition(SKSE::GetPluginHandle(),"Quickslot",version,conditionName,
        Conditions::CustomCondition::GetFactory<CastingCondition>());
    registered_=result==APIResult::OK||result==APIResult::AlreadyRegistered;
    if(registered_)log::info("Registered Open Animation Replacer condition {}",conditionName);
    else log::warn("Open Animation Replacer refused condition {} (result {}); spell casting stays off",conditionName,unsigned(result));
}
void SpellCasting::findAnimations() {
    std::error_code error;animations_=std::filesystem::exists(animationPack,error);
    log::info("Casting animations {}; spell casting {}",animations_?"found":"not found",installed()?"available":"unavailable");
}

std::optional<SpellCasting::Status> SpellCasting::status()const {
    if(!sequence_.active()||!item_)return {};
    return Status{item_->GetFormID(),sequence_.progress(),sequence_.channeling()};
}
void SpellCasting::stopSounds() {
    if(chargeSound_.IsValid())chargeSound_.Stop();
    if(loopSound_.IsValid())loopSound_.Stop();
    chargeSound_={};loopSound_={};
}
// Ends the casting animation: "ShoutStop" right away, then the condition stays for the short blend to the idle pose.
void SpellCasting::stopAnimation() {
    sinceRelease_=-1;shoutState=false;
    if(!playerShouting())notify("ShoutStop");
    linger_.linger(.4f,false);
}
void SpellCasting::finish(bool stopped) {
    stopSounds();
    const bool concentration=sequence_.concentration();
    if(auto* p=player();p&&item_&&(sequence_.channeling()||(stopped&&concentration))) {
        if(auto* caster=p->GetMagicCaster(side_==CastSide::left?RE::MagicSystem::CastingSource::kLeftHand:RE::MagicSystem::CastingSource::kRightHand))caster->InterruptCast(false);
        if(p->IsCasting(item_))p->InterruptCast(false);
    }
    if(stopped||concentration)stopAnimation();
    sequence_.end();item_=nullptr;scroll_=nullptr;
    recovery_=std::max(castTiming::recovery,linger_.wait());
}
void SpellCasting::reset() {
    stopSounds();sequence_.end();linger_={};pending_.reset();savedLocomotion_.reset();
    item_=nullptr;scroll_=nullptr;recovery_=0;sinceRelease_=-1;animation_=CastAnimation::none;clearCondition();
}
void SpellCasting::letGo() {
    if(pending_)pending_->held=false;
    sequence_.letGo();
}

SpellCasting::Outcome SpellCasting::cast(RE::MagicItem* item,RE::ScrollItem* scroll,bool leftHand,bool held) {
    if(!installed())return {Result::refused,tr("Spell casting needs Open Animation Replacer and Quickslot Casting Animations")};
    if(!item)return {Result::refused,{}};
    if(sequence_.channeling())return {Result::refused,{}};
    if(sequence_.active()||recovery_>0||linger_.wait()>0) {
        pending_=Pending{item,scroll,leftHand,held};return {Result::waiting,{}};
    }
    if(auto outcome=begin(item,scroll,leftHand,held))return *outcome;
    return {Result::started,{}};
}
// Starts the cast now. Empty when it started.
std::optional<SpellCasting::Outcome> SpellCasting::begin(RE::MagicItem* item,RE::ScrollItem* scroll,bool leftHand,bool held) {
    auto* p=player();
    if(!p)return Outcome{};
    auto* state=p->AsActorState();
    if(p->IsOnMount()){failFeedback(false);return Outcome{Result::refused,tr("Can't cast while riding")};}
    if(state->IsSwimming()){failFeedback(false);return Outcome{Result::refused,tr("Can't cast while swimming")};}
    if(p->IsInMidair()){failFeedback(false);return Outcome{Result::refused,tr("Can't cast in the air")};}
    if(state->GetAttackState()!=RE::ATTACK_STATE_ENUM::kNone||playerShouting())return Outcome{Result::refused,tr("Finish your current action, then try again")};
    const auto weapon=state->GetWeaponState();
    if(weapon!=RE::WEAPON_STATE::kSheathed&&weapon!=RE::WEAPON_STATE::kDrawn){pending_=Pending{item,scroll,leftHand,held};return Outcome{Result::waiting,{}};}
    const bool concentration=concentrationSpell(item);
    const float cost=scroll?0.f:item->CalculateMagickaCost(p);
    if(!scroll&&(concentration?magicka()<=0:magicka()<cost)){failFeedback(true);return Outcome{Result::refused,tr("Not enough magicka")};}

    const bool twoHanded=item->IsTwoHanded();
    animation_=castAnimationFor(item->GetDelivery()==RE::MagicSystem::Delivery::kSelf,concentration,twoHanded);
    side_=twoHanded?CastSide::both:leftHand?CastSide::left:CastSide::right;
    runningAnimation=int(animation_);runningSide=int(side_);runningPhase=1;shoutState=true;
    // In first person the shout behavior's moving state would take the arms half from the walk, so the cast starts
    // standing; the engine's value goes back on the next update.
    std::int32_t moving=0;
    if(firstPerson()&&p->GetGraphVariableInt("iSyncIdleLocomotion",moving)&&moving!=0){if(!savedLocomotion_)savedLocomotion_=moving;p->SetGraphVariableInt("iSyncIdleLocomotion",0);}
    if(!p->NotifyAnimationGraph("ShoutStart")) {
        clearCondition();
        if(sinceDraw_<1.5f){pending_=Pending{item,scroll,leftHand,held};return Outcome{Result::waiting,{}};}
        log::info("Casting animation refused for {:08X}",item->GetFormID());
        failFeedback(false);return Outcome{Result::refused,tr("Can't cast right now")};
    }
    linger_.restart();sinceRelease_=-1;stopSprinting(p);
    item_=item;scroll_=scroll;cost_=cost;
    sequence_.begin(item->GetChargeTime(),concentration,held);
    if(!concentration) {
        handArt(item,side_,castTiming::minimumCharge+std::max(0.f,item->GetChargeTime()));
        playSound(item,RE::MagicSystem::SoundID::kCharge,&chargeSound_);
    }
    log::info("Casting {:08X}: animation {}, hand {}, cost {:.1f}",item->GetFormID(),int(animation_),int(side_),cost);
    return {};
}
bool SpellCasting::fire(std::string& notice) {
    auto* p=player();
    if(!p||!item_)return false;
    if(chargeSound_.IsValid()){chargeSound_.Stop();chargeSound_={};}
    const bool concentration=concentrationSpell(item_);
    if(!scroll_&&(concentration?magicka()<=0:magicka()<cost_)){failFeedback(true);notice=tr("Not enough magicka");return false;}
    auto* caster=p->GetMagicCaster(side_==CastSide::left?RE::MagicSystem::CastingSource::kLeftHand:RE::MagicSystem::CastingSource::kRightHand);
    if(!caster)return false;
    const auto delivery=item_->GetDelivery();
    const bool self=delivery==RE::MagicSystem::Delivery::kSelf,aimed=aimsAtCrosshair(item_);
    if(concentration)caster->currentSpellCost=cost_;
    else if(!scroll_)p->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kMagicka,cost_);
    if(aimed) {
        faceCamera(-1);
        const CrosshairAim aim(caster);
        caster->CastSpellImmediate(item_,false,nullptr,1.f,false,0.f,p);
    } else caster->CastSpellImmediate(item_,false,self?p:nullptr,1.f,false,0.f,self?nullptr:p);
    // A rune or summon that found no place to land keeps casting forever.
    if(delivery==RE::MagicSystem::Delivery::kTargetLocation&&!concentration&&p->IsCasting(item_)) {
        p->InterruptCast(false);
        if(!scroll_)p->AsActorValueOwner()->RestoreActorValue(RE::ActorValue::kMagicka,cost_);
        failFeedback(false);notice=tr("No room to cast that here");return false;
    }
    if(scroll_)p->RemoveItem(scroll_,1,RE::ITEM_REMOVE_REASON::kRemove,nullptr,nullptr);
    playSound(item_,RE::MagicSystem::SoundID::kRelease);
    if(concentration)playSound(item_,RE::MagicSystem::SoundID::kCastLoop,&loopSound_);
    return true;
}
std::string SpellCasting::update(float seconds) {
    std::string notice;
    const float dt=std::isfinite(seconds)?std::clamp(seconds,0.f,.25f):0.f;
    recovery_=std::max(0.f,recovery_-dt);
    auto* p=player();
    if(p) {
        const auto weapon=p->AsActorState()->GetWeaponState();
        sinceDraw_=weapon!=RE::WEAPON_STATE::kSheathed&&weapon!=RE::WEAPON_STATE::kDrawn?0.f:std::min(sinceDraw_+dt,10.f);
        if(savedLocomotion_){p->SetGraphVariableInt("iSyncIdleLocomotion",p->IsMoving()?*savedLocomotion_:0);savedLocomotion_.reset();}
    }
    if(sinceRelease_>=0) {
        sinceRelease_+=dt;
        if(runningPhase.load()==2&&sinceRelease_>=releaseSeconds(animation_)-.05f)runningPhase=3;
    }
    switch(linger_.tick(dt)) {
        case CastLinger::Tick::endShout:shoutState=false;if(!playerShouting())notify("ShoutStop");if(!sequence_.active()){clearCondition();sinceRelease_=-1;}break;
        case CastLinger::Tick::clear:if(!sequence_.active()){clearCondition();sinceRelease_=-1;}break;
        default:break;
    }
    if(pending_) {
        pending_->age+=dt;
        if(pending_->age>castTiming::queueAge)pending_.reset();
        else if(!sequence_.active()&&recovery_<=0&&linger_.wait()<=0) {
            const auto next=*pending_;pending_.reset();
            if(auto outcome=begin(next.item,next.scroll,next.left,next.held)) {
                if(outcome->result==Result::waiting&&pending_)pending_->age=next.age;
                else if(outcome->result==Result::refused)notice=outcome->notice;
            }
        }
    }
    if(!sequence_.active())return notice;
    if(!p||p->IsDead()){finish(true);return notice;}
    stopSprinting(p);
    if(aimsAtCrosshair(item_))faceCamera(10*dt);
    auto* state=p->AsActorState();
    if(p->IsOnMount()||state->IsSwimming()||state->GetAttackState()!=RE::ATTACK_STATE_ENUM::kNone){finish(true);return notice;}
    const auto step=sequence_.tick(dt);
    if(step.stop){finish(true);return notice;}
    if(step.release){runningPhase=2;sinceRelease_=0;notify("MT_BreathExhaleShort");linger_.linger(releaseSeconds(animation_),true);}
    if(step.fire) {
        std::string failure;
        if(fire(failure)){sequence_.fired();if(!sequence_.active())finish(false);}
        else{finish(true);return failure;}
    }
    if(step.restart)p->NotifyAnimationGraph("ShoutStart");
    if(sequence_.channeling()&&magicka()<=0){failFeedback(true);finish(true);return tr("Not enough magicka");}
    return notice;
}
}
