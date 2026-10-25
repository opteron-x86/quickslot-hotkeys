#include "EditorModel.h"
#include "Settings.h"
#include "Transfer.h"
#include "Layout.h"
#include "EditorLaunch.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* reason,std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(reason)+" at line "+std::to_string(at.line()));
    ++checks;
}
Entry item(unsigned id,Hand hand=Hand::right) {Entry e;e.form=0x1234;e.owner=0x14;e.unique=std::uint16_t(id);e.hand=hand;e.name="Sword "+std::to_string(id);return e;}
int main(int argc,char** argv)try {
    const auto identity=[](auto oldID,auto& newID){newID=oldID;return true;};
    {
        EditorState s;s.open=true;s.session=1;s.revision=1;s.catalogVersion=10;
        Entry spell;spell.kind=Kind::spell;spell.spell=true;spell.form=42;spell.name="Healing";
        s.catalog.push_back({spell,1,1,false,true,{}});
        auto weapon=item(3);s.catalog.push_back({weapon,2,1,false,true,{}});
        EditCommand c;c.action=EditAction::saveCombination;c.catalogVersion=10;c.shortcut.kind=Kind::combo;c.shortcut.parts.resize(2);c.components={1,1};
        const auto capture=[&](std::uint64_t id,Hand hand)->std::optional<Entry>{auto e=catalogItem(s,id)->entry;e.hand=hand;return e;};
        auto pair=composeCombination(s,c,capture);
        check(pair&&pair->name=="Healing · Both hands"&&pair->parts[0].hand==Hand::right&&pair->parts[1].hand==Hand::left,"Known spell forms a named two-hand combination without saved singles");
        check(s.draft.size()==0,"Composing does not create temporary entries");
        auto save=c;save.action=EditAction::saveShortcut;save.shortcut=*pair;
        check(editStructure(s.draft,s.options,save)&&s.draft.size()==1,"Both hands are committed as one group entry");
        c.components={2,2};check(!validCombinationDraft(s,c),"One physical weapon cannot fill both hands");
        s.catalog[1].entry.unique=0;s.catalog[1].count=2;check(validCombinationDraft(s,c),"A plain stack of two weapons can supply separate copies");
        unsigned next=10;pair=composeCombination(s,c,[&](std::uint64_t id,Hand hand)->std::optional<Entry>{auto e=*capture(id,hand);e.unique=std::uint16_t(next++);return e;});
        check(pair&&pair->parts[0].unique!=pair->parts[1].unique,"Each plain copy is captured separately");
        const auto before=encodeArsenal(s.draft);unsigned calls=0;
        check(!composeCombination(s,c,[&](std::uint64_t id,Hand hand)->std::optional<Entry>{if(++calls==2)return {};return capture(id,hand);})&&encodeArsenal(s.draft)==before,"Failed second capture leaves the entire group unchanged");
        c.catalogVersion=9;calls=0;
        check(!composeCombination(s,c,[&](auto id,auto hand){++calls;return capture(id,hand);})&&calls==0,"Stale catalog IDs are rejected before inventory access");
        c.catalogVersion=10;c.components={1,0};c.shortcut.parts[1]=emptyHandEntry(Hand::left);
        check(composeCombination(s,c,capture).has_value(),"A catalog spell can be paired with an explicit empty hand");
        s.catalog[0].entry.twoHanded=true;check(!validCombinationDraft(s,c),"Two-handed spells cannot be combined with another hand");
        s.catalog[0].entry=weapon;s.catalog[0].entry.kind=Kind::shield;
        check(!validCombinationDraft(s,c),"A shield cannot occupy the right-hand picker");
    }
    if(argc!=2)throw std::runtime_error("Expected current save fixture");
    {
        Arsenal sorted;Options options;options.autoSort[1]=true;
        auto z=item(1);z.name="Zebra";auto b=item(2);b.name="beta";auto a=item(3);a.name="Alpha";
        sorted.groups[0].add(a);sorted.groups[1].add(z);sorted.groups[1].add(b);
        EditCommand move;move.action=EditAction::move;move.group=0;move.target=1;
        check(editGroup(sorted,options,move)&&sorted.active==1&&sorted.groups[1].entries.front().same(a)&&sorted.groups[1].selected==0,"Moving into an auto-sorted group sorts its entries and follows the moved item");
        EditCommand shortcut;shortcut.action=EditAction::saveShortcut;shortcut.group=1;shortcut.shortcut=unarmedEntry();
        check(editStructure(sorted,options,shortcut)&&sorted.groups[1].entries[2].kind==Kind::unarmed&&sorted.groups[1].selected==2,"Creating a reusable shortcut honors automatic sorting");
        const auto id=sorted.groups[1].entries[2].shortcut;
        sorted.groups[2].add(z);options.autoSort[2]=true;shortcut.action=EditAction::addShortcut;shortcut.group=2;shortcut.libraryID=id;
        check(editStructure(sorted,options,shortcut)&&sorted.groups[2].entries.front().shortcut==id,"Adding a library shortcut honors automatic sorting");
        sorted.groups[2].reorder(0,1);const auto remembered=sorted.groups[2].entries[sorted.groups[2].selected].shortcut;
        shortcut.action=EditAction::sortGroup;
        check(editStructure(sorted,options,shortcut)&&sorted.groups[2].entries[sorted.groups[2].selected].shortcut==remembered,"Manual sort preserves shared-shortcut selection");
        check(!editStructure(sorted,options,shortcut),"Sorting an already sorted group does not create a change");
    }
    std::ifstream file(argv[1]);check(bool(file),"Open current save fixture");
    const std::string bytes((std::istreambuf_iterator<char>(file)),{});const auto saved=Json::parse(bytes);const auto persisted=decodeArsenal(bytes,identity);
    check(persisted.size()==23&&[&]{auto upgraded=Json::parse(encodeArsenal(persisted));upgraded.erase("shortcuts");for(auto& group:upgraded["groups"])for(auto& entry:group["entries"])entry.erase("shortcut");return upgraded==saved;}(),"Current save retains every entry, identity and selection");

    Options o;o.names[5]="Magie — Épée";o.layout=3;o.selector=2;o.shape=1;o.groupCount=6;o.positionX=.27f;o.positionY=.38f;o.keys[5]=61;o.showHints=true;o.themeId="oathvein";o.selectorShape=3;o.selectorSpacing=0;o.showLabels=false;o.keyIcons=false;o.coloredMagic=false;
    const auto settings=encodeOptions(o);check(encodeOptions(decodeOptions(settings))==settings,"Names, bindings, eight groups and independent layouts persist");
    for(unsigned preset=0;preset<3;++preset) {
        auto styled=o;layoutPreset(styled,preset);
        check(styled.keys==o.keys&&styled.names==o.names&&styled.groupCount==o.groupCount&&styled.editorKey==o.editorKey&&styled.editorModifiers==o.editorModifiers&&styled.themeId==o.themeId&&styled.keyIcons==o.keyIcons&&styled.coloredMagic==o.coloredMagic&&styled.positionX==o.positionX&&styled.scale==o.scale,"Appearance presets preserve bindings, names, group count, scale and position");
        check(encodeOptions(decodeOptions(encodeOptions(styled)))==encodeOptions(styled),"Every preset survives settings reload");
    }
    auto visualOnly=persisted;visualOnly.groups[0].entries[0].icon=Icon::fire;
    check(encodeArsenal(visualOnly)==encodeArsenal(persisted),"Presentation metadata cannot change co-save identity or format");
    auto broken=o;broken.keys[0]=14;broken.keys[1]=broken.keys[2];broken.editorKey=broken.keys[3];broken.scale=NAN;broken.spacing=999;broken.groupCount=999;broken.layout=999;
    sanitizeOptions(broken);check(broken.keys[0]!=14&&broken.keys[1]!=broken.keys[2],"Reserved/duplicate group keys sanitized");
    check(!broken.editorKey||std::find(broken.keys.begin(),broken.keys.end(),broken.editorKey)==broken.keys.end(),"Editor key cannot collide with any group");
    check(broken.scale==1&&broken.spacing==40&&broken.groupCount==8&&broken.layout==5,"Invalid layout settings bounded");
    const auto defaults=decodeOptions(encodeOptions(Options{}));
    check(defaults.editorKey==43&&defaults.editorModifiers==keyModifier::shift,"Shift + backslash is the default and survives reload");
    for(const unsigned shift:{42u,54u}) {
        const auto modifiers=keyboardModifiers([&](unsigned scan){return scan==shift;});
        check(editorShortcutMatches(defaults,43,modifiers),"Either Shift key opens the default editor shortcut");
    }
    check(!editorShortcutMatches(defaults,43,0)&&!editorShortcutMatches(defaults,42,keyModifier::shift),"Neither bare backslash nor Shift alone opens the editor");
    check(!editorShortcutMatches(defaults,43,keyModifier::shift|keyModifier::ctrl)&&!editorShortcutMatches(defaults,43,keyModifier::alt),"Unassigned modifier combinations do not open the editor");
    auto singleKey=defaults;singleKey.editorKey=68;singleKey.editorModifiers=0;
    check(editorShortcutMatches(decodeOptions(encodeOptions(singleKey)),68,0),"An explicit single-key shortcut survives reload");
    singleKey.editorKey=0;check(!editorShortcutMatches(decodeOptions(encodeOptions(singleKey)),43,keyModifier::shift),"A disabled shortcut stays disabled");
    auto invalidModifiers=defaults;invalidModifiers.editorModifiers=0xff;sanitizeOptions(invalidModifiers);
    check(invalidModifiers.editorModifiers==keyModifier::all,"Unknown keyboard modifier bits are removed");
    invalidModifiers.editorKey=0;sanitizeOptions(invalidModifiers);check(invalidModifiers.editorModifiers==0,"Clearing the editor key clears modifiers");
    for(const auto key:{59u,65u,66u}) {
        auto binding=defaults;binding.keys[0]=key;
        check(decodeOptions(encodeOptions(binding)).keys[0]==key,"F1, F7 and F8 can be group bindings");
        binding=defaults;binding.editorKey=key;
        check(decodeOptions(encodeOptions(binding)).editorKey==key,"F1, F7 and F8 can be editor bindings");
    }
    const auto complete=Json::parse(settings);
    for(const auto& [field,value]:complete.items()) {
        if(field=="quickLootKeyboard"||field=="quickLootController"||field=="controllerModifiers"||field=="groupModes"||field=="autoSort"||field=="groupUnequipHotkey"||field=="groupRemoveHotkey"||field=="groupCaptureRightHotkey"||field=="groupCaptureLeftHotkey"||field=="groupMoveHotkey"||field=="selectorReorderHotkey"||field=="selectorUnequipHotkey"||field=="selectorRemoveHotkey"||field=="language"||field=="itemIconSet"||field=="keepDrawnVisible"||field=="textScale"||field=="placementSpacing"||field=="placementFrame"||field=="placementGridSize"||
           field=="equipmentX"||field=="equipmentY"||field=="equipmentScale"||field=="equipmentTextScale"||field=="equipmentSpacing"||field=="showEquipment"||field=="hideEmptyGroups"||field=="inactiveGroups"||field=="inactiveGroupOpacity"||
           field=="equipmentDim"||field=="equipmentStyle"||field=="equipmentLayout"||field=="equipmentTextPosition"||field=="equipmentFadeDelay"||field=="equipmentIdleOpacity")continue;
        auto incomplete=complete;incomplete.erase(field);bool rejected=false;
        try{decodeOptions(incomplete.dump());}catch(const std::exception&){rejected=true;}
        check(rejected,"Incomplete settings are rejected instead of guessing missing fields");
    }
    o.editorKey=68;o.editorModifiers=keyModifier::ctrl|keyModifier::alt;
    check(editorShortcutMatches(decodeOptions(encodeOptions(o)),68,keyModifier::ctrl|keyModifier::alt),"A remapped keyboard chord survives reload");
    EditorLaunch launch;
    check(launch.poll(0,true,true)==EditorLaunch::Result::idle,"No unrequested editor launch");
    launch.request(1);
    check(launch.poll(1.1,true,false)==EditorLaunch::Result::wait&&launch.pending(),"SMF launch waits for both input and freeze release");
    check(launch.poll(1.2,true,true)==EditorLaunch::Result::open&&!launch.pending(),"SMF launch opens once when gameplay resumes");
    check(launch.poll(1.3,true,true)==EditorLaunch::Result::idle,"Handoff cannot open twice");
    launch.request(2);check(launch.poll(2.1,false,true)==EditorLaunch::Result::cancel,"Foreign menu, focus loss or invalid game cancels launch");
    check(launch.poll(2.2,true,true)==EditorLaunch::Result::idle,"Cancelled handoff never opens later");
    launch.request(3);check(launch.poll(6,true,true)==EditorLaunch::Result::cancel,"Expired handoff cannot open after a long delay");
    launch.request(7);launch.cancel();check(launch.poll(7.1,true,true)==EditorLaunch::Result::idle,"Load/reset cancellation clears pending launch");
    Arsenal a;a.groups[7].add(item(1));a.active=0;Options visible;revealSavedGroups(visible,a);check(groupVisible(visible,7),"Loading never hides populated higher groups");
    a.groups[7]={};a.active=5;visible.groupCount=3;revealSavedGroups(visible,a);check(groupVisible(visible,5),"Loading reveals remembered active group");

    EditorState state;state.open=true;state.session=2;state.revision=7;state.draft.groups[0].add(item(1));state.draft.groups[0].add(item(2));state.draft.groups[0].selected=1;
    EditCommand c;c.session=2;c.revision=7;check(matchingEdit(state,c),"Current editor command accepted");
    c.session=1;check(!matchingEdit(state,c),"Previous editor session cannot mutate a reopened editor");c.session=2;c.revision=6;check(!matchingEdit(state,c),"Stale render snapshot rejected");c.revision=7;state.open=false;check(!matchingEdit(state,c),"Closed editor rejects queued edits");state.open=true;
    const auto original=state.draft;auto draft=original;
    c.action=EditAction::remove;c.index=0;check(editGroup(draft,o,c)&&draft.groups[0].entries[draft.groups[0].selected].unique==2,"Removing another shortcut preserves remembered item");
    check(original.groups[0].entries.size()==2,"Draft edits do not mutate applied data");
    c.action=EditAction::hand;c.hand=Hand::left;c.index=0;check(editGroup(draft,o,c)&&draft.groups[0].entries[0].hand==Hand::left,"Editor can change target hand without equipping");
    draft.groups[0].add(item(2));c.index=1;check(!editGroup(draft,o,c),"Hand change cannot create a duplicate shortcut");
    draft.groups[0].entries[1].twoHanded=true;check(!editGroup(draft,o,c),"Two-handed targets cannot change hand");
    Entry shield=item(7);shield.kind=Kind::shield;check(normalizedHand(shield,Hand::right)==Hand::left,"Shield addition always targets left hand");
    Entry greatsword=item(8);greatsword.twoHanded=true;check(normalizedHand(greatsword,Hand::left)==Hand::right,"Two-handed addition uses both-hands canonical slot");
    CatalogItem catalog;catalog.entry=item(9);catalog.entry.name="Imperial Sword (Superior)";
    check(catalogMatches(catalog,"sWoRd",1)&&!catalogMatches(catalog,"sword",2),"Search ignores case and honors weapon/spell filter");
    check(catalogMatches(catalog,"",0),"Empty search shows inventory");catalog.entry=shield;
    check(catalogMatches(catalog,"",3)&&!catalogMatches(catalog,"",1),"Offhand items have their own filter");
    bool owned=false;
    check(captureEditorButton(owned,true,false)&&!owned,"A key already held on entry is blocked without claiming its release");
    check(!captureEditorButton(owned,false,false),"Pre-editor release reaches the game so movement cannot stick");
    check(captureEditorButton(owned,true,true)&&owned,"A new editor press is captured");
    check(captureEditorButton(owned,true,false)&&owned,"A held editor key remains captured");
    check(captureEditorButton(owned,false,false)&&!owned,"An editor-owned release never triggers gameplay");

    InventorySample before{2,{{11,1,true,0x14,17}}};
    auto after=before;after.count=3;after.stacks.push_back({22,1,false,0,0});
    auto match=matchIncomingCopy(before,after);check(match.valid&&!match.plain&&match.token==22,"Recover exact new instance when pickup strips identity");
    auto rebound=original;rebound.groups[0].entries[0].owner=0xff00bc38;rebound.groups[0].entries[0].unique=0;
    rebound.groups[7].add(rebound.groups[0].entries[0]);
    check(followIdentityChange(rebound,{0x1234,0xff00bc38,0x14,0,31})==2,"Observed owner/zero-ID drop pattern reconnects every shortcut");
    check(rebound.groups[0].entries[1].unique==2,"Recovery does not bind an already-owned same-form copy");
    after=before;after.count=3;match=matchIncomingCopy(before,after);check(match.valid&&match.plain,"Known incoming plain copy can be isolated from raw count increase");
    after=before;check(!matchIncomingCopy(before,after).valid,"Failed pickup with no count increase cannot reconnect");
    after.count=4;check(!matchIncomingCopy(before,after).valid,"Multiple additions are ambiguous");
    after.count=3;after.stacks[0].unique=18;check(!matchIncomingCopy(before,after).valid,"Side-effect identity changes invalidate comparison");
    after=before;after.count=3;after.stacks[0].count=2;check(!matchIncomingCopy(before,after).valid,"Merged customized stack is not guessed");
    after=before;after.count=3;after.stacks.clear();check(!matchIncomingCopy(before,after).valid,"Existing stack disappearance rejects recovery");
    after=before;after.count=3;after.stacks.push_back({22,1,false,0,0});after.stacks.push_back({23,1,false,0,0});check(!matchIncomingCopy(before,after).valid,"Multiple new extra lists reject recovery");
    check(!matchIncomingCopy(before,after,2).valid,"Multi-count pickup cannot use single-copy recovery");
    check(!matchIncomingCopy({0,{{1,1,false,0,0}}},{1,{{2,1,false,0,0}}}).valid,"Invalid inventory counts reject recovery");

    unsigned configurations=0;
    for(std::size_t count=3;count<=8;++count)for(unsigned layout=0;layout<6;++layout)for(unsigned shape=0;shape<4;++shape)for(unsigned selector=0;selector<3;++selector)for(bool labels:{false,true})for(float gap:{0.f,4.f,40.f}) {
        Options appearance;appearance.groupCount=count;appearance.layout=layout;appearance.shape=shape;appearance.selector=selector;appearance.showLabels=labels;appearance.spacing=gap;appearance.selectorSpacing=std::min(gap,32.f);
        auto g=groupGeometry(appearance);bool separated=g.tiles.size()==count;
        for(std::size_t i=0;i<count;++i)for(std::size_t j=0;j<i;++j) {
            const auto dx=std::abs(g.tiles[i].x-g.tiles[j].x),dy=std::abs(g.tiles[i].y-g.tiles[j].y);
            if(shape==1&&!labels&&(layout==2||layout==4))separated&=std::hypot(dx,dy)>=73.99f;
            else if(shape==3&&!labels&&layout==2)separated&=dx+dy>=87.99f;
            else separated&=dx+.01f>=g.tileWidth||dy+.01f>=g.tileHeight;
        }
        check(separated,"Every layout keeps group tiles apart");
        for(const auto size:{Point{800,450},Point{1280,720},Point{1920,1080},Point{3440,1440},Point{3840,2160}}) {
            const auto h=hudGeometry(appearance,size.x,size.y);
            check(h.origin.x>=0&&h.origin.y>=0&&h.origin.x+h.width*h.scale<=size.x+.1f&&h.origin.y+h.height*h.scale<=size.y+.1f,"HUD and selector stay within screen bounds");
        }
        ++configurations;
    }
    Options tight;tight.groupCount=4;tight.layout=5;tight.shape=2;tight.showLabels=false;tight.spacing=4;
    auto stb=groupGeometry(tight);
    check(stb.tiles[1].y==(stb.tiles[0].y+stb.tiles[2].y)*.5f&&stb.tiles[1].y==stb.tiles[3].y,"STB side panels straddle the top/bottom seam");
    tight.layout=4;tight.shape=1;tight.groupCount=8;const auto compact=groupGeometry(tight);tight.showLabels=true;
    check(compact.width<groupGeometry(tight).width*.8f,"Hiding labels enables substantially tighter circular packing");
    const Options normal;
    check(hudGeometry(normal,3840,2160).scale==hudGeometry(normal,1920,1080).scale*2,"4K doubles physical resolution without halving the logical HUD size");
    check(hudGeometry(normal,3440,1440).scale==hudGeometry(normal,2560,1440).scale,"Ultrawide uses height rather than width for HUD scaling");
    std::cout<<"PASS: "<<checks<<" checks (editor drafts, current save storage, settings, pickup deltas; "<<configurations<<" layout combinations at five resolutions).\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
