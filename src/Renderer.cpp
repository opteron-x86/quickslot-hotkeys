#include "Game.h"
#include "Hud.h"
#include "Editor.h"
#include "DXIsolation.h"
#include "Assets.h"
#include "IconPacks.h"
#include "VROverlay.h"
#include <imgui_impl_dx11.h>
#include <windows.h>
#include <wrl/client.h>

namespace quickslot {
namespace {
struct Guard {
    ImGuiContext* previous=ImGui::GetCurrentContext();
    ~Guard(){ImGui::SetCurrentContext(previous);}
};
std::atomic<HWND> gameWindow{nullptr};
struct Graphics {
    ImGuiContext* imgui=nullptr;ID3D11Device* device=nullptr;
    std::unique_ptr<preview::DXIsolation> isolation;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> keyTexture;
    std::map<std::string,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> iconTextures;
    std::map<std::string,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> themeTextures;
    Hud hud;Editor editor;bool backend=false;double previous=0;
    ~Graphics() {
        Guard guard;if(guard.previous==imgui)guard.previous=nullptr;
        if(imgui)ImGui::SetCurrentContext(imgui);
        if(backend)ImGui_ImplDX11_Shutdown();
        isolation.reset();if(imgui)ImGui::DestroyContext(imgui);
    }
    void initialize(ID3D11Device* d,ID3D11DeviceContext* c,IDXGISwapChain* swap) {
        DXGI_SWAP_CHAIN_DESC desc{};
        if(FAILED(swap->GetDesc(&desc))||!desc.OutputWindow)throw std::runtime_error("Cannot locate game window");
        gameWindow=desc.OutputWindow;device=d;
        isolation=std::make_unique<preview::DXIsolation>(d,c);
        IMGUI_CHECKVERSION();imgui=ImGui::CreateContext();ImGui::SetCurrentContext(imgui);
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;io.MouseDrawCursor=false;
        io.ConfigErrorRecoveryEnableAssert=false;io.ConfigErrorRecoveryEnableTooltip=false;
        hud.initialize((dataDirectory()/"Quickslot/fonts").string());
        const auto upload=[&](const ImageAsset& image,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& view) {
            if(!image)return false;
            D3D11_TEXTURE2D_DESC textureDesc{};textureDesc.Width=image.width;textureDesc.Height=image.height;
            textureDesc.MipLevels=textureDesc.ArraySize=1;textureDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.SampleDesc.Count=1;textureDesc.Usage=D3D11_USAGE_IMMUTABLE;textureDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA pixels{};pixels.pSysMem=image.pixels.data();pixels.SysMemPitch=image.width*4;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            return SUCCEEDED(d->CreateTexture2D(&textureDesc,&pixels,&texture))&&SUCCEEDED(d->CreateShaderResourceView(texture.Get(),nullptr,&view));
        };
        for(const auto& pack:loadIconPacks(dataDirectory()/"Quickslot/icons")) {
            const auto atlas=loadItemAtlas(pack.atlas);auto& view=iconTextures[pack.id];
            if(upload(atlas,view))hud.setIcons(pack.id,pack.name,reinterpret_cast<ImTextureID>(view.Get()));
            else SKSE::log::warn("Icon pack {} unavailable",pack.id);
        }
        for(const auto& theme:hud.themes().all())if(!theme.atlas.empty()) {
            const auto image=loadImageAsset(theme.atlas);auto& view=themeTextures[theme.id];
            if(image.width==1024&&image.height==320&&upload(image,view))hud.setTheme(theme.id,reinterpret_cast<ImTextureID>(view.Get()));
            else SKSE::log::warn("Theme {} frame atlas unavailable; using vector fallback",theme.id);
        }
        for(const auto& warning:hud.themes().warnings())SKSE::log::warn("Theme pack: {}",warning);
        const auto keys=loadKeyAtlas(dataDirectory()/"Interface/ImGuiIcons/Icons");
        if(upload(keys.image,keyTexture))hud.setKeys(reinterpret_cast<ImTextureID>(keyTexture.Get()),keys.glyphs);
        SKSE::log::info("Quickslot HUD assets: item atlas {}, theme packs {}, ImGui key glyphs {}",bool(iconTextures["quickslot"]),hud.themes().all().size(),keyTexture?keys.loaded:0);
        if(!iconTextures["quickslot"])SKSE::log::warn("Quickslot icon atlas unavailable; using fallback glyphs");
        backend=ImGui_ImplDX11_Init(d,c);if(!backend)throw std::runtime_error("D3D11 backend initialization failed");
        previous=clockNow();SKSE::log::info("Quickslot native renderer ready");
    }
};
struct Renderer {std::unique_ptr<Graphics> graphics;double retryAfter=0;};

Renderer& renderer(){static auto* value=new Renderer;return *value;}
}
bool renderHasFocus() {
    if(REL::Module::IsVR())return vrOverlayReady();
    if(auto window=gameWindow.load())return GetForegroundWindow()==window;
    const auto foreground=GetForegroundWindow();DWORD pid=0;
    if(foreground)GetWindowThreadProcessId(foreground,&pid);
    return pid==GetCurrentProcessId();
}
RenderResult renderHud(const Snapshot& snapshot,const Options& options,const EditorState* editor,const std::vector<EditorInput>& input) {
    RenderResult result;
    if(!renderHasFocus()){if(editor)Game::get().rendererFailed();return {};}
    auto& r=renderer();const auto now=clockNow();if(now<r.retryAfter){if(editor)Game::get().rendererFailed();return {};}
    Guard guard;bool begun=false;ID3D11DeviceContext* activeContext=nullptr;
    try {
        auto* engine=RE::BSGraphics::Renderer::GetSingleton();if(!engine){if(editor)Game::get().rendererFailed();return {};}
        auto& data=engine->GetRuntimeData();
        auto* device=reinterpret_cast<ID3D11Device*>(data.forwarder);
        auto* context=reinterpret_cast<ID3D11DeviceContext*>(data.context);
        activeContext=context;
        auto* swap=reinterpret_cast<IDXGISwapChain*>(data.renderWindows[0].swapChain);
        if(!device||!context||!swap){if(editor)Game::get().rendererFailed();return {};}
        if(r.graphics&&r.graphics->device!=device)r.graphics.reset();
        if(!r.graphics){r.graphics=std::make_unique<Graphics>();r.graphics->initialize(device,context,swap);}
        auto& g=*r.graphics;ImGui::SetCurrentContext(g.imgui);
        localization().select(options.language);
        const bool vr=REL::Module::IsVR();
        preview::DXIsolation::Frame isolated(*g.isolation,!vr);
        const auto capturedCounts=isolated.shaderInstanceCounts();
        static bool captureLogged=false,nonzeroCaptureLogged=false;
        const bool nonzeroCounts=capturedCounts[0]||capturedCounts[1]||capturedCounts[2];
        if(!captureLogged||(!nonzeroCaptureLogged&&nonzeroCounts)) {
            captureLogged=true;nonzeroCaptureLogged=nonzeroCaptureLogged||nonzeroCounts;
            SKSE::log::info("DX compatibility: captured shader instance counts (HS={}, DS={}, CS={}).",capturedCounts[0],capturedCounts[1],capturedCounts[2]);
        }
        if(isolated.unexpectedShaderInstances()) {
            static bool logged=false;
            if(!logged) {
                logged=true;
                const auto counts=isolated.shaderInstanceCounts();
                SKSE::log::warn("DX compatibility: shader output contains unreported slots or oversized counts; limiting restore/release to captured counts (HS={}, DS={}, CS={}).",counts[0],counts[1],counts[2]);
            }
        }
        const auto size=vr?ImVec2{1920,1080}:isolated.targetSize();if(size.x<=0||size.y<=0)return {};
        auto& io=ImGui::GetIO();io.DisplaySize=size;io.DisplayFramebufferScale={1,1};
        io.DeltaTime=float(std::clamp(now-g.previous,.001,.1));g.previous=now;
        if(editor){g.editor.beginSession(*editor);g.editor.input(input);}
        io.MouseDrawCursor=editor&&(!editor->controller||g.editor.controllerPointer());
        const bool showEditor=vr?beginVROverlay(editor!=nullptr):editor!=nullptr;
        if(vr)io.MouseDrawCursor=false;
        ImGui_ImplDX11_NewFrame();ImGui::NewFrame();begun=true;
        if(editor&&showEditor)result.commands=g.editor.draw(*editor,g.hud,now);
        else if(!editor&&snapshot.visible&&!vrOtherOverlayOpen())g.hud.draw(snapshot,options,now);
        ImGui::Render();begun=false;
        if(vr)finishVROverlay(context,showEditor);
        else {isolated.bindTarget();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());}
        result.wantsTextInput=editor&&showEditor&&g.editor.wantsTextInput();
    }catch(const std::exception& error) {
        if(begun)ImGui::EndFrame();clearVROverlay(activeContext);r.graphics.reset();r.retryAfter=now+5;
        SKSE::log::error("Quickslot rendering failed: {}",error.what());
        Game::get().rendererFailed();
    }
    return result;
}
}
