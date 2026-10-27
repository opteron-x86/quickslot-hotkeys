#include "IconPolicy.h"
#include "I4Policy.h"
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
    assert(iconTint(0x80decafe,1,false)==0x80decafe);
    assert(iconTint(0x80decafe,0,false)==0x80ffffff&&iconTint(0x80decafe,2,false)==0x80ffffff);
    assert(iconTint(0x60808080,2,true)==0x60808080);
    Options options;assert(options.itemIconColor==1);
    options.itemIconColor=2;assert(decodeOptions(encodeOptions(options)).itemIconColor==2);
    options.itemIconColor=999;sanitizeOptions(options);assert(options.itemIconColor==2);
    auto previous=Json::parse(encodeOptions(options));previous["itemIconSet"]="old-custom-pack";
    const auto migrated=decodeOptions(previous.dump());
    assert(migrated.itemIconColor==options.itemIconColor&&!Json::parse(encodeOptions(migrated)).contains("itemIconSet"));
    Arsenal arsenal;const auto preset=exportPreset(arsenal,migrated,true,[](auto){return Json{};});
    assert(importPreset(preset,arsenal,Options{},true,[](const auto&,auto){return 0u;}).options.itemIconColor==migrated.itemIconColor);
    assert(std::string(i4DefaultLabel(emptyHandEntry(Hand::left)))=="armor_hands");
    assert(std::string(i4DefaultLabel(unarmedEntry()))=="armor_hands");
    assert(std::string(i4DefaultLabel(e))=="default_shout");
    for(unsigned type=0;type<unsigned(WeaponFamily::count);++type) {
        Entry rule;rule.kind=Kind::smartWeapon;rule.equipmentType=type;rule.twoHanded=smartTwoHanded(type);
        assert(std::string(i4DefaultLabel(rule))!="default_misc");
    }
    Entry arrows;arrows.kind=Kind::smartAmmo;Entry bolts=arrows;bolts.equipmentType=1;
    assert(std::string(i4DefaultLabel(arrows))=="weapon_arrow"&&std::string(i4DefaultLabel(bolts))=="weapon_bolt");
    assert(i4RequestKey(arrows)!=i4RequestKey(bolts));
    Entry sword;sword.form=123;Entry dagger=sword;dagger.form=124;dagger.kind=Kind::dagger;
    assert(i4HasForm(sword)&&i4RequestKey(sword)!=i4RequestKey(dagger));
    sword.unresolved=true;assert(!i4HasForm(sword)&&i4RequestKey(sword).starts_with("category:"));
    Entry original;original.form=0x1234;original.name="Sword";auto appearance=original;appearance.icon=Icon::greatsword;
    assert(original.same(appearance)&&encodeEntry(original)==encodeEntry(appearance));
    assert(i4SourcePath("")=="Interface/skyui/icons_item_psychosteve.swf");
    assert(i4SourcePath("I4IconAddon/icons.swf")=="Interface/I4IconAddon/icons.swf");
    assert(i4SourcePath("Interface\\Example\\Icons.SWF")=="Interface/Example/Icons.SWF");
    for(const auto* path:{"../icons.swf","Interface/../icons.swf","file:///icons.swf","/icons.swf","C:/icons.swf","pack//icons.swf","pack/./icons.swf","https://host/icons.swf","icons.png"})assert(!i4SourcePath(path));
    assert(!i4SourcePath(std::string("icons\0.swf",10)));
    const auto centered=fitI4Icon(-20,-10,20,10,256);assert(centered&&centered->x==128&&centered->y==128&&centered->scale==5.4);
    const auto offset=fitI4Icon(10,20,30,60,256);assert(offset&&std::abs((10+30)*.5*offset->scale+offset->x-128)<1e-9);
    assert(!fitI4Icon(0,0,0,1,256)&&!fitI4Icon(0,0,1,INFINITY,256));
    std::vector<std::uint8_t> pixels{64,32,0,128,9,8,7,0,12,34,56,255};
    assert(straightenI4Pixels(pixels));
    assert((pixels==std::vector<std::uint8_t>{128,64,0,128,0,0,0,0,12,34,56,255}));
    pixels.assign(4,0);assert(!straightenI4Pixels(pixels));
    std::cout<<"PASS: I4 categories and request identity, weapon keywords, hand actions, tint, settings migration, resource paths and pixel conversion\n";
}
