#include "LiveEditor.h"
#include "Presets.h"
#include "Recovery.h"
#include "I4Policy.h"
#include "EquippedState.h"
#include "EquipmentAction.h"
#include "ItemIdentity.h"
#include <iostream>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* label,std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(label)+" at "+std::to_string(at.line()));++checks;
}
Entry piece(std::uint32_t form,ArmorPart part,const char* name,ArmorWeight weight=ArmorWeight::heavy) {
    Entry e;e.kind=Kind::armor;e.form=form;e.owner=0x14;e.unique=std::uint16_t(form&0xffff);e.name=name;e.equipmentType=armorType(part,weight);return e;
}
Entry held(std::uint32_t form,Kind kind,Hand hand,const char* name,bool twoHanded=false) {
    Entry e;e.kind=kind;e.form=form;e.name=name;e.twoHanded=twoHanded;e.spell=kind==Kind::spell;
    if(tracksInstance(e)){e.owner=0x14;e.unique=std::uint16_t(form&0xffff);}
    e.hand=normalizedHand(e,hand);return e;
}
Entry outfit(std::vector<Entry> parts,bool exclusive=false,const char* name="Test outfit") {
    Entry e;e.kind=Kind::outfit;e.parts=std::move(parts);e.exclusive=exclusive;e.name=name;return e;
}
bool rejects(const std::string& bytes) {
    try{decodeArsenal(bytes,[](auto old,auto& mapped){mapped=old;return true;});return false;}catch(const std::exception&){return true;}
}
int main()try {
    const auto identity=[](auto old,auto& mapped){mapped=old;return true;};
    const auto cuirass=piece(0x1001,ArmorPart::body,"Steel Armor"),helmet=piece(0x1002,ArmorPart::head,"Steel Helmet"),
        gauntlets=piece(0x1003,ArmorPart::hands,"Steel Gauntlets"),boots=piece(0x1004,ArmorPart::feet,"Steel Boots"),
        robes=piece(0x2001,ArmorPart::body,"Mage Robes",ArmorWeight::clothing),hood=piece(0x2002,ArmorPart::head,"Mage Hood",ArmorWeight::clothing),
        ring=piece(0x3001,ArmorPart::ring,"Ring of Smithing",ArmorWeight::clothing),amulet=piece(0x3002,ArmorPart::amulet,"Amulet of Zenithar",ArmorWeight::clothing),
        circlet=piece(0x3003,ArmorPart::circlet,"Gold Circlet",ArmorWeight::clothing),cloak=piece(0x4001,ArmorPart::other,"Fur Cloak",ArmorWeight::light);
    const auto sword=held(0x5001,Kind::sword,Hand::right,"Steel Sword"),shield=held(0x5002,Kind::shield,Hand::left,"Steel Shield"),
        flames=held(0x5003,Kind::spell,Hand::left,"Flames"),bow=held(0x5004,Kind::bow,Hand::right,"Hunting Bow",true);

    // A worn piece is an inventory item with its own copy and no hand.
    check(validArmorEntry(cuirass)&&tracksInstance(cuirass)&&isInventory(cuirass)&&!isStack(cuirass)&&wornGear(cuirass),"A worn piece follows its exact copy");
    check(!flexibleHand(helmet)&&!pairable(helmet)&&!entryForHand(helmet,Hand::left)&&!entryForHand(helmet,Hand::right)&&normalizedHand(helmet,Hand::left)==Hand::right,"A worn piece takes no hand");
    check(!handButtonTarget(ring,Hand::left)&&entryHands(ring,true)==0&&!objectEquipHand(ring,false)&&!objectEquipHand(ring,true),"Hand buttons and hand markers leave worn pieces alone");
    check(wornForEntry(ring,true,false)&&wornForEntry(ring,false,true)&&!wornForEntry(ring,false,false),"A piece counts as worn in either worn state");
    check(handName(cuirass)=="Body"&&handName(helmet)=="Head"&&handName(ring)=="Ring"&&handName(amulet)=="Amulet"&&handName(circlet)=="Circlet"&&handName(cloak)=="Worn","Pieces are named by where they sit");
    check(jewelry(ring)&&jewelry(amulet)&&jewelry(circlet)&&!jewelry(helmet)&&!jewelry(sword),"Rings, amulets and circlets are jewelry");
    {
        Row row;row.entry=helmet;row.available=row.equipped=true;check(rowStatus(row)=="Head · Worn","A worn piece says so");
        row.equipped=false;check(rowStatus(row)=="Head","A piece in the pack shows its place");
    }
    {
        Selection::Commit commit{};Options o;
        check(selectionAction(cuirass,commit,o)==SelectionAction::equip&&selectionAction(outfit({cuirass}),commit,o)==SelectionAction::equip,"Pieces and outfits go through the equip action");
        Group g;g.add(helmet);check(!rememberHand(g,0,Hand::left),"A hand choice never changes a worn piece");
    }

    // Icons: the piece's part in the artwork of its weight.
    check(std::string(i4DefaultLabel(cuirass))=="armor_body"&&std::string(i4DefaultLabel(hood))=="clothing_head"&&std::string(i4DefaultLabel(cloak))=="default_armor","Armor icons follow part and weight");
    check(std::string(i4DefaultLabel(piece(1,ArmorPart::feet,"Boots",ArmorWeight::light)))=="lightarmor_feet"&&std::string(i4DefaultLabel(ring))=="armor_ring"&&std::string(i4DefaultLabel(amulet))=="armor_amulet"&&std::string(i4DefaultLabel(circlet))=="armor_circlet","Light armor and jewelry have their own artwork");
    check(i4HasForm(cuirass)&&i4RequestKey(cuirass)=="form:"+std::to_string(cuirass.form),"A piece asks for its own item icon");

    // Outfits: pieces first, then no hands, one two-handed item or a right and a left entry.
    const auto steel=outfit({cuirass,helmet,gauntlets,boots},true),mage=outfit({robes,hood}),crafting=outfit({ring,amulet});
    check(validOutfit(steel)&&validOutfit(crafting)&&isReusable(steel)&&!isInventory(steel)&&!tracksInstance(steel),"An outfit is a shortcut that holds pieces");
    check(validOutfit(outfit({cuirass,sword,shield}))&&validOutfit(outfit({cuirass,bow}))&&validOutfit(outfit({sword,flames})),"An outfit can set a pair of hands, a two-handed item, or only hands");
    check(validOutfit(outfit({cuirass,emptyHandEntry(Hand::right),emptyHandEntry(Hand::left)}))&&validOutfit(outfit({cuirass,sword,emptyHandEntry(Hand::left)})),"Empty hands are part of an outfit's hands");
    check(!validOutfit(outfit({}))&&!validOutfit(outfit({cuirass,cuirass}))&&!validOutfit(outfit({sword,shield,cuirass})),"Empty outfits, repeated pieces and hands before pieces are invalid");
    check(!validOutfit(outfit({cuirass,sword}))&&!validOutfit(outfit({cuirass,sword,shield,flames}))&&!validOutfit(outfit({cuirass,shield,sword})),"Hands are one two-handed item or a right then a left entry");
    {
        auto left=sword;left.hand=Hand::left;auto rightShield=shield;rightShield.hand=Hand::right;
        check(!validOutfit(outfit({sword,left}))&&!validOutfit(outfit({rightShield,flames})),"The same copy in both hands and a shield in the right hand are invalid");
        check(!validOutfit(outfit({cuirass,smartEquipmentEntry(false,unsigned(WeaponFamily::sword)),emptyHandEntry(Hand::left)}))&&!validOutfit(outfit({cuirass,unarmedEntry()})),"Smart choices and Unarmed stay out of outfits");
        std::vector<Entry> many;for(unsigned i=0;i<=outfitPieceLimit;++i)many.push_back(piece(0x9000+i,ArmorPart::other,"Piece"));
        check(!validOutfit(outfit(many)),"An outfit holds at most its piece limit");many.pop_back();check(validOutfit(outfit(many)),"A full outfit is valid");
        auto bad=cuirass;bad.equipmentType=15;check(!validArmorEntry(bad)&&!validOutfit(outfit({bad})),"A piece needs a known part");
    }
    check(outfitPieces(outfit({cuirass,helmet,sword,shield})).size()==2&&outfitHands(outfit({cuirass,helmet,sword,shield})).size()==2,"An outfit splits into pieces and hands");
    check(outfitFace(mage)->form==robes.form&&outfitFace(crafting)->form==ring.form&&outfitFace(outfit({helmet,cuirass}))->form==cuirass.form,"An outfit shows its body piece, or its first piece");
    check(std::string(i4DefaultLabel(mage))=="clothing_body"&&std::string(i4DefaultLabel(outfit({sword,flames})))==i4DefaultLabel(sword),"An outfit's icon is its face");
    check(outfitSummary(steel)=="4 pieces"&&outfitSummary(outfit({ring}))=="1 piece"&&outfitSummary(outfit({cuirass,sword,shield}))=="1 piece + hands","An outfit says what it holds");
    check(outfitName(mage)=="Mage Robes outfit"&&handName(mage)=="Outfit"&&entryHands(steel,true)==0,"An outfit is named after its body piece");
    check(steel.same(outfit({cuirass,helmet,gauntlets,boots},true,"Other name"))&&!steel.same(outfit({cuirass,helmet,gauntlets,boots},false))&&!steel.same(mage),"Outfits compare by pieces and by whether they take the rest off");
    {
        const auto pair=handCombination(outfitHands(outfit({cuirass,sword,shield})),"Knight");
        check(pair&&pair->kind==Kind::combo&&pair->parts.size()==2&&validPair(pair->parts[0],pair->parts[1])&&equipmentSequence(*pair),"A pair of hands equips as a combination");
        check(!handCombination(outfitHands(outfit({cuirass,bow})),"Hunter")&&!handCombination({},"None"),"A two-handed item and no hands need no combination");
    }

    // What comes off and what goes on.
    {
        const std::vector<Entry> worn{cuirass,helmet,gauntlets,boots,ring};
        auto change=wearChange(mage,worn);
        check(change.remove.empty()&&change.add.size()==2,"An outfit worn over other gear only adds its pieces");
        auto strict=mage;strict.exclusive=true;change=wearChange(strict,worn);
        check(change.remove.size()==5&&change.add.size()==2,"An exclusive outfit takes every other piece off");
        change=wearChange(steel,{cuirass,helmet,ring});
        check(change.add.size()==2&&change.remove.size()==1&&samePiece(change.remove[0],ring),"Pieces already on stay on");
        change=wearChange(crafting,{cuirass,ring});check(change.add.size()==1&&samePiece(change.add[0],amulet)&&change.remove.empty(),"A jewelry set leaves the armor alone");
        // Back from the mage outfit to the steel armor and ring.
        WornBefore before{7,worn,{},true};
        change=restoreChange(strict,before,{robes,hood});
        check(change.remove.size()==2&&change.add.size()==5,"Going back takes the outfit off and puts the earlier pieces on");
        before.pieces={robes,ring};change=restoreChange(strict,before,{robes,hood,amulet});
        check(change.remove.size()==1&&samePiece(change.remove[0],hood)&&change.add.size()==1&&samePiece(change.add[0],ring),"A piece worn before and in the outfit stays on, and later pieces are left alone");
        change=removeChange(steel,{cuirass,boots,ring});check(change.remove.size()==2&&change.add.empty(),"Taking an outfit off removes its worn pieces only");
    }
    {
        auto saved=mage;saved.shortcut=7;std::optional<WornBefore> before;
        check(!returnsToEarlierGear(before,saved),"Nothing to go back to before an outfit goes on");
        rememberWornBefore(before,saved,{cuirass},{sword,shield});
        check(before&&before->outfit==7&&before->pieces.size()==1&&before->hands.size()==2&&!before->applied&&returnsToEarlierGear(before,saved),"The earlier gear is recorded when the outfit starts to go on");
        rememberWornBefore(before,saved,{robes},{});check(before->pieces[0].form==cuirass.form,"A second try at an incomplete outfit keeps the first record");
        before->applied=true;check(returnsToEarlierGear(before,saved),"A worn outfit can go back to the gear recorded for it");
        auto other=steel;other.shortcut=8;check(!returnsToEarlierGear(before,other),"The record belongs to one outfit");
        rememberWornBefore(before,other,{robes,hood},{});check(before->outfit==8&&before->pieces.size()==2&&!before->applied,"Another outfit records the gear worn before it");
        rememberWornBefore(before,other,{robes},{});before->applied=true;rememberWornBefore(before,other,{cuirass},{});
        check(before->pieces.size()==1&&before->pieces[0].form==cuirass.form&&!before->applied,"Wearing a complete outfit again records the gear of that moment");
    }

    // Saving: pieces in groups, outfits in the library, and the earlier gear.
    Arsenal a;Options o;
    check(a.groups[0].add(helmet)&&a.groups[0].add(ring)&&!a.groups[0].add(helmet)&&a.groups[0].entries.size()==2,"Pieces are group entries, once each");
    EditCommand c;c.action=EditAction::saveShortcut;c.group=1;c.shortcut=steel;
    check(editStructure(a,o,c)&&a.shortcuts.size()==1&&a.groups[1].entries[0].kind==Kind::outfit&&a.groups[1].entries[0].shortcut,"An outfit is saved as a shared shortcut");
    const auto steelID=a.groups[1].entries[0].shortcut;
    check(addShortcut(a,2,steelID)&&shortcutUses(a,steelID)==2,"One outfit can sit in several groups");
    c.shortcut=outfit({cuirass,sword,shield},false,"Knight");check(editStructure(a,o,c),"An outfit with hands is saved");
    const auto knightID=a.groups[1].entries[1].shortcut;
    c.shortcut=outfit({});check(!editStructure(a,o,c),"An invalid outfit is not saved");
    c.shortcut=outfit({robes,hood},true,"Mage");c.libraryID=steelID;
    check(editStructure(a,o,c)&&a.groups[2].entries[0].parts[0].form==robes.form&&a.groups[1].entries[0].name=="Mage","Editing an outfit updates every group that holds it");
    a.wornBefore=WornBefore{knightID,{robes,ring},{sword,emptyHandEntry(Hand::left)},true};
    const auto bytes=encodeArsenal(a);const auto restored=decodeArsenal(bytes,identity);
    check(encodeArsenal(restored)==bytes,"Pieces, outfits and the earlier gear survive saving");
    check(restored.groups[0].entries[0].kind==Kind::armor&&armorPart(restored.groups[0].entries[0])==ArmorPart::head&&armorWeight(restored.groups[0].entries[0])==ArmorWeight::heavy&&restored.groups[0].entries[0].unique==helmet.unique,"A saved piece keeps its copy, part and weight");
    check(restored.groups[1].entries[0].exclusive&&!restored.groups[1].entries[1].exclusive&&restored.groups[1].entries[1].parts.size()==3&&restored.wornBefore&&restored.wornBefore->applied&&restored.wornBefore->hands.size()==2,"A saved outfit keeps its switch and its hands");
    {
        auto j=Json::parse(bytes);j["groups"][0]["entries"][0]["equipmentType"]=15u;check(rejects(j.dump()),"A piece with an unknown part is rejected");
        j=Json::parse(bytes);j["groups"][0]["entries"][0]["twoHanded"]=true;check(rejects(j.dump()),"A two-handed piece is rejected");
        j=Json::parse(bytes);j["shortcuts"][0]["parts"]=Json::array();check(rejects(j.dump()),"An outfit without parts is rejected");
        j=Json::parse(bytes);j["shortcuts"][0]["parts"].push_back(j["shortcuts"][0]);check(rejects(j.dump()),"An outfit inside an outfit is rejected");
        j=Json::parse(bytes);j["shortcuts"][0].erase("exclusive");check(rejects(j.dump()),"An outfit needs its switch");
        j=Json::parse(bytes);j["shortcuts"][1]["parts"].erase(2);check(rejects(j.dump()),"An outfit with one one-handed hand entry is rejected");
        j=Json::parse(bytes);j["wornBefore"]["outfit"]=999u;auto kept=decodeArsenal(j.dump(),identity);
        check(!kept.wornBefore&&kept.shortcuts.size()==2&&kept.groups[1].entries.size()==2,"An unreadable record of earlier gear is left out and the rest loads");
        j=Json::parse(bytes);j["wornBefore"]["pieces"][0]["kind"]=unsigned(Kind::sword);check(!decodeArsenal(j.dump(),identity).wornBefore,"Earlier pieces must be worn pieces");
        const auto recovered=recoverArsenal(bytes,identity);check(recovered&&!recovered->dropped&&recovered->arsenal.wornBefore,"Recovery keeps a valid save as it is");
        j=Json::parse(bytes);j["groups"][0]["entries"][1]["equipmentType"]=99u;const auto repaired=recoverArsenal(j.dump(),identity);
        check(repaired&&repaired->dropped==1&&repaired->arsenal.groups[0].entries.size()==1&&repaired->arsenal.shortcuts.size()==2,"Recovery drops one unreadable piece and keeps the outfits");
        // A form that no longer resolves leaves the piece in place as unavailable.
        const auto missing=decodeArsenal(bytes,[&](auto old,auto& mapped){mapped=old;return old!=robes.form;});
        check(missing.groups[1].entries[0].parts[0].unresolved&&validOutfit(missing.groups[1].entries[0])&&missing.groups[1].entries[0].parts[1].form==hood.form,"A piece from a missing plugin stays in the outfit as unavailable");
    }

    // An item that changes its identity is followed in pieces, outfits and the earlier gear.
    {
        auto followed=restored;const IdentityChange change{robes.form,robes.owner,0x14,robes.unique,0x7777};
        const auto count=followIdentityChange(followed,change);
        check(count>=3&&followed.groups[1].entries[0].parts[0].unique==0x7777&&followed.shortcuts[0].parts[0].unique==0x7777,"A copy's new identity reaches outfits and the library");
        bool inRecord=false;eachSavedEntry(followed,[&](const Entry& e){eachLeaf(e,[&](const Entry& leaf){inRecord|=leaf.form==ring.form;});});
        check(inRecord,"The earlier gear counts among the saved entries");
        std::size_t leaves=0;eachLeaf(steel,[&](const Entry&){++leaves;});check(leaves==4,"An outfit's parts are its leaves");
    }
    {
        auto pruned=restored;check(deleteShortcut(pruned,knightID)&&!pruned.wornBefore&&pruned.groups[1].entries.size()==1,"Deleting an outfit drops the gear recorded for it");
        const auto preset=Json::parse(exportPreset(restored,o,false,[](std::uint32_t id)->Json{return {{"file","Test.esp"},{"local",id&0xffffff}};}));
        check(!preset["arsenal"].contains("wornBefore")&&preset["arsenal"]["shortcuts"].size()==2,"A preset carries outfits without the earlier gear");
        const auto imported=importPreset(preset.dump(),restored,o,presetPart::all,[](const std::string&,std::uint32_t local){return local;});
        check(!imported.arsenal.wornBefore&&imported.arsenal.shortcuts.size()==2,"Importing a preset clears the earlier gear");
    }

    // Favorites: armor and jewelry are categories of their own, and saved sources keep their numbers.
    check(unsigned(FavoriteSource::offhand)==10&&unsigned(FavoriteSource::selected)==11&&unsigned(FavoriteSource::armor)==12&&unsigned(FavoriteSource::jewelry)==13,"Saved Favorites sources keep their numbers");
    check(favoriteCategoryBit(FavoriteSource::weapons)==1&&favoriteCategoryBit(FavoriteSource::offhand)==256&&favoriteCategoryBit(FavoriteSource::armor)==512&&favoriteCategoryBit(FavoriteSource::jewelry)==1024&&allFavoriteCategories==2047,"Saved category selections keep their bits");
    check(!favoriteCategoryBit(FavoriteSource::selected)&&!favoriteCategoryBit(FavoriteSource::all)&&!favoriteCategoryBit(FavoriteSource::manual),"Only categories have bits");
    check(favoriteMatches(FavoriteSource::armor,cuirass)&&favoriteMatches(FavoriteSource::armor,cloak)&&!favoriteMatches(FavoriteSource::armor,ring)&&!favoriteMatches(FavoriteSource::armor,shield),"Favorite armor holds armor and clothing");
    check(favoriteMatches(FavoriteSource::jewelry,ring)&&favoriteMatches(FavoriteSource::jewelry,circlet)&&!favoriteMatches(FavoriteSource::jewelry,helmet)&&favoriteMatches(FavoriteSource::all,ring)&&!favoriteMatches(FavoriteSource::weapons,cuirass),"Favorite jewelry holds rings, amulets and circlets");
    check(favoriteSourceForCategories(512)==FavoriteSource::armor&&favoriteSourceForCategories(1024)==FavoriteSource::jewelry&&favoriteSourceForCategories(1536)==FavoriteSource::selected&&favoriteSourceForCategories(2047)==FavoriteSource::all&&favoriteSourceForCategories(511)==FavoriteSource::selected,"Category selections name their source");
    check(std::string(favoriteSourceName(FavoriteSource::armor))=="Favorite armor / clothing"&&std::string(favoriteSourceName(FavoriteSource::jewelry))=="Favorite jewelry"&&std::string(favoriteSourceName(FavoriteSource::selected))=="Favorite categories","The new categories have names");
    {
        Group g;check(setFavoriteCategories(g,favoriteCategoryBit(FavoriteSource::armor)|favoriteCategoryBit(FavoriteSource::jewelry),Hand::left),"A group can follow favorite armor and jewelry");
        refreshFavorites(g,{sword,cuirass,ring,shield,helmet});
        check(g.entries.size()==3&&std::all_of(g.entries.begin(),g.entries.end(),[](const Entry& e){return e.kind==Kind::armor&&e.hand==Hand::right;}),"The group lists favorite pieces, whatever its default hand");
        const auto loaded=decode(encode(g),identity);check(loaded.source==FavoriteSource::selected&&loaded.favoriteCategories==1536,"The selection survives saving");
        Group single;setFavoriteSource(single,FavoriteSource::jewelry,Hand::right);const auto again=decode(encode(single),identity);
        check(again.source==FavoriteSource::jewelry&&favoriteCategoryMask(again)==1024,"A single new category survives saving");
    }

    // The editor's item list and outfit drafts.
    {
        CatalogItem item{cuirass,1,1,false,true,{}};
        check(catalogMatches(item,"steel",catalogFilter::armor)&&catalogMatches(item,"",catalogFilter::all)&&!catalogMatches(item,"",catalogFilter::weapons)&&!catalogMatches(item,"",catalogFilter::offhand),"Armor and jewelry have their own filter");
        item.entry=sword;check(!catalogMatches(item,"",catalogFilter::armor)&&!catalogMatches(item,"",catalogFilter::shortcuts)&&!catalogMatches(item,"",catalogFilter::outfits),"Inventory stays out of the armor and shortcut filters");
        check(shortcutMatches(steel,"",catalogFilter::outfits)&&shortcutMatches(steel,"helmet",catalogFilter::shortcuts)&&!shortcutMatches(steel,"",catalogFilter::combinations)&&!shortcutMatches(steel,"",catalogFilter::smart),"Outfits have their own filter and match by their pieces");
        Entry combo;combo.kind=Kind::combo;combo.parts={sword,shield};check(!shortcutMatches(combo,"",catalogFilter::outfits)&&shortcutMatches(combo,"",catalogFilter::combinations),"Combinations stay out of the outfit filter");
    }
    {
        EditorState state;state.open=true;state.catalogVersion=1;
        unsigned id=0;for(const auto& e:{cuirass,helmet,ring,sword,shield,bow})state.catalog.push_back({e,++id,1,false,true,{}});
        const auto capture=[&](std::uint64_t candidate,Hand hand)->std::optional<Entry>{auto e=catalogItem(state,candidate)->entry;e.hand=normalizedHand(e,hand);e.unique=std::uint16_t(0x100+candidate);return e;};
        EditCommand draft;draft.action=EditAction::saveOutfit;draft.catalogVersion=1;draft.shortcut=outfit({cuirass,helmet,sword,shield},true,"");draft.pieceCandidates={1,2,4,5};
        check(validOutfitDraft(state,draft),"A draft picked from the item list is valid");
        const auto made=composeOutfit(state,draft,capture);
        check(made&&made->name=="Steel Armor outfit"&&made->parts[0].unique==0x101&&made->parts[3].unique==0x105&&made->exclusive,"Saving captures each picked item and names the outfit");
        auto wrong=draft;wrong.pieceCandidates={1,2,4};check(!validOutfitDraft(state,wrong),"Each part needs its item list entry");
        wrong=draft;wrong.pieceCandidates={4,2,4,5};check(!validOutfitDraft(state,wrong),"A weapon cannot stand in for a piece");
        wrong=draft;wrong.pieceCandidates={1,2,1,5};check(!validOutfitDraft(state,wrong),"A piece cannot stand in for a hand");
        wrong=draft;wrong.catalogVersion=2;check(!validOutfitDraft(state,wrong),"A draft from an earlier item list is refused");
        wrong=draft;wrong.pieceCandidates={1,2,4,0};check(!validOutfitDraft(state,wrong),"A new part cannot claim to be saved already");
        auto both=draft;both.shortcut=outfit({ring,bow});both.pieceCandidates={3,6};check(validOutfitDraft(state,both)&&composeOutfit(state,both,capture)->parts[1].twoHanded,"A two-handed item fills both hands");
        both.pieceCandidates={3,4};check(!validOutfitDraft(state,both),"A one-handed item cannot stand in for a two-handed one");
        auto bare=draft;bare.shortcut=outfit({cuirass,emptyHandEntry(Hand::right),emptyHandEntry(Hand::left)});bare.pieceCandidates={1,0,0};check(validOutfitDraft(state,bare),"Empty hands need no item");
        check(!composeOutfit(state,draft,[](std::uint64_t,Hand)->std::optional<Entry>{return {};}),"A piece that cannot be captured stops the save");
        // Editing a saved outfit: kept parts stay as saved, new parts come from the list.
        state.draft=a;auto edit=draft;edit.libraryID=knightID;edit.shortcut=*libraryShortcut(a,knightID);edit.pieceCandidates={0,0,0};
        check(validOutfitDraft(state,edit),"A saved outfit is a valid draft of itself");
        edit.shortcut.parts.insert(edit.shortcut.parts.begin()+1,helmet);edit.pieceCandidates={0,2,0,0};
        const auto grown=composeOutfit(state,edit,capture);check(grown&&grown->parts.size()==4&&grown->parts[1].unique==0x102&&grown->parts[0].unique==cuirass.unique&&grown->name=="Knight","Adding a piece keeps the saved parts and the name");
        edit.shortcut.parts[0]=boots;check(!validOutfitDraft(state,edit),"A kept part must match the saved outfit");
        edit=draft;edit.libraryID=999;check(!validOutfitDraft(state,edit),"A draft for a missing outfit is refused");
    }
    std::cout<<"PASS: "<<checks<<" armor and outfit checks (pieces, icons, outfit validity, gear changes, earlier gear, saving, recovery, identity, presets, Favorites categories, filters, drafts).\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
