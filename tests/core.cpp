#include "Model.h"
#include "Storage.h"
#include "InputFilter.h"
#include "CallSite.h"
#include "ItemIdentity.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace quickslot;
Entry weapon(unsigned form=0x1234,unsigned uid=1) {
    Entry e;e.form=form;e.owner=0x14;e.unique=std::uint16_t(uid);e.name="Tempered sword";return e;
}
int main() {
    unsigned checks=0;
    const auto check=[&](bool condition){assert(condition);++checks;};
    Group g;g.add(weapon());g.add(weapon(0x1234,2));g.add(weapon(0x5678,3));
    check(g.entries.size()==3);check(g.selected==2);
    check(!g.add(weapon(0x1234,1)));check(g.entries.size()==3&&g.selected==0);
    auto left=weapon();left.hand=Hand::left;check(g.add(left));check(g.entries.size()==4);
    g.selected=0;
    Selection selection;selection.press(10,g);selection.tick(10.1);
    check(selection.held()&&!selection.browsing());
    auto tap=selection.release(g);check(tap&&tap->index==0&&selection.accepts(*tap,g));
    check(!selection.release(g)); // A key-up cannot equip twice.
    selection.press(11,g);check(!selection.accepts(*tap,g));
    selection.tick(11.3);check(selection.browsing());
    selection.cycle(-1,g);check(selection.index()==3);selection.cycle(1,g);check(selection.index()==0);
    selection.cycle(2,g);auto commit=selection.release(g);check(commit&&commit->index==2);
    check(g.selected==0); // Preview does not change the remembered slot.
    selection.cancel();check(!selection.accepts(*commit,g));
    selection.press(12,g);selection.cancel();check(!selection.release(g)); // Escape before hold threshold.
    selection.press(13,g);selection.cycle(1,g);check(selection.browsing()); // Immediate wheel opens preview.
    g.add(weapon(0x9999,5));check(!selection.release(g)); // Edited while a key was held.
    selection.press(14,g);commit=selection.release(g);g.remove();check(!selection.accepts(*commit,g));
    Group empty;selection.press(15,empty);selection.cycle(-1,empty);check(!selection.release(empty));
    Group full;for(unsigned i=1;i<=Group::limit;++i)check(full.add(weapon(i,i)));
    check(!full.add(weapon(99,99))&&full.entries.size()==16);
    check(!full.add(weapon(1,1))&&full.selected==0); // A duplicate at capacity still selects it.
    full.selected=15;check(full.remove()&&full.selected==14);
    while(!full.entries.empty())full.remove();
    check(full.selected==0&&!full.remove());
    const auto identity=[](std::uint32_t oldID,std::uint32_t& newID){newID=oldID;return true;};
    const auto json=encode(g);const auto restored=decode(json,identity);
    check(restored.entries.size()==g.entries.size());check(restored.selected==g.selected);
    for(std::size_t i=0;i<g.entries.size();++i)check(restored.entries[i].same(g.entries[i]));
    const auto remapped=decode(json,[](auto oldID,auto& newID){newID=oldID+0x1000000;return true;});
    check(remapped.entries[0].form==g.entries[0].form+0x1000000);
    check(remapped.entries[0].owner==g.entries[0].owner+0x1000000);
    const auto missing=decode(json,[](auto,auto&){return false;});
    check(missing.entries[0].unresolved&&missing.entries[0].form==0);
    const auto missingAgain=decode(encode(missing),identity);check(missingAgain.entries[0].unresolved);
    // Real inventory IDs may be zero. They must survive save/load and stay
    // distinct from both other copies and matching IDs with different owners.
    Group zeroID;check(zeroID.add(weapon(0x1234,0)));
    check(zeroID.add(weapon(0x1234,1)));
    auto otherOwner=weapon(0x1234,0);otherOwner.owner=0x4321;check(zeroID.add(otherOwner));
    auto zeroRestored=decode(encode(zeroID),identity);
    check(zeroRestored.entries.size()==3&&!zeroRestored.entries[0].unresolved);
    check(zeroRestored.entries[0].same(zeroID.entries[0]));
    check(!zeroRestored.add(weapon(0x1234,0))&&zeroRestored.selected==0);
    const auto ownerMissing=decode(encode(zeroID),[](auto oldID,auto& newID){newID=oldID;return oldID!=0x14;});
    check(ownerMissing.entries[0].unresolved);
    check(decode(encode(ownerMissing),identity).entries[0].unresolved);
    // GetNextUniqueID repeatedly offers an ID until it is used in inventory.
    // A missing shortcut can reserve that ID; allocate locally beyond it.
    UsedItemIDs used;used.set(65535);auto fresh=allocateItemID(used,65535);
    check(fresh.has_value()&&*fresh==0);
    used.set(0);used.set(1);fresh=allocateItemID(used,65535);
    check(fresh.has_value()&&*fresh==2);
    used.set();check(!allocateItemID(used,7));
    used.reset(42);fresh=allocateItemID(used,43);check(fresh&&*fresh==42);
    used.reset();used.set(7);fresh=allocateItemID(used,7);check(fresh&&*fresh==8);
    used.set(*fresh);fresh=allocateItemID(used,7);check(fresh&&*fresh==9);
    // Follow one exact item through world/container/player identities, keeping
    // both hand shortcuts together and other copies of the same weapon separate.
    Group tracking;tracking.add(weapon(0x1234,7));
    auto trackedLeft=weapon(0x1234,7);trackedLeft.hand=Hand::left;tracking.add(trackedLeft);
    tracking.add(weapon(0x1234,8));tracking.add(weapon(0x2345,7));
    auto foreignOwner=weapon(0x1234,7);foreignOwner.owner=0x4321;tracking.add(foreignOwner);
    auto spellReference=weapon(0x1234,7);spellReference.spell=true;tracking.add(spellReference);
    auto lost=weapon(0x1234,7);lost.unresolved=true;tracking.add(lost);
    const auto beforeTransfer=tracking;selection.press(20,tracking);const auto beforeTransferCommit=selection.release(tracking);
    check(followIdentityChange(tracking,{0x1234,0x14,0xff001234,7,0})==2);
    check(tracking.revision==beforeTransfer.revision+1&&!selection.accepts(*beforeTransferCommit,tracking));
    check(tracking.entries[0].owner==0xff001234&&tracking.entries[0].unique==0&&!tracking.entries[0].unresolved);
    check(tracking.entries[1].owner==0xff001234&&tracking.entries[1].hand==Hand::left);
    for(unsigned i=2;i<6;++i)check(tracking.entries[i].same(beforeTransfer.entries[i]));
    check(tracking.entries[6].owner==0x14&&tracking.entries[6].unresolved);
    tracking=decode(encode(tracking),identity); // Save/reload while it is outside inventory.
    check(tracking.entries[0].owner==0xff001234&&tracking.entries[0].unique==0&&!tracking.entries[0].unresolved);
    check(followIdentityChange(tracking,{0x1234,0xff001234,0x9876,0,0})==2); // Owner-only change.
    check(followIdentityChange(tracking,{0x1234,0x9876,0x14,0,39})==2);
    check(tracking.entries[0].owner==0x14&&tracking.entries[0].unique==39);
    check(tracking.entries[1].owner==0x14&&tracking.entries[1].unique==39&&tracking.entries[1].hand==Hand::left);
    check(entryLabel(tracking.entries[1])=="Left hand: Tempered sword");
    check(followIdentityChange(tracking,{0x1234,0x9876,0x14,0,39})==0); // Replayed/old event.
    check(followIdentityChange(tracking,{0x1234,0x14,0x14,39,39})==0);
    check(followIdentityChange(tracking,{0,0x14,0x9876,39,1})==0);
    check(followIdentityChange(tracking,{0x1234,0,0x9876,39,1})==0);
    check(followIdentityChange(tracking,{0x1234,0x14,0,39,0})==2); // Identity destroyed.
    check(tracking.entries[0].unresolved&&tracking.entries[1].unresolved);
    check(followIdentityChange(tracking,{0x1234,0,0x14,0,39})==0); // Never resurrect a removed identity.
    check(decode(encode(tracking),identity).entries[0].unresolved);
    const auto invalid=[&](const std::string& data){try{decode(data,identity);return false;}catch(...){return true;}};
    check(invalid("{"));check(invalid(std::string(65537,'x')));
    auto bad=Json::parse(json);bad["schema"]=99;check(invalid(bad.dump()));
    bad=Json::parse(json);bad["entries"][0]["hand"]=-1;check(invalid(bad.dump()));
    bad=Json::parse(json);bad["entries"][0]["unique"]=65536;check(invalid(bad.dump()));
    bad=Json::parse(json);bad["entries"][0]["form"]=4294967296ULL;check(invalid(bad.dump()));
    bad=Json::parse(json);bad["entries"][0]["name"]=std::string(513,'a');check(invalid(bad.dump()));
    bad=Json::parse(json);bad["selected"]=999u;check(decode(bad.dump(),identity).selected==g.entries.size()-1);
    struct Node {int id;Node* next;};Node c{3,nullptr},b{2,&c},a{1,&b};
    int calls=0;
    filterEvents(&a,[](Node* n){return n->id==2;},[&](Node** p){++calls;check(*p==&a&&a.next==&c&&c.next==nullptr);});
    check(calls==1&&a.next==&b&&b.next==&c);
    filterEvents(&a,[](Node*){return true;},[&](Node** p){check(*p==nullptr);});
    check(a.next==&b&&b.next==&c&&c.next==nullptr);
    filterEvents(&a,[](Node*){return false;},[&](Node** p){check(*p==&a&&a.next==&b&&b.next==&c);});
    filterEvents<Node>(nullptr,[](Node*){return false;},[&](Node** p){check(*p==nullptr);});
    try {
        filterEvents(&a,[](Node* n){return n->id==2;},[](Node**){throw std::runtime_error("downstream");});
    }catch(const std::runtime_error&){}
    check(a.next==&b&&b.next==&c);
    const std::uint8_t e8[]{0xe8,0xfb,0xff,0xff,0xff};
    const auto noSlot=[](std::uintptr_t)->std::optional<std::uintptr_t>{return {};};
    auto call=decodeCall(0x1000,std::span(e8),noSlot);check(call&&call->size==5&&call->target==0x1000);
    const std::uint8_t ff15[]{0xff,0x15,0x1a,0,0,0};
    call=decodeCall(0x1000,std::span(ff15),[&](std::uintptr_t slot)->std::optional<std::uintptr_t>{check(slot==0x1020);return 0x12345678;});
    check(call&&call->size==6&&call->target==0x12345678); // Not the pointer-slot address.
    check(!decodeCall(0x1000,std::span(ff15),noSlot));
    const std::uint8_t unsupported[]{0x90,0,0,0,0};check(!decodeCall(0x1000,std::span(unsupported),noSlot));
    check(!decodeCall(0x1000,std::span(e8,4),noSlot));
    check(cleanName("")=="Unnamed item");
    check(cleanName(std::string(511,'a')+"é").size()==511);
    const auto repaired=cleanName(std::string(1,char(0xff)));check(Json::parse(Json(repaired).dump()).get<std::string>()==repaired);
    std::cout<<"PASS: "<<checks<<" checks (tap/hold/cancel, stale tasks, item identity, storage, event ownership).\n";
}
