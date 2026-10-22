#include "Settings.h"
#include "Storage.h"
#include "ItemIdentity.h"
#include "EquipmentAction.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>

using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* reason,std::source_location location=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(reason)+" at line "+std::to_string(location.line()));
    ++checks;
}
Entry item(unsigned id,Hand hand=Hand::right) {
    Entry e;e.form=0x1234;e.owner=0x14;e.unique=std::uint16_t(id);e.hand=hand;e.name="Sword "+std::to_string(id);return e;
}
int main()try {
    const auto identity=[](auto oldID,auto& newID){newID=oldID;return true;};
    {
        Group group;
        const std::array<std::string,6> names{"zebra","alpha","ALPHA","Épée","e\xCC\x81pée","Beta"};
        for(unsigned i=0;i<names.size();++i){auto entry=item(i);entry.name=names[i];if(i==3){entry.form=entry.owner=0;entry.unresolved=true;}group.add(entry);}
        group.selected=3;const auto revision=group.revision;
        Selection pending;const auto commit=pending.request(group,0,0);
        check(group.sortByName()&&group.revision==revision+1,"Sorting changes group order and invalidates stale actions");
        const std::array<unsigned,6> expected{1,2,5,0,3,4};
        for(std::size_t i=0;i<expected.size();++i)check(group.entries[i].unique==expected[i],"A-Z ignores case, normalizes Unicode, and preserves equal-name order");
        check(group.entries[group.selected].unique==3&&group.entries[group.selected].unresolved,"Sorting follows the remembered item even when unavailable");
        check(!pending.accepts(*commit,group),"An action queued before sorting cannot target a different item");
        check(!group.sortByName()&&group.revision==revision+1,"Already sorted order is a no-op");
        auto added=item(9);added.name="aardvark";
        check(group.add(added,true)&&group.selected==0&&group.entries[0].unique==9,"Auto-sorting an addition selects the newly added item");
        check(!group.add(added,true)&&group.selected==0&&group.entries.size()==7,"Adding an existing item selects it without duplicating it");
        auto restored=decode(encode(group),identity);
        check(restored.selected==group.selected&&encode(restored)==encode(group),"Sorted order, exact identities and selection survive save/load");
        group.reorder(0,group.entries.size()-1);added.unique=10;added.name="Axe";group.add(added);
        check(group.entries.back().unique==10,"Auto-sort off retains manual order and appends additions");
        Group empty;check(!empty.sortByName(),"Sorting an empty group is safe");
    }
    Arsenal a;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)a.groups[i].add(item(i*10+j));
    a.groups[0].selected=0;a.groups[1].selected=1;a.groups[2].selected=2;
    Selection s;
    s.press(1,a.groups[0],0);s.cycle(1,a.groups[0]);
    s.press(2,a.groups[1],1);
    check(!s.release(a.groups[0],0)&&s.held(),"An older group's release must not commit the new group");
    const auto second=s.release(a.groups[1],1);
    check(second&&second->group==1&&second->index==1,"Release commits the current group");
    check(s.accepts(*second,a.groups[1],1)&&!s.accepts(*second,a.groups[0],0),"Equal revisions in another group cannot accept a commit");
    s.press(3,a.groups[2],2);check(!s.accepts(*second,a.groups[1],1),"Changing groups invalidates queued equipment");
    s.cancel();check(!s.release(a.groups[2],2),"Cancelled release never equips");
    auto clear=s.request(a.groups[1],1,2);
    check(clear&&clear->index==2&&!s.held(),"Unequip captures the highlighted entry without a later release action");
    check(!s.release(a.groups[1],1),"Backspace cannot be followed by an accidental equip on release");
    s.press(4,a.groups[0],0);check(!s.accepts(*clear,a.groups[1],1),"New group cancels a queued unequip");
    s.cancel();check(!s.request(a.groups[0],0,99),"Out-of-range unequip is rejected");

    auto& order=a.groups[0];order.selected=1;const auto remembered=order.entries[1];
    s.press(5,order,0);s.cycle(-1,order);const auto moving=order.entries[0];
    check(order.reorder(0,2),"Move a card later in the carousel");s.rebase(order,2);
    check(order.entries[order.selected].same(remembered),"Reorder preserves the remembered item");
    auto reordered=s.release(order,0);
    check(reordered&&order.entries[reordered->index].same(moving),"Release after reorder equips the moved card");
    check(!order.reorder(2,3)&&!order.reorder(0,0),"Reorder at boundaries is a no-op");
    check(order.reorder(2,0)&&order.entries[order.selected].same(remembered),"Moving earlier preserves another remembered item");
    order.selected=2;const auto selected=order.entries[2];order.reorder(2,0);
    check(order.selected==0&&order.entries[0].same(selected),"Moving the remembered item follows its new position");
    for(unsigned scan:{203u,205u}) {
        const auto arrow=selectorArrow(scan,false);check(arrow.delta==(scan==203?-1:1)&&!arrow.reorder,"Bare arrows browse instead of editing group order");
        auto list=order;list.selected=scan==203?0:list.entries.size()-1;const auto before=encode(list);
        Selection browse;browse.press(0,list);browse.cycle(arrow.delta,list);
        check(browse.index()==(scan==203?list.entries.size()-1:0)&&encode(list)==before,"Arrow browsing wraps in either direction without changing saved order");
        const auto move=selectorArrow(scan,true);check(move.reorder&&move.delta==arrow.delta,"Ctrl keeps fast arrow reordering available");
        const auto movingItem=list.entries[list.selected];const auto target=wrappedIndex(list.selected,move.delta,list.entries.size());
        Selection reorder;reorder.press(0,list);check(list.reorder(list.selected,target),"Ctrl-arrow crosses the first/last boundary");reorder.rebase(list,target);
        const auto commit=reorder.release(list);check(commit&&list.entries[commit->index].same(movingItem),"Wrapped reordering keeps the moved item highlighted and commits that item");
    }
    check(wrappedIndex(0,-1,0)==0&&wrappedIndex(0,1,1)==0,"Empty and single-entry groups have safe arrow boundaries");

    Arsenal moves;moves.groups[0].add(item(0));moves.groups[0].add(item(1));moves.groups[0].add(item(2));
    moves.groups[0].selected=2;const auto saved=moves.groups[0].entries[2];
    check(moves.move(0,0,1)&&moves.active==1,"Move activates destination without an equipment request");
    check(moves.groups[0].entries[moves.groups[0].selected].same(saved),"Source remembered item survives deletion before it");
    check(moves.groups[1].entries[0].unique==0,"Zero ID is preserved in moves");
    for(unsigned i=100;i<116;++i)moves.groups[2].add(item(i));
    const auto fullBefore=encodeArsenal(moves);
    check(!moves.move(0,0,2)&&encodeArsenal(moves)==fullBefore,"Full destination does not remove the source");
    moves.groups[0].add(item(100));const auto beforeMerge=moves.size();
    check(moves.move(0,moves.groups[0].selected,2),"An existing entry in a full destination can accept a move");
    check(moves.groups[2].entries.size()==16&&moves.size()==beforeMerge-1&&moves.groups[2].selected==0,"Duplicate shortcut merges without deleting inventory");
    check(!moves.move(0,99,1)&&!moves.move(0,0,0)&&!moves.move(Arsenal::count,0,1),"Invalid moves do not mutate groups");
    Arsenal only;only.groups[0].add(item(5));check(only.move(0,0,2)&&only.groups[0].selected==0&&only.groups[0].entries.empty(),"Moving the final source entry leaves a valid empty group");

    a.active=2;auto roundtrip=decodeArsenal(encodeArsenal(a),identity);
    check(roundtrip.active==2&&roundtrip.size()==9,"All groups and active group round-trip");
    for(std::size_t i=0;i<Arsenal::count;++i)check(encode(roundtrip.groups[i])==encode(a.groups[i]),"Each group retains order, identities and remembered index");
    const auto remapped=decodeArsenal(encodeArsenal(a),[](auto oldID,auto& newID){newID=oldID+0x1000000;return true;});
    check(remapped.groups[2].entries[0].form==0x1001234&&remapped.groups[2].entries[0].owner==0x1000014,"References remap in later groups too");
    const auto missing=decodeArsenal(encodeArsenal(a),[](auto,auto&){return false;});
    check(missing.groups[0].entries[0].unresolved&&missing.groups[2].entries[0].unresolved,"Failed references stay missing across groups");
    const auto invalid=[&](const Json& json){try{decodeArsenal(json.dump(),identity);return false;}catch(...){return true;}};
    auto bad=Json::parse(encodeArsenal(a));bad["active"]=Arsenal::count;check(invalid(bad),"Reject out-of-range active group");
    bad=Json::parse(encodeArsenal(a));bad["groups"].erase(1);check(invalid(bad),"Reject missing groups");
    bad=Json::parse(encodeArsenal(a));bad["active"]=-1;check(invalid(bad),"Reject negative active index");
    bad=Json::parse(encodeArsenal(a));bad["groups"][2]["entries"][0]["unique"]=65536;check(invalid(bad),"Validate identities in every group");
    check(invalid(Json::parse(encode(a.groups[0]))),"A standalone group is not a complete save");
    for(unsigned schema=0;schema<7;++schema) {
        bad=Json::parse(encodeArsenal(a));bad["schema"]=schema;
        check(invalid(bad),"Unsupported save schemas are rejected");
    }
    for(unsigned schema=0;schema<5;++schema) {
        bad=Json::parse(encodeArsenal(a));bad["groups"][0]["schema"]=schema;
        check(invalid(bad),"Unsupported nested group schemas are rejected");
    }
    bad=Json::parse(encodeArsenal(a));bad.erase("presetIdentity");check(invalid(bad),"Incomplete save identity is rejected");

    Arsenal shared;
    shared.groups[0].add(item(7));shared.groups[1].add(item(7,Hand::left));shared.groups[2].add(item(7));
    shared.groups[2].add(item(8));
    check(followIdentityChange(shared,{0x1234,0x14,0xff001234,7,0})==3,"All shortcuts follow an identity change, including inactive groups");
    check(shared.groups[2].entries[1].unique==8,"Another copy is not rebound");
    check(followIdentityChange(shared,{0x1234,0xff001234,0x14,0,17})==3,"Returned item reconnects in all groups");

    using P=EquipmentPhase;using R=EquipmentProgress;
    check(equipmentProgress(P::movingOut,true,true,false,true,false)==R::wait,"Wait until exact copy leaves the other hand");
    check(equipmentProgress(P::movingOut,true,true,false,false,false)==R::equipAfterMove,"Equip only after source unequip is observed");
    check(equipmentProgress(P::movingOut,false,true,false,false,false)==R::cancelled,"Cancellation prevents second step of hand move");
    check(equipmentProgress(P::movingOut,true,false,false,false,false)==R::unavailable,"Dropped item prevents second step");
    check(equipmentProgress(P::movingOut,true,true,false,false,true)==R::timeout,"An expired move cannot start a delayed equip");
    check(equipmentProgress(P::equipping,true,true,true,false,false)==R::complete,"Confirm the target hand after moving");
    check(equipmentProgress(P::unequipping,true,true,true,false,false)==R::wait,"Unequip waits for exact item to leave saved hand");
    check(equipmentProgress(P::unequipping,true,true,false,true,false)==R::complete,"Item can remain in other hand after unequip");
    check(equipmentProgress(P::unequipping,false,true,false,false,false)==R::cancelled,"Cancelled operation cannot report completion");
    check(equipmentProgress(P::unequipping,true,true,true,false,true)==R::timeout,"A blocked unequip is reported, not forced");

    Options bindings;bindings.keys[0]=3;bindings.keys[1]=3;bindings.keys[2]=4;sanitizeOptions(bindings);
    check(bindings.keys[0]==3&&bindings.keys[1]==0&&bindings.keys[2]==4,"Clear duplicate keys while preserving the first binding");
    bindings.keys[0]=14;bindings.keys[1]=211;bindings.keys[2]=1;sanitizeOptions(bindings);
    for(auto key:bindings.keys)check(!key||validGroupKey(key),"Editing and modifier keys cannot become group bindings");
    {
        Options keys;
        check(assignGroupKey(keys,0,3)&&keys.keys[0]==3&&keys.keys[1]==2,"Assigning an occupied group key swaps the bindings");
        check(assignGroupKey(keys,0,0)&&decodeOptions(encodeOptions(keys)).keys[0]==0,"An explicitly cleared hotkey survives settings reload");
        check(assignGroupKey(keys,0,2)&&keys.keys[1]==0,"An unbound group can take another group's key without a temporary binding");
        check(assignGroupKey(keys,0,5)&&keys.keys[3]==0,"An inactive group's key is freed instead of blocking reassignment");
        check(!assignGroupKey(keys,0,keys.editorKey)&&!assignGroupKey(keys,0,1),"Editor and reserved keys remain protected");
        Arsenal groups;keys.autoSort[1]=true;
        check(removeGroup(groups,keys,1)&&!keys.keys[1]&&!keys.autoSort[1],"Removing a group frees its key and resets its sorting preference");
        keys.autoSort[0]=true;
        const auto restored=decodeOptions(encodeOptions(keys));
        check(restored.autoSort==keys.autoSort&&restored.keys==keys.keys,"Per-group sorting and unbound keys persist together");
        auto oldSettings=Json::parse(encodeOptions(keys));oldSettings.erase("autoSort");
        check(decodeOptions(oldSettings.dump()).autoSort==Options{}.autoSort,"Existing settings default to manual ordering");
    }
    std::cout<<"PASS: "<<checks<<" checks (groups, storage, quick editing, unequip, hand-move cancellation).\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
