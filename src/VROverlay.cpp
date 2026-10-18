#include "Game.h"
#include "Version.h"
#include "VROverlay.h"
#include "VRInput.h"
#include "VRFocus.h"
#include "../third_party/imgui-vr-helper/ImGuiVRHelperClientSDK.h"

namespace quickslot {
namespace {
namespace api=ImGuiVRHelperPluginAPI;
struct Overlay {
    api::Client editor;
    std::uint32_t hud=0;
    std::atomic_bool ready{false};
    std::atomic_bool hasFocus{false};
    std::atomic<int> editorReply{-1};
    std::atomic_bool editorWanted{false};
    VRFocus focus;
    VRPanelFocus panelFocus;
};
// SKSE plugins are process lifetime; avoid DX/ImGui or helper calls at teardown.
Overlay& overlay(){static auto* value=new Overlay;return *value;}
void clear(api::Client& client,ID3D11DeviceContext* context) {
    api::PanelHandle panel{};
    if(client.IsConnected()&&context&&client.Helper()->GetPanel(client.Id(),&panel)&&panel.rtv) {
        constexpr float transparent[]{0,0,0,0};context->ClearRenderTargetView(panel.rtv,transparent);
    }
}
api::PanelHandle hudPanel() {
    auto& v=overlay();api::PanelHandle panel{};
    if(v.hud&&v.editor.IsConnected())v.editor.Helper()->GetPanel(v.hud,&panel);
    return panel;
}
void clearHUD(ID3D11DeviceContext* context) {
    if(const auto panel=hudPanel();context&&panel.rtv){constexpr float transparent[]{0,0,0,0};context->ClearRenderTargetView(panel.rtv,transparent);}
}
}
bool initializeVROverlay() {
    if(!REL::Module::IsVR())return true;
    auto& v=overlay();if(v.ready)return true;
    if(!v.editor.Connect("Quickslot",release::version,api::kClientFlag_RendersOnFocus)) {
        SKSE::log::error("Skyrim VR requires ImGui VR Helper; Quickslot remains disabled until it is installed");return false;
    }
    // Render on the helper's frame thread, even when Skyrim hides/closes our
    // menus. This clears stale HUD pixels during loading, TM, and menu swaps.
    v.hud=v.editor.Helper()->RegisterClient("Quickslot HUD",release::version,[](const api::Frame*,void*){
        auto& state=overlay();
        if(state.ready){state.hasFocus=state.editor.HasFocus();Game::get().drawVR();}
    },nullptr,api::kClientFlag_HUDMode);
    if(!v.hud){v.editor.Disconnect();SKSE::log::error("ImGui VR Helper could not register the Quickslot HUD");return false;}
    v.ready=true;SKSE::log::info("Quickslot VR HUD and editor registered with ImGui VR Helper");return true;
}
bool vrOverlayReady(){return overlay().ready;}
bool vrOtherOverlayOpen() {
    // SDK HasFocus belongs to the helper's frame thread. Publish its snapshot
    // before drawing so game-thread input never races that non-atomic field.
    auto& v=overlay();return v.ready&&v.editor.Helper()->IsOverlayVisible()&&!v.hasFocus;
}
void feedVROverlay(RE::InputEvent* event) {
    auto& v=overlay();if(!v.ready||!event||!vrDevice(unsigned(event->GetDevice())))return;
    if(auto* b=event->AsButtonEvent())v.editor.FeedVREvent(unsigned(event->GetDevice()),b->GetIDCode(),b->IsPressed(),0,0);
    else if(auto* s=event->AsThumbstickEvent())v.editor.FeedVREvent(unsigned(event->GetDevice()),s->GetIDCode(),false,s->xValue,s->yValue);
    else if(event->GetEventType()==RE::INPUT_EVENT_TYPE::kVrTouchpadPosition||event->GetEventType()==RE::INPUT_EVENT_TYPE::kVrTouchpadSwipe) {
        if(auto* devices=RE::BSInputDeviceManager::GetSingleton()) {
            auto* device=unsigned(event->GetDevice())%2?devices->GetVRControllerRight():devices->GetVRControllerLeft();
            if(device){const auto axis=static_cast<RE::BSOpenVRControllerDevice*>(device)->GetThumbstick();v.editor.FeedVREvent(unsigned(event->GetDevice()),11,false,axis.x,axis.y);}
        }
    }
}
void vrEditorResult(bool open){overlay().editorReply.store(open?1:0);}
bool vrEditorWanted(){return overlay().editorWanted;}
bool beginVROverlay(bool editorOpen) {
    auto& v=overlay();if(!v.ready)return false;
    const auto reply=v.editorReply.exchange(-1);
    v.focus.update(editorOpen,reply<0?std::nullopt:std::optional<bool>(reply!=0));
    v.editor.ApplyPanelDisplaySize(); // Wand coordinates must use the panel's exact size before PumpInput.
    if(const auto request=v.panelFocus.update(v.focus.desired,v.editor.HasFocus())) {
        if(*request)v.editor.RequestFocus();else v.editor.ReleaseFocus();
    }
    v.editorWanted=v.focus.desired;
    if(const auto request=v.focus.request(editorOpen))Game::get().requestVREditor(*request);
    const bool editing=editorOpen&&v.focus.desired;
    v.editor.PumpKeyboard();
    v.editor.PumpInput(editing&&v.editor.HasFocus());
    auto& io=ImGui::GetIO();
    if(editing)v.editor.ApplyPanelDisplaySize();
    else {
        // Layout uses a stable 1080p canvas; helper supersampling changes the
        // render resolution without moving or shrinking Quickslot's widgets.
        io.DisplaySize={1920,1080};io.DisplayFramebufferScale={1,1};
        if(const auto panel=hudPanel();panel.width&&panel.height)
            io.DisplayFramebufferScale={panel.width/1920.f,panel.height/1080.f};
    }
    return editing;
}
void finishVROverlay(ID3D11DeviceContext* context,bool editorOpen) {
    auto& v=overlay();if(!v.ready)return;
    if(editorOpen){clearHUD(context);v.editor.RenderToPanel(context);}
    else {
        clear(v.editor,context);
        if(const auto panel=hudPanel();context&&panel.rtv) {
            clearHUD(context);context->OMSetRenderTargets(1,&panel.rtv,nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }
}
void clearVROverlay(ID3D11DeviceContext* context){auto& v=overlay();if(v.ready){clearHUD(context);clear(v.editor,context);}}
}
