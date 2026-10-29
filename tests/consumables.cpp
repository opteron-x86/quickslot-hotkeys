#include "ConsumableUse.h"
#include "EditorModel.h"
#include "ItemIdentity.h"
#include "Storage.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* why,std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(why)+" at line "+std::to_string(at.line()));
    ++checks;
}
Entry item(Kind kind) {
    Entry e;e.form=0x1234+unsigned(kind);e.kind=kind;e.name="Test item";e.spell=kind==Kind::spell||kind==Kind::power||kind==Kind::lesserPower;return e;
}
void potionEffects(const std::filesystem::path& root) {
    std::ifstream stream(root/"tests/fixtures/potion_effects.json");
    const auto fixtures=Json::parse(stream);
    for(const auto& fixture:fixtures.at("cases"))for(int mode=0;mode<3;++mode) {
        PotionCandidate bottle;bottle.entry=item(Kind::potion);bottle.count=2;
        for(const auto& value:fixture.at("effects")) {
            const auto flags=std::stoul(value.at("flags").get<std::string>(),nullptr,0);
            const auto condition=value.value("conditionMode",mode)==mode;
            addPotionEffect(bottle,{PotionArchetype(value.at("archetype").get<unsigned>()),PotionValue(value.at("actorValue").get<unsigned>()),
                value.at("magnitude").get<float>(),value.at("duration").get<unsigned>(),bool(flags&2),bool(flags&4),condition});
        }
        const auto resource=fixture.at("resource").get<unsigned>();
        check(bottle.restore[resource]==fixture.at("expected").get<float>(),"Captured vanilla/Apothecary/CACO restoration has the expected total");
        for(unsigned i=0;i<3;++i)check(bottle.regeneration[i]==0&&(i==resource||bottle.restore[i]==0),"Restoration never aliases regeneration or a different resource");
        std::vector<PotionCandidate> stock{bottle};
        for(unsigned policy=0;policy<3;++policy)check(choosePotion(smartEntry(resource,0,policy),stock,60)==&stock[0],"Every smart restoration policy recognizes captured potion records");
        stock[0].entry.name="Renamed potion";check(choosePotion(smartEntry(resource,0,2),stock,60)==&stock[0],"Names do not determine potion eligibility");
        auto excluded=smartEntry(resource,0,2);excluded.exclusions={{stock[0].entry.form,0}};
        check(!choosePotion(excluded,stock,60),"Recognizing modded effects preserves explicit exclusions");
        stock[0].count=0;check(!choosePotion(smartEntry(resource,0,2),stock,60),"An empty recognized potion is not usable");
    }
    const PotionEffect restore{PotionArchetype::peakValueModifier,PotionValue::health,5,10};
    for(unsigned variant=0;variant<8;++variant) {
        auto effect=restore;
        switch(variant) {
        case 0:effect.recover=true;break;
        case 1:effect.detrimental=true;break;
        case 2:effect.conditionsMet=false;break;
        case 3:effect.archetype=PotionArchetype(999);break;
        case 4:effect.magnitude=-1;break;
        case 5:effect.magnitude=NAN;break;
        case 6:effect.magnitude=INFINITY;break;
        case 7:effect.magnitude=std::numeric_limits<float>::max();break;
        }
        PotionCandidate bottle;addPotionEffect(bottle,effect);
        check(bottle.restore==std::array<float,3>{},"Fortification, detrimental/inactive/unsupported effects and invalid strengths cannot become restoration");
    }
    for(unsigned resource=0;resource<3;++resource)for(auto archetype:{PotionArchetype::valueModifier,PotionArchetype::peakValueModifier}) {
        PotionCandidate bottle;PotionEffect effect{archetype,PotionValue(unsigned(PotionValue::healthRate)+resource),50,300,true};
        addPotionEffect(bottle,effect);effect.magnitude=25;addPotionEffect(bottle,effect);
        check(bottle.regeneration[resource]==50&&bottle.restore==std::array<float,3>{},"Regeneration retains its strongest magnitude and remains separate from restoration");
        effect.magnitude=100;effect.conditionsMet=false;addPotionEffect(bottle,effect);
        check(bottle.regeneration[resource]==50,"Inactive regeneration variants do not affect ranking");
    }
    for(unsigned resource=0;resource<3;++resource) {
        std::vector<PotionCandidate> stock(4);
        for(unsigned i=0;i<stock.size();++i) {
            auto& bottle=stock[i];bottle.entry=item(Kind::potion);bottle.entry.form+=i;bottle.count=1;
            addPotionEffect(bottle,{i?PotionArchetype::peakValueModifier:PotionArchetype::valueModifier,
                PotionValue(unsigned(PotionValue::health)+resource),i?float(5u<<(i-1)):25.f,i?10u:0u,i==3});
        }
        auto rule=smartEntry(resource,0,0);
        check(choosePotion(rule,stock,40)==&stock[0],"Instant restoration competes with duration-based restoration for lowest strength");
        rule.policy=1;
        check(choosePotion(rule,stock,40)==&stock[2],"Highest strength uses the full restoration duration and excludes fortification");
        rule.policy=2;
        check(choosePotion(rule,stock,40)==&stock[1],"Best fit selects the smallest sufficient duration-based potion");
        rule.exclusions={{stock[1].entry.form,0}};
        check(choosePotion(rule,stock,40)==&stock[2],"Excluding a matching overhaul potion advances to the next sufficient potion");
        stock[2].count=0;
        check(choosePotion(rule,stock,40)==&stock[0],"Depleted overhaul potions leave eligible instant restoration available");
        stock[2].count=2;
        check(choosePotion(rule,stock,40)==&stock[2],"Restocking makes the recognized overhaul potion eligible again");
    }
}
int main(int argc,char** argv)try {
    if(argc!=2)return 2;
    potionEffects(argv[1]);
    const auto identity=[](auto before,auto& after){after=before;return true;};
    std::ifstream file(std::filesystem::path(argv[1])/"tests/fixtures/arsenal.json");
    const std::string bytes((std::istreambuf_iterator<char>(file)),{});const auto saved=Json::parse(bytes);
    const auto restored=decodeArsenal(bytes,identity);
    check(restored.size()==23&&[&]{auto upgraded=Json::parse(encodeArsenal(restored));upgraded.erase("shortcuts");for(auto& group:upgraded["groups"])for(auto& entry:group["entries"])entry.erase("shortcut");return upgraded==saved;}(),"Current save round-trips all 23 shortcuts");
    const Kind kinds[]{Kind::torch,Kind::potion,Kind::food,Kind::poison,Kind::scroll,Kind::power,Kind::lesserPower,Kind::shout};
    for(const auto kind:kinds) {
        auto e=item(kind);e.hand=normalizedHand(e,Hand::left);Group g;g.add(e);
        check(!tracksInstance(e)&&!e.owner&&!e.unique,"Stack and learned abilities never allocate item IDs");
        check(isInventory(e)==isStack(e),"Only inventory-backed categories use inventory lookup");
        check(!g.add(e)&&g.entries.size()==1,"Restocking cannot create another shortcut for the same stack");
        check(decode(encode(g),identity).entries[0].same(e),"Every new category survives co-save round trip");
        const auto mapped=decode(encode(g),[](auto before,auto& after){after=before+0x1000000;return true;});
        check(mapped.entries[0].form==e.form+0x1000000&&mapped.entries[0].owner==0,"Remap base forms without inventing copy owners");
        const auto missing=decode(encode(g),[](auto,auto&){return false;});
        check(missing.entries[0].unresolved&&decode(encode(missing),identity).entries[0].unresolved,"Removed content remains a missing placeholder");
        g.entries[0].owner=20;g.entries[0].unique=7;
        check(followIdentityChange(g,{e.form,20,42,7,8})==0,"Physical-copy tracking ignores stack/voice entries");
        CatalogItem c;c.entry=e;
        const int filter=kind==Kind::torch?3:kind==Kind::potion?4:kind==Kind::food?5:kind==Kind::poison?6:kind==Kind::scroll?7:8;
        for(int f=0;f<=8;++f)check(catalogMatches(c,"",f)==(f==0||f==filter),"New categories appear only in their intended filters");
        check(!flexibleHand(e)==(kind!=Kind::scroll),"Only scrolls expose hand choice");
        if(isVoice(e))check(handName(e)=="Voice","Voice abilities use the voice slot");
    }
    auto scroll=item(Kind::scroll);scroll.hand=Hand::left;Group scrolls;scrolls.add(scroll);scroll.hand=Hand::right;
    check(scrolls.add(scroll)&&scrolls.entries.size()==2,"A scroll stack can have separate left and right shortcuts");
    scroll.twoHanded=true;check(!flexibleHand(scroll)&&normalizedHand(scroll,Hand::left)==Hand::right,"Ritual scrolls use both hands");
    auto crafted=item(Kind::potion);crafted.form=0xff000123;crafted.recipe=0x123456789abcdef0ull;Group craft;craft.add(crafted);
    check(decode(encode(craft),identity).entries[0].recipe==crafted.recipe,"Full 64-bit recipe signature survives storage");
    crafted.recipe++;
    check(!crafted.same(craft.entries[0]),"A reused dynamic ID with a different recipe is not the saved item");
    const auto invalid=[&](Json value){try{decode(value.dump(),identity);return false;}catch(...){return true;}};
    auto bad=Json::parse(encode(craft));bad["entries"][0]["recipe"]=-1;check(invalid(bad),"Reject negative recipe signatures");
    bad=Json::parse(encode(craft));bad["entries"][0]["kind"]=100u;check(invalid(bad),"Reject future/invalid categories");
    bad=Json::parse(encode(craft));bad["entries"][0]["owner"]=20u;check(invalid(bad),"Reject a stack with an instance owner");
    bad=Json::parse(encode(craft));bad["entries"][0]["hand"]=1u;check(invalid(bad),"Reject a hand-specific potion");
    bad=Json::parse(encode(craft));bad["entries"][0]["spell"]=true;check(invalid(bad),"Reject consumable masquerading as a learned spell");
    bad=Json::parse(encode(craft));bad["schema"]=1;check(invalid(bad),"Unsupported group schema is rejected");
    Row row{item(Kind::potion),false,false,{},0};
    check(rowStatus(row)=="Empty","Zero stock keeps a recoverable shortcut visible");
    row.available=true;row.quantity=7;check(rowStatus(row)=="×7","Restocking restores the existing row's quantity feedback");
    row={item(Kind::shout),true,true,{},0,2.2f};
    check(rowStatus(row)=="Voice · Selected · 3s cooldown","Cooldown is rounded up without claiming the shout was cast");
    row={item(Kind::power),true,false,{},0,0,true};check(rowStatus(row)=="Voice · Recharging","A recharging power remains selectable");

    ConsumableUse use;using R=ConsumableUse::Result;int requests=0;
    const auto request=[&](double now,int count,bool poison=false){if(use.begin(now,count,poison)){++requests;return true;}return false;};
    check(!request(0,0)&&!request(0,-1)&&requests==0,"Empty/invalid stacks never send a use request");
    check(request(1,3)&&requests==1,"One explicit action sends one request");
    for(int i=0;i<20;++i)check(!request(1.1+i*.1,3),"Animation delay blocks duplicate requests across groups");
    check(use.observe(4,3,false)==R::waiting&&requests==1,"Unchanged count waits; it does not retry or claim success");
    check(use.observe(5,2,false)==R::decreased&&!use.active(),"Delayed native consumption confirms by inventory change");
    check(use.observe(5.1,2,false)==R::idle,"One use cannot confirm twice");
    check(request(6,2)&&use.observe(6.01,1,false)==R::decreased,"Immediate vanilla use also confirms");
    check(!request(6.02,1),"Duplicate rapid release cannot spend a second dose");
    check(request(7,1)&&use.observe(7.1,0,false)==R::decreased,"Last dose can be consumed safely");
    check(!request(7.5,0)&&request(8,4),"Empty stack refuses use; restocked stack resumes");
    check(use.observe(18,4,false)==R::unconfirmed&&!use.active(),"Interrupted/declined animation expires without retry");
    const int previous=requests;check(use.observe(19,4,false)==R::idle&&requests==previous,"Timeout cannot trigger delayed consumption");
    check(request(20,1,true)&&use.observe(20.1,1,true)==R::waiting,"Poison waits for the native dialog");
    check(use.observe(60,1,true)==R::waiting&&use.active(),"Reading a poison dialog does not consume or expire it");
    check(use.observe(61,1,false)==R::cancelled&&!use.active(),"Cancelling the native prompt leaves the dose intact");
    check(request(62,1,true)&&use.observe(62.1,1,true)==R::waiting&&use.observe(63,0,false)==R::decreased,"Accepted poisoning confirms the count change");
    check(request(64,1,true)&&use.observe(74,1,false)==R::unconfirmed,"No poisonable weapon or declined request does not pretend to succeed");
    check(request(75,3),"A fresh explicit use can follow a declined request");use.reset();
    check(use.observe(76,2,false)==R::idle&&request(76,2),"Load/reset discards the old observer");
    Group potions;potions.add(item(Kind::potion));Selection selection;selection.press(80,potions);selection.cancel();
    check(!selection.release(potions),"Cancelling a consumable carousel cannot commit a dose");
    selection.press(81,potions);auto commit=selection.release(potions);check(bool(commit)&&!selection.release(potions),"Tap releases at most one consumable commit");
    selection.press(82,potions);check(!selection.accepts(*commit,potions),"A stale queued consume is rejected before its native call");
    std::cout<<"PASS: "<<checks<<" consumable/voice checks (current save storage, stacks, filters, recipe guard, one-shot use, animation delay, poison prompts, depleted/restocked counts).\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
