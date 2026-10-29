#pragma once
#include "Model.h"
#include "EquipmentAction.h"
#include "ConsumableUse.h"
#include "SelectionTime.h"
#include "LiveEditor.h"
#include "Transfer.h"
#include "EditorLaunch.h"
#include "EditorTextInput.h"
#include "MenuFramework.h"
#include "Controller.h"
#include "GameplayMenus.h"
#include "HudMenuState.h"
#include "VoiceAction.h"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <array>

namespace quickslot {
inline constexpr char menuName[]="QuickslotHUD";
inline constexpr char editorMenuName[]="QuickslotEditor";
double clockNow();
std::filesystem::path dataDirectory();
std::vector<EditCommand> renderHud(const Snapshot&,const Options&,const EditorState*,const std::vector<EditorInput>&);
bool renderHasFocus();
class Game final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                   public RE::BSTEventSink<RE::TESContainerChangedEvent>,
                   public RE::BSTEventSink<RE::TESUniqueIDChangeEvent>,
                   public RE::BSTEventSink<RE::TESEquipEvent>,
                   public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    std::recursive_mutex mutex_;
    Arsenal arsenal_;
    Selection selection_;
    GroupRecall groupRecall_;
    Snapshot snapshot_;
    Options options_;
    EditorState editor_;
    EditHistory editHistory_;
    SettingsAutosave autosave_;
    std::string settingsError_;
    struct NativeCandidate {RE::TESBoundObject* object=nullptr;RE::ExtraDataList* extra=nullptr;bool plain=false;Entry entry;};
    std::vector<NativeCandidate> candidates_;
    std::vector<EditorInput> editorInput_;
    bool editorRequested_=false,catalogDirty_=false,textInputRecoveryLogged_=false;
    EditorLaunch editorLaunch_;
    EditorTextInput editorTextInput_;
    std::string frameworkMessage_;
    std::array<bool,256> keyboardOwned_{};
    std::array<bool,16> mouseOwned_{};
    ControllerInput controller_;
    bool controllerLast_=false,selectionController_=false,controllerConnected_=false;
    std::array<bool,2> stickOwned_{};
    VoiceAction voice_;
    RE::FormID voiceForm_=0;
    bool nativeVoiceDown_=false;
    RE::INPUT_DEVICE voiceDevice_=RE::INPUT_DEVICE::kKeyboard;
    unsigned voiceCode_=0;
    void cycleSelection(int);
    void beginSelection(std::size_t,bool);
    void finishSelection();
    bool consumeController(RE::InputEvent*);
    void startVoice(const Entry&);
    void progressVoice();
    void stopVoice();
    bool initialized_=false,ready_=false,dirty_=true;
    HudMenuState hudMenu_;
    double customMenuProbeAt_=0;
    std::atomic_bool tickQueued_{false};
    struct Pending {Selection::Commit commit;double deadline=0;EquipmentPhase phase=EquipmentPhase::equipping;int pairStep=-1;bool remove=false,castVoice=false;std::optional<Entry> action;};
    std::optional<Pending> pending_;
    struct Released {Selection::Commit commit;bool unequip=false;};
    std::optional<Released> released_;
    ConsumableUse use_;
    Entry usedEntry_;
    double nextStatus_=0,nextUsePoll_=0,nextFavorites_=0;
    RE::BSTSmartPointer<RE::BShkbAnimationGraph> animationGraph_;
    struct AnimationSignal {double time;std::string tag,payload;};
    std::mutex animationMutex_;
    std::vector<AnimationSignal> animationSignals_;
    void watchAnimation();
    void drainAnimation();
    void stopAnimationWatch();
    void equipPairStep(const Selection::Commit&,int,bool,std::optional<Entry> plan={});
    void menuCapture(std::size_t,Hand);
    bool captureMenuOpen()const;
    void selectionTime(bool);
    SelectionTime selectionTime_;
    bool soulsRequested_=false,pauseRequested_=false;
    std::uint64_t menuCaptureEpoch_=0;
    bool quickLootAllowed(std::optional<bool> controller={})const;
    bool allowed(MenuPurpose purpose=MenuPurpose::input,std::optional<bool> controller={})const;
    bool consume(RE::InputEvent*);
    void capture(Hand);
    void equip(const Selection::Commit&,bool unequip=false);
    void progressEquipment();
    void progressUse();
    bool accepts(const Selection::Commit&)const;
    void activate(std::size_t group);
    void queueUnequip(std::optional<std::size_t> index={},bool controller=false);
    void queueHand(Hand,bool controller);
    void queueReorder(int delta);
    void queueMove(std::size_t target);
    void refresh();
    void publish();
    void syncHudMenu();
    void cancel();
    void notice(std::string,bool visible=true);
    void tick();
    void queueEdit(int action,std::optional<std::size_t> index={});
    std::string editorBlockReason()const;
    void openEditor();
    void closeEditor(std::string_view reason="requested");
    void updateEditorTextInput();
    void edit(const EditCommand&);
    void buildCatalog();
    void refreshEditor();
    void refreshPresets();
    bool presetCommand(const EditCommand&);
    bool addCandidate(const EditCommand&);
    std::optional<Entry> captureCandidate(std::uint64_t,Hand);
    bool saveCombination(const EditCommand&);
    void applyEditor();
    void flushSettings(bool force=false);
    void saveSettings(const Options&);
public:
    static Game& get();
    bool initialize();
    void loaded();
    void reset(bool clear);
    void dispatch(RE::BSTEventSource<RE::InputEvent*>*,RE::InputEvent**);
    void draw(bool editor=false);
    void drawVR();
    void rendererFailed();
    FrameworkStatus frameworkStatus();
    bool requestEditorFromFramework();
    void requestVREditor(bool open);
    std::optional<TransferTicket> beforeTransfer(RE::PlayerCharacter*,RE::TESBoundObject*,RE::TESObjectREFR*,RE::ExtraDataList*);
    void afterTransfer(RE::PlayerCharacter*,RE::TESBoundObject*,const TransferTicket&);
    void save(SKSE::SerializationInterface*);
    void load(SKSE::SerializationInterface*);
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent*,RE::BSTEventSource<RE::MenuOpenCloseEvent>*)override;
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent*,RE::BSTEventSource<RE::TESContainerChangedEvent>*)override;
    RE::BSEventNotifyControl ProcessEvent(const RE::TESUniqueIDChangeEvent*,RE::BSTEventSource<RE::TESUniqueIDChangeEvent>*)override;
    RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent*,RE::BSTEventSource<RE::TESEquipEvent>*)override;
    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent*,RE::BSTEventSource<RE::BSAnimationGraphEvent>*)override;
};
}
