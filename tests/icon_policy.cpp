#include "IconPolicy.h"
#include "WeaponIcons.h"
#include "Settings.h"
#include <cassert>
#include <iostream>
using namespace quickslot;
int main() {
    Entry e;e.kind=Kind::axe;assert(baseIcon(e)==Icon::axe);
    e.icon=Icon::battleaxe;assert(baseIcon(e)==Icon::battleaxe);
    e.icon=Icon::bolt;e.kind=Kind::ammo;assert(baseIcon(e)==Icon::bolt);
    e.icon=Icon::automatic;e.kind=Kind::smartPotion;e.resource=2;assert(baseIcon(e)==Icon::stamina);
    assert(baseIcon(unarmedEntry())==Icon::unarmed);
    const auto first=atlasRegion(Icon::sword),last=atlasRegion(Icon::woodcutters_axe);
    assert(first&&first->u0==0&&first->v0==0&&first->u1==.125f&&first->v1==.125f);
    assert(last&&last->u0==.125f&&last->v0==.875f&&last->u1==.25f&&last->v1==1);
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
    assert(classify({"WeapTypeSword","WeapTypeClaw"})==Icon::automatic);
    assert(classify({"Fancy Katana","OCF_varWeapNotKatana","OCF_WeapTypeGun2H_BasicSpear","WeapTypeSpearExtra"})==Icon::automatic);
    assert(atlasTint(0x80decafe,1,false)==0x80decafe);
    assert(atlasTint(0x80decafe,0,false)==0x80ffffff&&atlasTint(0x80decafe,2,false)==0x80ffffff);
    assert(atlasTint(0x60808080,2,true)==0x60808080); // Preserve unavailable state and opacity.
    Options options;assert(options.itemIconColor==1);
    options.itemIconColor=2;assert(decodeOptions(encodeOptions(options)).itemIconColor==2);
    options.itemIconColor=999;sanitizeOptions(options);assert(options.itemIconColor==2);
    Entry original;original.form=0x1234;original.name="Sword";auto appearance=original;appearance.icon=Icon::greatsword;
    assert(original.same(appearance)&&encodeEntry(original)==encodeEntry(appearance));
    std::cout<<"PASS: 58 atlas cells, weapon keyword precedence/fallback, ammo/unarmed, tint, settings and equipment identity isolation\n";
}
