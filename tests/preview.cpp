#include "Hud.h"
#include "Editor.h"
#include "EditorFixture.h"
#include "UiProbe.h"
#include "Assets.h"
#include "IconPacks.h"
#include "Layout.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <filesystem>

using namespace quickslot;
int main(int argc,char** argv) {
    const std::string root=argc>1?argv[1]:".";
    const std::string output=argc>2?argv[2]:"";
    const std::string scene=argc>3?argv[3]:"carousel";
    const int width=argc>4?std::stoi(argv[4]):1600,height=argc>5?std::stoi(argv[5]):900;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){std::cerr<<SDL_GetError()<<'\n';return 1;}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto* window=SDL_CreateWindow("Quickslot — native HUD",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!window){std::cerr<<SDL_GetError()<<'\n';return 1;}
    auto gl=SDL_GL_CreateContext(window);if(!gl){std::cerr<<SDL_GetError()<<'\n';return 1;}
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    ImGui_ImplSDL2_InitForOpenGL(window,gl);ImGui_ImplOpenGL3_Init("#version 330");
    Hud hud;hud.initialize(root+"/assets/fonts");Options options;Editor editor;auto editing=fixture::editor();
    const auto upload=[](const ImageAsset& image) {
        if(!image)throw std::runtime_error("Cannot load preview image");
        GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.pixels.data());return id;
    };
    const auto texture=[&](const std::string& path){return upload(loadImageAsset(path));};
    const auto atlas=upload(loadItemAtlas(root+"/assets/icons/items.png"));hud.setIcons("quickslot","Quickslot",ImTextureID(atlas));
    std::vector<GLuint> iconTextures;
    for(const auto& pack:loadIconPacks(std::filesystem::path(root)/"assets/icons"))if(pack.id!="quickslot"){auto id=upload(loadItemAtlas(pack.atlas));iconTextures.push_back(id);hud.setIcons(pack.id,pack.name,ImTextureID(id));}
    std::vector<GLuint> themes;for(const auto& t:hud.themes().all())if(!t.atlas.empty()){const auto id=texture(t.atlas.string());themes.push_back(id);hud.setTheme(t.id,ImTextureID(id));}
    const auto keys=loadKeyAtlas(argc>7?std::filesystem::path(argv[7]):std::filesystem::path(root)/"tests/fixtures/keys");
    const auto keyTexture=keys.image?upload(keys.image):0;hud.setKeys(ImTextureID(keyTexture),keys.glyphs);
    const auto backdrop=argc>6?texture(argv[6]):0;
    ImGui::GetCurrentContext()->TestEngineHookItems=true;
    const bool editorScene=scene.starts_with("editor");
    Snapshot snapshot;snapshot.visible=true;snapshot.browsing=scene!="idle";snapshot.preview=1;snapshot.selected=1;snapshot.activeGroup=1;
    options.names[0]="Arsenal";options.names[1]="Offhand";options.names[2]="Magic";
    for(const auto& [name,kind]:std::vector<std::pair<std::string,Kind>>{{"Dawnbreaker",Kind::sword},{"Chain Lightning",Kind::spell},{"Nightingale Bow",Kind::bow}}) {
        Entry entry;entry.name=name;entry.kind=kind;entry.spell=kind==Kind::spell;
        if(entry.spell)entry.icon=Icon::shock;
        entry.hand=entry.spell?Hand::left:Hand::right;entry.twoHanded=kind==Kind::bow;
        snapshot.rows.push_back({entry,true,false,{}});
    }
    snapshot.groups[0]={snapshot.rows[0].entry,5,true,true};
    snapshot.groups[1]={snapshot.rows[1].entry,3,true,false};
    if(scene=="empty"){snapshot.rows.clear();snapshot.groups[1]={};}
    if(scene=="missing"){snapshot.rows[1].available=false;snapshot.rows[1].entry.name="Enchanted Dragonbone Greatsword of the Inferno";snapshot.rows[1].entry.kind=Kind::greatsword;snapshot.rows[1].entry.icon=Icon::greatsword;}
    if(scene=="single") {snapshot.rows.resize(1);snapshot.selected=snapshot.preview=0;}
    if(scene=="toast"){snapshot.browsing=false;snapshot.notice="Unequipped — Left hand: Chain Lightning";snapshot.noticeUntil=1e12;}
    if(scene=="scaled")options.scale=1.5;
    if(scene.find("diamond")!=std::string::npos){options.layout=2;options.shape=1;options.selector=1;options.groupCount=4;}
    if(scene.find("cross")!=std::string::npos){options.layout=3;options.selector=0;options.groupCount=5;}
    if(scene.find("circle")!=std::string::npos){options.layout=4;options.shape=1;options.selector=2;options.groupCount=8;}
    if(scene=="vertical"){options.layout=1;options.selector=1;options.groupCount=4;}
    if(scene.starts_with("oathvein")) {
        options.layout=2;options.shape=1;options.selector=1;options.groupCount=4;options.scale=1.3f;
        options.names[0]="Arsenal";options.names[1]="Offhand";options.names[2]="Destruction";options.names[3]="Restoration";
        snapshot.activeGroup=0;snapshot.rows.clear();
        for(const auto i:{0,1,2,3})snapshot.rows.push_back({editing.catalog[i].entry,true,i==0,{}});
        snapshot.selected=0;snapshot.preview=1;snapshot.groups[0]={snapshot.rows[0].entry,4,true,true};
        auto shield=editing.catalog[6].entry;snapshot.groups[1]={shield,2,true,true};
        auto flames=editing.catalog[4].entry;flames.icon=Icon::fire;snapshot.groups[2]={flames,3,true,false};
        auto healing=editing.catalog[5].entry;healing.icon=Icon::restoration;snapshot.groups[3]={healing,1,true,false};
        if(scene=="oathvein-idle")snapshot.browsing=false;
        if(scene=="oathvein-hints")options.showHints=true;
    }
    if(scene=="circle") {
        snapshot.rows.clear();for(const auto& choice:editing.catalog)snapshot.rows.push_back({choice.entry,true,false,{}});
        snapshot.rows[4].entry.icon=Icon::fire;snapshot.rows[5].entry.icon=Icon::restoration;snapshot.rows[8].entry.icon=Icon::shock;
        for(std::size_t i=0;i<8;++i)snapshot.groups[i]={snapshot.rows[i].entry,2,true,i==0};
    }
    if(scene.starts_with("theme-")) {
        options.groupCount=4;options.names={"Destruction","Arsenal","Restoration","Offhand","Group 5","Group 6","Group 7","Group 8"};
        const unsigned preset=scene.find("diamond")!=std::string::npos?1:scene.find("circles")!=std::string::npos?2:scene.find("silver")!=std::string::npos?3:0;
        options.themeId=scene.find("silver")!=std::string::npos?"silver":"oathvein";layoutPreset(options,std::min(preset,2u));options.scale=1.1f;
        snapshot.activeGroup=1;snapshot.rows.clear();
        for(const auto i:{0,1,2,3})snapshot.rows.push_back({editing.catalog[i].entry,true,i==0,{}});
        snapshot.selected=0;snapshot.preview=1;
        auto flames=editing.catalog[4].entry;flames.icon=Icon::fire;snapshot.groups[0]={flames,3,true,false};
        snapshot.groups[1]={snapshot.rows[0].entry,4,true,true};
        auto healing=editing.catalog[5].entry;healing.icon=Icon::restoration;snapshot.groups[2]={healing,1,true,false};
        snapshot.groups[3]={editing.catalog[6].entry,2,true,true};
        if(scene.find("idle")!=std::string::npos)snapshot.browsing=false;
        if(scene.find("eight")!=std::string::npos) {
            options.groupCount=8;
            for(std::size_t i=4;i<8;++i)snapshot.groups[i]={editing.catalog[i].entry,2,true,false};
        }
        if(scene.find("widekey")!=std::string::npos){options.keys[0]=57;options.keys[1]=68;}
        if(scene.find("ring")!=std::string::npos){options.selector=2;options.selectorShape=3;for(unsigned i=4;i<8;++i)snapshot.rows.push_back({editing.catalog[i].entry,true,false,{}});}
        if(scene.find("missing")!=std::string::npos){snapshot.groups[0].available=false;snapshot.rows[1].available=false;snapshot.rows[1].entry.name="Enchanted Dragonbone Greatsword of the Inferno";}
    }
    if(scene.starts_with("pack-")){options.groupCount=4;layoutPreset(options,0);options.themeId=scene.substr(5);}
    if(scene=="free-selector"){options.groupCount=4;layoutPreset(options,1);options.selectorDock=4;options.selectorX=.45f;options.selectorY=.35f;}
    if(scene.find("fallback")!=std::string::npos){hud.setKeys(0,{});hud.setTheme(options.themeId,0);options.keys[1]=68;}
    if(scene=="editor-layout"||scene=="editor-groups"||scene=="editor-selector"||scene=="editor-position-grid"||scene=="editor-presets"){editing.options.groupCount=4;layoutPreset(editing.options,0);}
    if(scene=="editor-empty"){editing.draft.groups[0]={};fixture::rows(editing);}
    if(scene.starts_with("v04-")) {
        options.groupCount=4;layoutPreset(options,0);options.scale=1.15f;
        options.names[0]="Arsenal";options.names[1]="Supplies";options.names[2]="Voice";options.names[3]="Scrolls";
        const auto row=[](const char* name,Kind kind,Icon icon,int count,bool ready=true,float cooldown=0) {
            Entry e;e.name=name;e.kind=kind;e.icon=icon;e.spell=kind==Kind::power||kind==Kind::lesserPower;
            return Row{e,ready,false,{},count,cooldown};
        };
        snapshot.rows={row("Potion of Healing",Kind::potion,Icon::health,12),row("Potion of Magicka",Kind::potion,Icon::magicka,6),row("Potion of Stamina",Kind::potion,Icon::stamina,8)};
        snapshot.activeGroup=1;snapshot.selected=snapshot.preview=0;
        snapshot.groups[0]={editing.catalog[0].entry,4,true,true};snapshot.groups[1]={snapshot.rows[0].entry,3,true,false,12};
        auto voice=row("Unrelenting Force",Kind::shout,Icon::shout,0,true,12.4f);
        snapshot.groups[2]={voice.entry,3,true,true,0,12.4f};auto scroll=row("Scroll of Fireball",Kind::scroll,Icon::scroll,3);
        snapshot.groups[3]={scroll.entry,2,true,false,3};
        if(scene=="v04-empty"){snapshot.rows[0].available=false;snapshot.rows[0].quantity=0;}
        if(scene=="v04-food")snapshot.rows={row("Vegetable Soup",Kind::food,Icon::food,4),row("Weak Poison",Kind::poison,Icon::poison,9),row("Potion of Healing",Kind::potion,Icon::health,12)};
        if(scene=="v04-poison"){snapshot.rows={row("Weak Poison",Kind::poison,Icon::poison,9),row("Potion of Healing",Kind::potion,Icon::health,12)};}
        if(scene=="v04-voice") {
            snapshot.activeGroup=2;voice.equipped=true;snapshot.rows={voice,row("Battle Cry",Kind::power,Icon::greater_power,0),row("Night Eye",Kind::lesserPower,Icon::lesser_power,0)};
            snapshot.rows[1].powerUsed=true;
        }
        if(scene=="v04-power"){snapshot.activeGroup=2;auto power=row("Battle Cry",Kind::power,Icon::greater_power,0);power.powerUsed=true;snapshot.rows={power,voice};}
        if(scene=="v04-scroll"){snapshot.activeGroup=3;scroll.entry.hand=Hand::left;snapshot.rows={scroll,row("Scroll of Blizzard",Kind::scroll,Icon::scroll,1)};snapshot.rows[1].entry.twoHanded=true;}
        if(scene=="v04-circles"){layoutPreset(options,2);options.themeId="silver";}
        if(scene=="v04-ring"){options.selector=2;options.selectorShape=1;options.selectorSpacing=0;}
    }
    if(scene=="editor-consumables") {
        editing.catalog.erase(editing.catalog.begin(),editing.catalog.begin()+10);
        editing.draft.groups[0]={};for(unsigned i=0;i<4;++i)editing.draft.groups[0].add(editing.catalog[i].entry);
        editing.options.groupCount=4;layoutPreset(editing.options,0);editing.options.names[0]="Supplies";fixture::rows(editing);
    }
    if(scene.starts_with("v05-")||scene.starts_with("v06-")||scene=="editor-smart"||scene=="editor-combinations"||scene.starts_with("editor-behavior")) {
        options.groupCount=4;layoutPreset(options,0);options.scale=1.2f;
        options.names={"Arsenal","Supplies","Voice","Offhand","Group 5","Group 6","Group 7","Group 8"};
        auto rule=smartEntry(0,0,2);rule.exclusions={{editing.potions.back().entry.form,0}};
        Entry combo;combo.kind=Kind::combo;combo.name="Sword & shield";combo.parts={editing.catalog[0].entry,editing.catalog[6].entry};combo.parts[1].hand=Hand::left;
        editing.draft.groups[0].add(combo);editing.draft.groups[1].add(rule);editing.options=options;
        editing.options.names[1]="Supplies";editing.soulsSlowdown=true;fixture::rows(editing);
        editing.rows[1].back()={rule,true,false,"Potion of Vigorous Healing · 7 remaining",7};
        editing.rows[0].back()={combo,true,true,"Both hands equipped"};
        snapshot.rows={{rule,true,false,"Potion of Vigorous Healing · 7 remaining",7},{smartEntry(1,0,0),true,false,"Potion of Magicka · 4 remaining",4}};
        snapshot.activeGroup=1;snapshot.selected=snapshot.preview=0;snapshot.groups[1]={rule,2,true,false,7};snapshot.groups[0]={combo,4,true,true};
        if(scene=="v05-combination"){snapshot.activeGroup=0;snapshot.rows={{combo,true,true,"Both hands equipped"},{editing.catalog[1].entry,true,false,{}}};}
        if(scene=="v05-one-group"){options.groupCount=1;options.groupOrder={1,0,2,3,4,5,6,7};options.layout=4;options.shape=1;options.showLabels=false;}
        if(scene=="v05-select"){snapshot.rows={editing.rows[1].back()};snapshot.notice="Selected · tap 2 to use one";snapshot.noticeUntil=1e12;}
        if(scene=="editor-smart")editing.draft.active=1;
    }
    if(scene=="editor-behavior-pause")editing.options.selectionSpeed=0;
    if(scene.starts_with("edge-")) {
        options.groupCount=4;layoutPreset(options,0);options.scale=1.25f;
        options.positionX=scene.find("left")!=std::string::npos?0.f:1.f;
        options.positionY=scene.find("top")!=std::string::npos?0.f:1.f;
        snapshot.activeGroup=0;snapshot.rows.clear();
        for(const auto i:{0,1,2})snapshot.rows.push_back({editing.catalog[i].entry,true,i==1,{}});
        snapshot.groups[0]={snapshot.rows[1].entry,3,true,true};snapshot.groups[1]={editing.catalog[4].entry,2,true,false};
        snapshot.groups[2]={editing.catalog[6].entry,2,true,true};snapshot.groups[3]={editing.catalog[5].entry,1,true,false};
        if(scene.ends_with("idle")){snapshot.browsing=false;snapshot.notice="Selected · tap 1 to equip";snapshot.noticeUntil=1e12;}
    }
    if(scene.starts_with("v06-")) {
        auto& row=snapshot.rows[0];const auto* bottle=choosePotion(row.entry,editing.potions,60);const auto feedback=potionFeedback(row.entry,bottle,60);row.feedback=feedback.reason+"\n"+feedback.estimate;
        if(scene=="v06-smart-ring"){options.selector=2;options.selectorSpacing=32;}
        if(scene=="v06-hidden-keys")options.showKeys=false;
        if(scene=="v06-ammo"){
            snapshot.rows.clear();snapshot.activeGroup=1;options.names[1]="Ammo";
            for(unsigned i=0;i<3;++i){Entry e;e.kind=Kind::ammo;e.icon=i==1?Icon::bolt:Icon::arrow;e.name=i==0?"Steel Arrow":i==1?"Dwarven Bolt":"Ebony Arrow";snapshot.rows.push_back({e,true,i==0,{},int(32+i*20)});}
            snapshot.groups[1]={snapshot.rows[0].entry,3,true,true,32};
        }
    }
    if(scene=="editor-position-grid"){editing.options.placementGrid=editing.options.placementSnap=true;editing.options.positionX=.75f;editing.options.positionY=.8f;}
    if(scene=="editor-presets")editing.presets={"Combat - Oathvein","Nordic circles"};
    if(scene=="v061-hand-actions"||scene=="editor-hand-actions") {
        layoutPreset(options,0);options.groupCount=3;options.names[0]="Hands";
        snapshot.rows.clear();snapshot.activeGroup=0;snapshot.preview=snapshot.selected=1;
        snapshot.rows={{emptyHandEntry(Hand::left),true,false,{}},{unarmedEntry(),true,true,{}},{emptyHandEntry(Hand::right),true,false,{}}};
        for(unsigned i=0;i<3;++i)snapshot.groups[i]={snapshot.rows[i].entry,1,true,i==1};
        editing.options=options;for(const auto& row:snapshot.rows)editing.draft.groups[0].add(row.entry);fixture::rows(editing);
    }
    if(scene.find("controller")!=std::string::npos) {
        editing.controller=true;snapshot.controller=true;options.groupCount=editing.options.groupCount=4;
        layoutPreset(options,0);layoutPreset(editing.options,0);
        options.controllerGroups[1]={PadButton::y,PadButton::lb};
        if(scene.find("ps")!=std::string::npos)options.controllerStyle=editing.options.controllerStyle=1;
    }
    if(scene.starts_with("v081-")) {
        layoutPreset(options,2);options.themeId="oathvein";options.groupCount=4;
        options.positionX=.85f;options.positionY=.85f;options.selectorDock=2;
        options.names={"Arsenal","Offhand","Supplies","Voice","Group 5","Group 6","Group 7","Group 8"};
        snapshot.activeGroup=0;snapshot.rows.clear();
        for(const auto i:{0,1,2,3,4,5,6})snapshot.rows.push_back({editing.catalog[i].entry,true,i==0,{}});
        snapshot.rows.back().available=false;
        snapshot.preview=2;snapshot.selected=0;
        if(scene.find("vertical")!=std::string::npos)options.selector=3;
        if(scene.find("list")!=std::string::npos){options.selector=4;options.layout=1;options.shape=0;}
        if(scene.find("cards")!=std::string::npos){options.layout=1;options.shape=0;options.showCardNames=false;}
        if(scene.find("smart")!=std::string::npos) {
            auto rule=smartEntry(0,0,2);snapshot.rows[2]={rule,true,false,"Potion of Minor Healing · 10 remaining",10};
            snapshot.rows[2].feedback="Health full\nNo potion needed";
        }
        if(scene.find("ring")!=std::string::npos){options.selector=2;options.selectorSpacing=32;}
        if(scene.find("single")!=std::string::npos){snapshot.rows={snapshot.rows[2]};snapshot.preview=snapshot.selected=0;}
        if(scene.find("empty")!=std::string::npos){snapshot.rows.clear();snapshot.preview=snapshot.selected=0;}
        if(scene.find("hints")!=std::string::npos)options.showHints=true;
        for(std::size_t i=0;i<4&&i<snapshot.rows.size();++i)snapshot.groups[i]={snapshot.rows[i].entry,7,true,i==0,snapshot.rows[i].quantity};
    }
    if(scene=="editor-unicode") {
        editing.options.themeId="untarnished";
        const char* names[]{"Стальной меч","火球術","治癒の手","치유의 손","Épée enchantée"};
        for(unsigned i=0;i<5;++i)editing.catalog[i].entry.name=names[i];
        editing.draft.groups[0]={};for(unsigned i=0;i<5;++i)editing.draft.groups[0].add(editing.catalog[i].entry);
        editing.options.names[0]="装備 · Снаряжение";fixture::rows(editing);
    }
    if(scene.starts_with("editor-library")) {
        editing.options.themeId="oathvein";
        Entry combo;combo.kind=Kind::combo;combo.name="Fire and healing";combo.parts={editing.catalog[4].entry,editing.catalog[5].entry};combo.parts[1].hand=Hand::left;
        const auto id=rememberShortcut(editing.draft,combo);addShortcut(editing.draft,0,id);addShortcut(editing.draft,1,id);
        for(const auto& entry:{smartEntry(0,0,2),smartEquipmentEntry(false,unsigned(WeaponFamily::mace)),smartEquipmentEntry(true),unarmedEntry(),emptyHandEntry(Hand::left)})rememberShortcut(editing.draft,entry);
        fixture::rows(editing);
    }
    if(scene=="editor-position-bounds"||scene=="editor-position-custom") {
        editing.options.themeId="oathvein";editing.options.placementGrid=true;editing.options.placementSnap=true;
        editing.options.placementFrame=3;editing.options.placementSpacing=2;editing.options.positionX=.8f;editing.options.positionY=.7f;
        editing.options.groupCount=4;layoutPreset(editing.options,1);
        if(scene=="editor-position-custom"){editing.options.placementSpacing=3;editing.options.placementGridSize=48;}
    }
    if(scene.starts_with("text-")) {
        options.themeId="oathvein";options.scale=.55f;options.showHints=true;options.showLabels=true;options.positionX=.6f;options.positionY=.8f;
        options.groupCount=4;options.shape=1;options.layout=2;
        if(scene.find("large")!=std::string::npos)options.textScale=1.5f;
        if(scene.find("ring")!=std::string::npos)options.selector=2;
        if(scene.find("list")!=std::string::npos)options.selector=4;
    }
    std::vector<std::string> previewClicks;
    if(scene.starts_with("equipped-")) {
        options.themeId="oathvein";layoutPreset(options,2);options.groupCount=4;options.showEquipment=true;options.inactiveGroups=1;
        options.positionX=.9f;options.positionY=.8f;options.showCardNames=true;
        snapshot.browsing=false;snapshot.equipment=editing.equippedDisplay;snapshot.selected=snapshot.preview=0;
        snapshot.activeGroup=0;snapshot.rows={{editing.catalog[0].entry,true,true}};
        snapshot.groups[0]={editing.catalog[0].entry,4,true,true};
        snapshot.groups[1]={editing.catalog[6].entry,2,true,true};
        snapshot.groups[2]={editing.catalog[4].entry,2,true,false};
        snapshot.groups[3]={editing.catalog[10].entry,2,true,false,7};snapshot.groups[3].consumable=true;
        if(scene.find("twohand")!=std::string::npos) {
            auto e=editing.catalog[7].entry;e.kind=Kind::greatsword;e.twoHanded=true;e.icon=Icon::greatsword;e.name="Steel Greatsword";
            snapshot.equipment.hands={Row{e,true,true},Row{e,true,true}};
            snapshot.groups[0]={e,4,true,true};
            snapshot.groups[1].equipped=false;
        }
        if(scene.find("hide")!=std::string::npos)options.inactiveGroups=2;
        if(scene.find("silver")!=std::string::npos)options.themeId="silver";
        if(scene.find("nordic")!=std::string::npos)options.themeId="nordic";
        if(scene.find("small")!=std::string::npos){options.scale=.55f;options.textScale=1.5f;}
        if(scene.find("circle")!=std::string::npos)options.equipmentStyle=1;
        if(scene.find("diamond")!=std::string::npos)options.equipmentStyle=2;
        if(scene.find("square")!=std::string::npos)options.equipmentStyle=3;
        if(scene.find("text")!=std::string::npos)options.equipmentStyle=4;
        if(scene.find("horizontal")!=std::string::npos)options.equipmentLayout=1;
        if(scene.find("top")!=std::string::npos)options.equipmentTextPosition=0;
        if(scene.find("bottom")!=std::string::npos)options.equipmentTextPosition=1;
        if(scene.find("left")!=std::string::npos)options.equipmentTextPosition=2;
        if(scene.find("only")!=std::string::npos)options.showIdle=false;
        if(scene.find("tight")!=std::string::npos)options.equipmentSpacing=0;
        if(scene.find("loose")!=std::string::npos)options.equipmentSpacing=4;
        if(scene.find("faded")!=std::string::npos){options.equipmentDim=true;options.equipmentFadeDelay=0;}
        if(scene.find("used")!=std::string::npos)snapshot.equipment.voice=Row{editing.catalog[14].entry,true,true,{},0,0,true};
    }
    if(scene=="editor-equipped"){editing.options.showEquipment=true;editing.options.themeId="oathvein";previewClicks={"Appearance","Equipment"};}
    if(scene=="editor-equipped-spacing"){editing.options.showEquipment=true;editing.options.themeId="oathvein";editing.options.equipmentStyle=1;editing.options.equipmentSpacing=4;previewClicks={"Appearance","Equipment"};}
    if(scene=="editor-equipped-layout"){editing.options.showEquipment=true;editing.options.themeId="oathvein";previewClicks={"Appearance","Equipment","##equipment style","Circles","##equipment layout","Horizontal","##equipment text","Bottom"};}
    if(scene=="editor-cycle")previewClicks={"##group mode"};
    if(scene=="editor-library")previewClicks={"##filter","Shortcuts"};
    if(scene=="editor-library-edit")previewClicks={"##filter","Shortcuts","##candidate","Edit shortcut"};
    if(scene=="editor-controller")previewClicks={"Controls","Controller"};
    if(scene=="editor-controller-actions")previewClicks={"Controls","Controller","Browsing actions","Reset controller actions"};
    if(scene=="editor-keyboard-actions")previewClicks={"Controls","Keyboard","Remap actions…"};
    if(scene=="editor-keyboard"||scene=="editor-keyboard-eight")previewClicks={"Controls","Keyboard"};
    if(scene=="editor-keyboard-eight")editing.options.groupCount=Arsenal::count;
    if(scene=="editor-controller-keyboard")previewClicks={"Groups##main","Items"};
    if(scene=="editor-controller-position"){editing.options.placementGrid=true;previewClicks={"Appearance","Groups","Position"};}
    if(scene=="editor-layout")previewClicks={"Appearance"};
    if(scene=="editor-groups")previewClicks={"Appearance","Groups"};
    if(scene.starts_with("editor-spacing-")) {
        editing.options.themeId="oathvein";editing.options.groupCount=4;layoutPreset(editing.options,1);editing.options.scale=1.5f;
        editing.options.spacing=scene.find("loose")!=std::string::npos?40:0;previewClicks={"Appearance","Groups"};
        if(scene.find("circles")!=std::string::npos){editing.options.shape=1;editing.options.layout=4;}
        if(scene.find("cross")!=std::string::npos){editing.options.shape=2;editing.options.layout=5;}
    }
    if(scene=="editor-selector")previewClicks={"Appearance","Selector"};
    if(scene=="editor-position-grid"||scene=="editor-position-bounds"||scene=="editor-position-custom")previewClicks={"Appearance","Position"};
    if(scene=="editor-presets")previewClicks={"Presets"};
    if(scene=="editor-hand-actions")previewClicks={"New","Hand action"};
    if(scene.starts_with("editor-behavior"))previewClicks={"Controls"};
    if(scene=="editor-smart-weapon")previewClicks={"New","Smart weapon"};
    if(scene=="editor-smart-ammo")previewClicks={"New","Smart ammo"};
    if(scene=="editor-smart-shield")previewClicks={"New","Smart shield"};
    if(scene=="editor-smart-combination")previewClicks={"New","Combination","##right component","Best Sword","##left component","Best Shield"};
    if(scene=="editor-smart-hand-picker")previewClicks={"New","Combination","##left component"};
        if(scene=="editor-language")previewClicks={"Appearance"};
    if(scene=="editor-visibility")previewClicks={"Appearance","Visibility","##widget visibility","Hide when idle"};
    if(scene=="editor-visibility-modes")previewClicks={"Appearance","Visibility","##widget visibility"};
    if(scene=="editor-smart")previewClicks={"New","Smart potion"};
    if(scene=="editor-combinations")previewClicks={"New","Combination","##right component","Healing","Both hands"};
    if(scene=="editor-hand-picker")previewClicks={"New","Combination","##right component"};
    if(scene=="editor-group-behavior")previewClicks={"##group mode"};
    EditHistory history;history.begin(editing.draft,editing.options);
    bool running=true;int frames=0,previewStep=0,scrollAttempts=0,settled=0,keyboardStep=0;ImVec2 tabPoint{};
    while(running) {
        SDL_Event event;while(SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if(event.type==SDL_QUIT)running=false;
            if(event.type==SDL_KEYDOWN&&event.key.keysym.sym==SDLK_ESCAPE)running=false;
            if(event.type==SDL_KEYDOWN&&event.key.keysym.sym==SDLK_1)snapshot.browsing=true;
            if(event.type==SDL_KEYUP&&event.key.keysym.sym==SDLK_1){snapshot.selected=snapshot.preview;snapshot.browsing=false;}
            if(event.type==SDL_MOUSEWHEEL&&!snapshot.rows.empty()) {
                const int count=int(snapshot.rows.size());snapshot.preview=std::size_t((int(snapshot.preview)-event.wheel.y%count+count)%count);
            }
        }
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();
        if(editorScene){editor.beginSession(editing);
            const int click=previewStep/4,phase=previewStep%4;bool advance=true;
            if(click<int(previewClicks.size())){
                if(phase==1){
                    const auto& target=previewClicks[std::size_t(click)];
                    if(probe::find(target).empty()) {
                        auto& popups=ImGui::GetCurrentContext()->OpenPopupStack;
                        if(popups.empty()||!popups.back().Window||++scrollAttempts>24)throw std::runtime_error("Preview target unavailable: "+target);
                        auto* popup=popups.back().Window;ImGui::SetScrollY(popup,popup->Scroll.y+popup->InnerClipRect.GetHeight()*.4f);advance=false;
                    }else {
                        const auto item=probe::one(target);auto* owner=ImGui::FindWindowByName(item.window.c_str());
                        if(owner&&!owner->InnerClipRect.Contains(item.rect)) {
                            if(++scrollAttempts>24)throw std::runtime_error("Cannot scroll to preview target: "+target);
                            ImGui::SetScrollY(owner,owner->Scroll.y+item.rect.GetCenter().y-owner->InnerClipRect.GetCenter().y);advance=false;
                        }else {tabPoint=item.rect.GetCenter();io.AddMousePosEvent(tabPoint.x,tabPoint.y);scrollAttempts=0;}
                    }
                }
                if(phase==2)io.AddMouseButtonEvent(0,true);
                if(phase==3)io.AddMouseButtonEvent(0,false);
                if(advance)++previewStep;
            }else if(scene=="editor-controller-keyboard"&&keyboardStep<10) {
                std::vector<EditorInput> events;
                if(keyboardStep==0||keyboardStep==1)events.push_back({EditorInputType::pad,unsigned(PadButton::up),keyboardStep==0});
                if(keyboardStep==4||keyboardStep==5)events.push_back({EditorInputType::pad,unsigned(PadButton::a),keyboardStep==4});
                editor.input(events);++keyboardStep;
            }else ++settled;
        }
        probe::clear();ImGui::NewFrame();
        auto* background=ImGui::GetBackgroundDrawList();const auto size=io.DisplaySize;
        background->AddRectFilledMultiColor({0,0},size,IM_COL32(41,54,60,255),IM_COL32(27,37,39,255),IM_COL32(11,17,19,255),IM_COL32(20,27,29,255));
        if(backdrop)background->AddImage(ImTextureID(backdrop),{0,0},size);
        if(scene.find("bright")!=std::string::npos)background->AddRectFilled({0,0},size,IM_COL32(230,231,228,255));

        if(editorScene){for(const auto& command:editor.draw(editing,hud,double(SDL_GetTicks64())/1000.))fixture::apply(editing,command,history);}
        else hud.draw(snapshot,options,double(SDL_GetTicks64())/1000.);
        ImGui::Render();int fw,fh;SDL_GL_GetDrawableSize(window,&fw,&fh);glViewport(0,0,fw,fh);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if(!output.empty()&&(editorScene?settled>=8:frames>=16)) {
            std::vector<unsigned char> pixels(fw*fh*4),flipped(pixels.size());
            glReadPixels(0,0,fw,fh,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            for(int y=0;y<fh;++y)std::copy_n(pixels.data()+(fh-1-y)*fw*4,fw*4,flipped.data()+y*fw*4);
            auto* surface=SDL_CreateRGBSurfaceWithFormatFrom(flipped.data(),fw,fh,32,fw*4,SDL_PIXELFORMAT_RGBA32);
            if(!surface||SDL_SaveBMP(surface,output.c_str())!=0){std::cerr<<SDL_GetError()<<'\n';return 2;}
            SDL_FreeSurface(surface);running=false;std::cout<<"Rendered "<<scene<<" at "<<fw<<'x'<<fh<<" to "<<output<<'\n';
        }
        SDL_GL_SwapWindow(window);++frames;
    }
    glDeleteTextures(1,&atlas);for(const auto t:iconTextures)glDeleteTextures(1,&t);for(const auto t:themes)glDeleteTextures(1,&t);glDeleteTextures(1,&keyTexture);if(backdrop)glDeleteTextures(1,&backdrop);
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);SDL_Quit();
}
