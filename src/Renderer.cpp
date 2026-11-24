#include <atomic>
#include "Game.h"
#include "Hud.h"
#include "Editor.h"
#include "DXIsolation.h"
#include "Assets.h"
#include "I4Icons.h"
#include "I4Policy.h"
#include "PNGIIIcons.h"
#include "UIIconTasks.h"
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
UIIconTasks& uiIconTasks(){static auto* tasks=new UIIconTasks;return *tasks;}
std::vector<unsigned char> readGameResource(const std::string& path) {
    auto resource=path;std::replace(resource.begin(),resource.end(),'/','\\');
    RE::BSResourceNiBinaryStream stream(resource);
    if(!stream.good()||!stream.stream||!stream.stream->totalSize||stream.stream->totalSize>16*1024*1024)return {};
    std::vector<unsigned char> bytes(stream.stream->totalSize);
    if(!stream.read(bytes.data(),unsigned(bytes.size())))bytes.clear();
    return bytes;
}
struct Graphics {
    ImGuiContext* imgui=nullptr;ID3D11Device* device=nullptr;
    std::unique_ptr<preview::DXIsolation> isolation;
    std::unique_ptr<I4Icons> i4;
    std::unique_ptr<PNGIIIcons> pngii;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> keyTexture;
    struct TextureCacheKey {
        std::filesystem::path path;
        unsigned rasterSize=0;
        bool operator<(const TextureCacheKey& o)const{return path<o.path||(path==o.path&&rasterSize<o.rasterSize);}
    };
    struct CachedTexture {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        int width=0,height=0;
        std::uintmax_t size=0;
        std::filesystem::file_time_type mtime{};
    };
    std::map<TextureCacheKey,CachedTexture> themeTextures;
    // Retired textures from a theme reload are held until the next frame so the GPU finishes referencing them.
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> retiredTextures;
    Hud hud;Editor editor;bool backend=false;double previous=0;
    ~Graphics() {
        Guard guard;if(guard.previous==imgui)guard.previous=nullptr;
        if(imgui)ImGui::SetCurrentContext(imgui);
        if(backend)ImGui_ImplDX11_Shutdown();
        isolation.reset();if(imgui)ImGui::DestroyContext(imgui);
    }
    bool uploadTexture(const ImageAsset& image,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& view) {
        if(!image||!device)return false;
        D3D11_TEXTURE2D_DESC textureDesc{};textureDesc.Width=image.width;textureDesc.Height=image.height;
        textureDesc.MipLevels=textureDesc.ArraySize=1;textureDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count=1;textureDesc.Usage=D3D11_USAGE_IMMUTABLE;textureDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA pixels{};pixels.pSysMem=image.pixels.data();pixels.SysMemPitch=image.width*4;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        return SUCCEEDED(device->CreateTexture2D(&textureDesc,&pixels,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&view));
    }
    void loadThemeAssets(bool reload) {
        if(reload)hud.reloadThemes();
        std::map<TextureCacheKey,CachedTexture> nextTextures;
        const auto fetch=[&](const std::filesystem::path& path,unsigned rasterSize,bool legacyAtlas)->LoadedTexture {
            if(path.empty())return {};
            std::error_code ec;
            if(!std::filesystem::is_regular_file(path,ec))return {};
            const auto sz=std::filesystem::file_size(path,ec);if(ec)return {};
            const auto mt=std::filesystem::last_write_time(path,ec);if(ec)return {};
            const TextureCacheKey key{std::filesystem::weakly_canonical(path,ec),legacyAtlas?0u:rasterSize};
            if(const auto reuse=nextTextures.find(key);reuse!=nextTextures.end())
                return {reinterpret_cast<ImTextureID>(reuse->second.view.Get()),float(reuse->second.width),float(reuse->second.height)};
            if(const auto prev=themeTextures.find(key);prev!=themeTextures.end()&&prev->second.size==sz&&prev->second.mtime==mt&&prev->second.view) {
                nextTextures.emplace(key,prev->second);
                return {reinterpret_cast<ImTextureID>(prev->second.view.Get()),float(prev->second.width),float(prev->second.height)};
            }
            const auto image=legacyAtlas?loadImageAsset(path):loadThemeImage(path,rasterSize);
            if(legacyAtlas&&(image.width!=1024||image.height!=320))return {};
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
            if(!uploadTexture(image,view))return {};
            nextTextures.emplace(key,CachedTexture{view,image.width,image.height,sz,mt});
            return {reinterpret_cast<ImTextureID>(view.Get()),float(image.width),float(image.height)};
        };
        for(const auto& theme:hud.themes().all()) {
            SkinTextures skin=loadSkinTextures(theme,[&](const ThemeImage& img){return img?fetch(img.path,img.rasterSize,false):LoadedTexture{};});
            if(!theme.atlas.empty()) {
                const auto atlas=fetch(theme.atlas,0,true);
                if(atlas)skin.atlas=atlas.id;
                else SKSE::log::warn("Theme {} frame atlas unavailable; using vector fallback",theme.id);
            }
            hud.setThemeTextures(theme.id,std::move(skin));
        }
        // Keep previous textures alive for one more frame before releasing them.
        for(auto& [key,entry]:themeTextures)if(entry.view&&!nextTextures.contains(key))retiredTextures.push_back(std::move(entry.view));
        themeTextures=std::move(nextTextures);
        for(const auto& warning:hud.themes().warnings())SKSE::log::warn("Theme pack: {}",warning);
        hud.setDetectedTheme(hud.themes().detect(readGameResource));
        SKSE::log::info("{}: {} theme packs, {} textures, installed interface {}",reload?"Theme reload":"Theme init",
            hud.themes().all().size(),themeTextures.size(),hud.detectedTheme().empty()?"none (Minimal)":hud.detectedTheme());
    }
    void initialize(ID3D11Device* d,ID3D11DeviceContext* c,IDXGISwapChain* swap) {
        DXGI_SWAP_CHAIN_DESC desc{};
        if(FAILED(swap->GetDesc(&desc))||!desc.OutputWindow)throw std::runtime_error("Cannot locate game window");
        gameWindow=desc.OutputWindow;device=d;
        isolation=std::make_unique<preview::DXIsolation>(d,c);
        i4=std::make_unique<I4Icons>(d,c);
        pngii=std::make_unique<PNGIIIcons>(d,readGameResource,[](std::string_view message){SKSE::log::info("{}",message);},[](std::function<void()> task){uiIconTasks().post(std::move(task));});
        hud.setItemIconProvider([this](const Entry& entry){
            auto fallback=i4->find(entry);
            if(const auto label=i4->label(entry))if(auto icon=pngii->find(i4HasForm(entry)?entry.form:0,*label))return icon;
            return fallback;
        });
        IMGUI_CHECKVERSION();imgui=ImGui::CreateContext();ImGui::SetCurrentContext(imgui);
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;io.MouseDrawCursor=false;
        io.ConfigErrorRecoveryEnableAssert=false;io.ConfigErrorRecoveryEnableTooltip=false;
        hud.initialize((dataDirectory()/"Quickslot/fonts").string(),(dataDirectory()/"Interface/ImGuiIcons/Fonts").string());
        loadThemeAssets(false);
        const auto keys=loadKeyAtlas(dataDirectory()/"Interface/ImGuiIcons/Icons");
        if(uploadTexture(keys.image,keyTexture))hud.setKeys(reinterpret_cast<ImTextureID>(keyTexture.Get()),keys.glyphs);
        SKSE::log::info("Quickslot HUD assets: I4 item icons, theme packs {}, ImGui key glyphs {}",hud.themes().all().size(),keyTexture?keys.loaded:0);
        backend=ImGui_ImplDX11_Init(d,c);if(!backend)throw std::runtime_error("D3D11 backend initialization failed");
        previous=clockNow();SKSE::log::info("Quickslot native renderer ready");
    }
};
struct Renderer {std::unique_ptr<Graphics> graphics;double retryAfter=0;};

