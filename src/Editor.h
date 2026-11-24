#pragma once
#include "EditorModel.h"
#include "Hud.h"
#include "ThemeWatcher.h"
#include <array>
#include <filesystem>
#include <map>

namespace quickslot {
std::string scanName(unsigned scan);
class Editor {
    std::uint64_t session_=0,catalogVersion_=0,restoreSerial_=0;
    std::array<char,128> search_{};
    std::array<std::array<char,128>,Arsenal::count> names_{};
    struct KeyPress {unsigned scan,modifiers;};
    std::vector<KeyPress> downs_;
    std::vector<PadButton> padDowns_;
    std::array<bool,unsigned(PadButton::count)> padKeys_{};
    bool controllerMode_=false,vrMode_=false,pointerMode_=false,pointerDown_=false;
    ImGuiID returnTextFocus_=0;
    ImVec2 pointerAxis_{};
    bool dismissingPopup_=false;
    bool textUpper_=false;
    std::array<char,128> padText_{};
    std::array<bool,256> keys_{};
    std::uint64_t candidate_=0;
    std::size_t selected_=0;
    // Commands execute on the game thread; follow entries when the reordered snapshot arrives.
    struct OrderSelection {
        std::uint64_t revision=0;
        std::size_t group=0;
        std::optional<Entry> selected,shortcut;
        bool added=false;
    };
    std::optional<OrderSelection> orderSelection_;
    int filter_=0,binding_=-1,tab_=0,placementTarget_=0,controlsPage_=0;
    struct PadDraft {PadBinding original,value;};
    std::map<unsigned,PadDraft> padDrafts_;
    bool placement_=false,previewOpen_=true;
    int placementPanelTarget_=-1;
    // The group tile or equipment cell, by display position, that Free placement controls work on.
    std::size_t placementTile_=0,placementEquipmentSlot_=0;
    bool placementPanelRight_=false;
    bool peekHud_=false,peekRight_=false;
    int previewBackdrop_=0,previewState_=0;
    std::string hint_;
    ImVec2 mouse_{},dragStart_{},nudge_{};
    // Position mode: dragging re-anchors to the nearest of the nine anchors until one is chosen by hand.
    bool autoAnchor_=true;
    float scaleStart_=1,scaleReach_=1;
    int appearancePage_=0,previewMode_=0,previewFittedMode_=-1,exportScope_=0;
    float previewZoom_=100;
    unsigned presetParts_=1;
    std::uint64_t presetCheckRevision_=~0ull;
    const void* presetCheckInfo_=nullptr;
    bool presetModified_=false;
    std::array<char,65> presetName_{};
    // A preset file name or a built-in preset id.
    std::string presetSelected_;
    // The preset name the save scope was last matched to.
    std::string presetScopeName_;
    Entry shortcut_=smartEntry(0,0,0);
    std::optional<std::size_t> shortcutIndex_;
    std::uint32_t librarySelection_=0,editingLibrary_=0;
    std::size_t shortcutGroup_=Arsenal::count;
    std::array<char,128> comboName_{};
    int builder_=0;
    std::array<std::array<char,128>,2> handSearch_{};
    std::array<std::uint64_t,2> comboCandidates_{};
    // The outfit being built: the item list entry behind each part, the selected part and the piece search.
    std::vector<std::uint64_t> outfitCandidates_;
    std::size_t outfitSelected_=0;
    std::array<char,128> pieceSearch_{};
    std::uint64_t shortcutRevision_=0;
    bool reloadThemesRequested_=false,watchThemes_=false;
    double themesReloadedAt_=-10;
    ThemeWatcher themeWatcher_;
public:
    void beginSession(const EditorState&);
    void input(const std::vector<EditorInput>&);
    bool controllerPointer() const{return pointerMode_;}
    // Returns true before NewFrame when a manual reload or watched theme directory edit is due.
    bool consumeThemeReloadRequest(double now,const std::filesystem::path& themesDir,bool editorOpen) {
        if(reloadThemesRequested_) {
            reloadThemesRequested_=false;
            themeWatcher_.sync(themesDir,now);
            themesReloadedAt_=now;
            return true;
        }
        if(editorOpen&&watchThemes_&&tab_==1&&appearancePage_==0&&themeWatcher_.poll(themesDir,now)) {
            themesReloadedAt_=now;
            return true;
        }
        return false;
    }
    std::vector<EditCommand> draw(const EditorState&,Hud&,double now);
};
}
