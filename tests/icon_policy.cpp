#include "IconPolicy.h"
#include "WeaponIcons.h"
#include "Settings.h"
#include "Presets.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace quickslot;
int main() {
    Entry e;e.kind=Kind::axe;assert(baseIcon(e)==Icon::axe);
    e.icon=Icon::battleaxe;assert(baseIcon(e)==Icon::battleaxe);
    e.icon=Icon::bolt;e.kind=Kind::ammo;assert(baseIcon(e)==Icon::bolt);
    e.icon=Icon::automatic;e.kind=Kind::smartPotion;e.resource=2;assert(baseIcon(e)==Icon::stamina);
    e.kind=Kind::power;assert(baseIcon(e)==Icon::greater_power);
    e.kind=Kind::lesserPower;assert(baseIcon(e)==Icon::lesser_power);
    e.kind=Kind::shout;assert(baseIcon(e)==Icon::shout);
    assert(baseIcon(unarmedEntry())==Icon::unarmed);
    const auto first=atlasRegion(Icon::sword),last=atlasRegion(Icon::greater_power),lesser=atlasRegion(Icon::lesser_power);
    assert(first&&first->u0==0&&first->v0==0&&first->u1==.125f&&first->v1==1.f/9);
    assert(last&&last->u0==0&&last->v0==8.f/9&&last->u1==.125f&&last->v1==1);
    assert(lesser&&lesser->u0==.875f&&lesser->v0==7.f/9&&lesser->u1==1&&std::abs(lesser->v1-8.f/9)<1e-6f);
    static_assert(unsigned(Icon::woodcutters_axe)==58&&unsigned(Icon::gun)==63);
    assert(atlasRegion(Icon::arrow)&&atlasRegion(Icon::bolt)&&atlasRegion(Icon::unarmed));
    for(unsigned i=1;i<unsigned(Icon::count);++i) {
        const auto uv=atlasRegion(Icon(i));assert(uv&&uv->u0>=0&&uv->v0>=0&&uv->u1<=1&&uv->v1<=1);
    }
    for(const auto icon:{Icon::automatic,Icon::count,Icon(999)})assert(!atlasRegion(icon));
    const auto classify=[](std::initializer_list<std::string_view> keywords,bool twoHanded=false) {
        WeaponIconSelection selected;for(const auto keyword:keywords)selected.consider(keyword,twoHanded);return selected.icon;
    };
    assert(classify({"WeapTypeSword","WeapTypeRapier"})==Icon::rapier);
    assert(classify({"OCF_WeapTypeKatana1H"})==Icon::katana);
    assert(classify({"OCF_WeapTypeKatana2H"})==Icon::nodachi);
    assert(classify({"WeapTypeKatana"},true)==Icon::nodachi);
    assert(classify({"ABC_I4WIO_Katana","WeapTypeNodachi"})==Icon::nodachi);
    assert(classify({"OCF_WeapTypeSpear2H","ABC_I4WIO_Shortspear"})==Icon::shortspear);
    assert(classify({"WeapTypeSpear","WeapTypePike"})==Icon::pike);
    assert(classify({"OCF_WeapTypeHalberd2H","WeapTypeNaginata"})==Icon::naginata);
    assert(classify({"OCF_WeapTypeClub1H","WeapTypeSpikedClub"})==Icon::spiked_club);
    assert(classify({"WeapTypeSpikedClub","OCF_WeapTypeClub1H"})==Icon::spiked_club);
    assert(classify({"OCF_WeapTypeWoodaxe1H","OCF_WeapTypeWoodHatchet1H"})==Icon::hatchet);
    assert(classify({"ABC_I4WIO_Pickaxe1H"})==Icon::pickaxe);
    assert(classify({"OCF_WeapTypeWoodaxe2H"})==Icon::woodcutters_axe);
    assert(classify({"WeapTypeSword","WeapTypeClaw"})==Icon::claw);
    assert(classify({"WeapTypeJavelin"})==Icon::javelin);
    assert(classify({"WeapTypeQuarterstaff"})==Icon::quarterstaff);
    assert(classify({"WeapTypeWhip"})==Icon::whip);
    assert(classify({"WeapTypeGun"})==Icon::gun);
    assert(baseIcon(emptyHandEntry(Hand::right))==Icon::unarmed&&baseIcon(emptyHandEntry(Hand::left))==Icon::unarmed);
    assert(classify({"Fancy Katana","OCF_varWeapNotKatana","OCF_WeapTypeGun2H_BasicSpear","WeapTypeSpearExtra"})==Icon::automatic);
    assert(atlasTint(0x80decafe,1,false)==0x80decafe);
    assert(atlasTint(0x80decafe,0,false)==0x80ffffff&&atlasTint(0x80decafe,2,false)==0x80ffffff);
    assert(atlasTint(0x60808080,2,true)==0x60808080);
    Options options;assert(options.itemIconColor==1);
    options.itemIconColor=2;assert(decodeOptions(encodeOptions(options)).itemIconColor==2);
    options.itemIconColor=999;sanitizeOptions(options);assert(options.itemIconColor==2);
    options.itemIconSet="sample-icons";assert(decodeOptions(encodeOptions(options)).itemIconSet=="sample-icons");
    auto previous=Json::parse(encodeOptions(options));previous.erase("itemIconSet");assert(decodeOptions(previous.dump()).itemIconSet=="quickslot");
    Arsenal arsenal;const auto preset=exportPreset(arsenal,options,true,[](auto){return Json{};});
    assert(importPreset(preset,arsenal,Options{},true,[](const auto&,auto){return 0u;}).options.itemIconSet=="sample-icons");
    options.itemIconSet="../other";sanitizeOptions(options);assert(options.itemIconSet=="quickslot");
    const auto folder=std::filesystem::temp_directory_path()/("quickslot-icon-packs-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {std::filesystem::path folder;~Cleanup(){std::error_code e;std::filesystem::remove_all(folder,e);}} cleanup{folder};
    assert(loadIconPacks(folder).size()==1);
    const Json pack{{"schema",1},{"id","sample-icons"},{"name","Sample icons"},{"atlas","items.png"},{"columns",8},{"rows",9},{"cellSize",256},{"icons",65}};
    const auto write=[&](const char* name,const Json& metadata){const auto path=folder/"packs"/name;std::filesystem::create_directories(path);std::ofstream(path/"pack.json")<<metadata;std::ofstream(path/"items.png")<<"test";};
    write("01-valid",pack);
    auto malformed=pack;malformed["atlas"]="../items.png";write("02-traversal",malformed);
    malformed=pack;malformed["id"]="quickslot";write("03-reserved",malformed);
    malformed=pack;malformed["icons"]=58;write("04-dimensions",malformed);
    malformed=pack;malformed["id"]="invalid/name";write("05-id",malformed);
    write("06-duplicate",pack);
    write("07-invalid-json",pack);std::ofstream(folder/"packs/07-invalid-json/pack.json")<<"{";
    malformed=pack;malformed["id"]="old-layout";malformed["rows"]=8;malformed["icons"]=63;write("08-old-layout",malformed);
    const auto packs=loadIconPacks(folder);assert(packs.size()==2&&packs[1].id=="sample-icons"&&packs[1].atlas==folder/"packs/01-valid/items.png");
    std::filesystem::remove_all(folder/"packs");std::ofstream(folder/"packs")<<"not a directory";
    assert(loadIconPacks(folder).size()==1);
    Entry original;original.form=0x1234;original.name="Sword";auto appearance=original;appearance.icon=Icon::greatsword;
    assert(original.same(appearance)&&encodeEntry(original)==encodeEntry(appearance));
    std::cout<<"PASS: 65 atlas cells, distinct power icons, weapon keywords, hand actions, tint, optional pack validation, settings/presets and equipment identity isolation\n";
}