Renderer& renderer(){static auto* value=new Renderer;return *value;}
}
void runUIIconTasks() {
    try{uiIconTasks().run();}
    catch(const std::exception& error){SKSE::log::error("PNGII UI icon lookup failed: {}",error.what());}
}
bool renderHasFocus() {
    if(REL::Module::IsVR())return vrOverlayReady();
    if(auto window=gameWindow.load())return GetForegroundWindow()==window;
    const auto foreground=GetForegroundWindow();DWORD pid=0;
    if(foreground)GetWindowThreadProcessId(foreground,&pid);
    return pid==GetCurrentProcessId();
}
namespace {std::atomic<float> lastWidth{0},lastHeight{0};}
std::pair<float,float> renderSize(){return {lastWidth.load(),lastHeight.load()};}
std::vector<EditCommand> renderHud(const Snapshot& snapshot,const Options& options,const EditorState* editor,const std::vector<EditorInput>& input) {
    std::vector<EditCommand> commands;
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
        g.retiredTextures.clear();
        if(g.editor.consumeThemeReloadRequest(now,g.hud.themesDirectory(),editor!=nullptr))g.loadThemeAssets(true);
        g.i4->beginFrame(editor?editor->options.itemArtwork:options.itemArtwork);
        g.pngii->beginFrame((editor?editor->options.itemArtwork:options.itemArtwork)==2,itemIconGeneration());
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
        lastWidth=size.x;lastHeight=size.y;
        auto& io=ImGui::GetIO();io.DisplaySize=size;io.DisplayFramebufferScale={1,1};
        io.DeltaTime=float(std::clamp(now-g.previous,.001,.1));g.previous=now;
        if(editor){g.editor.beginSession(*editor);g.editor.input(input);}
        io.MouseDrawCursor=editor&&(!editor->controller||g.editor.controllerPointer());
        const bool showEditor=vr?beginVROverlay(editor!=nullptr):editor!=nullptr;
        if(vr)io.MouseDrawCursor=false;
        ImGui_ImplDX11_NewFrame();ImGui::NewFrame();begun=true;
        if(editor&&showEditor)commands=g.editor.draw(*editor,g.hud,now);
        else if(!editor&&snapshot.visible&&!vrOtherOverlayOpen())g.hud.draw(snapshot,options,now);
        ImGui::Render();begun=false;
        if(vr)finishVROverlay(context,showEditor);
        else {isolated.bindTarget();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());}
    }catch(const std::exception& error) {
        if(begun)ImGui::EndFrame();clearVROverlay(activeContext);r.graphics.reset();r.retryAfter=now+5;
        SKSE::log::error("Quickslot rendering failed: {}",error.what());
        Game::get().rendererFailed();
    }
    return commands;
}
}
