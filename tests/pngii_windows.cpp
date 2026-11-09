#include "pngii_checks.h"
#include "PNGIIIcons.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <map>
#include <thread>

using Microsoft::WRL::ComPtr;
using namespace quickslot;
std::array<unsigned char,4> texturePixel(ID3D11Device* device,ID3D11DeviceContext* context,ImTextureID id) {
    auto* view=reinterpret_cast<ID3D11ShaderResourceView*>(id);ComPtr<ID3D11Resource> resource;view->GetResource(&resource);
    ComPtr<ID3D11Texture2D> texture,staging;pngiiCheck(SUCCEEDED(resource.As(&texture)),"Get uploaded icon texture");
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);pngiiCheck(desc.Width==256&&desc.Height==256,"Icon GPU dimensions");
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    pngiiCheck(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"Create icon readback");context->CopyResource(staging.Get(),texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};pngiiCheck(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Map uploaded icon");
    std::array<unsigned char,4> pixel;std::memcpy(pixel.data(),static_cast<unsigned char*>(mapped.pData)+128*mapped.RowPitch+128*4,4);context->Unmap(staging.Get(),0);return pixel;
}
int main(int argc,char** argv)try {
    if(argc!=4)return 2;pngii_test::checks();
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    auto status=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
    if(FAILED(status))status=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
    pngiiCheck(SUCCEEDED(status),"Create icon upload device");
    std::map<std::string,unsigned> reads;std::vector<std::string> logs;UIIconTasks tasks;unsigned queued=0;
    const auto read=[&](const std::string& path) {
        ++reads[path];
        if(path.ends_with("icon.png"))return std::vector<unsigned char>(pngiiPNG.begin(),pngiiPNG.end());
        if(path.ends_with("icon.svg"))return std::vector<unsigned char>(pngiiSVG,pngiiSVG+sizeof(pngiiSVG)-1);
        if(path.ends_with("bad.png"))return std::vector<unsigned char>{1,2,3};
        if(path.ends_with("throws.png"))throw std::runtime_error("Resource read failure");
        return std::vector<unsigned char>{};
    };
    const auto log=[&](std::string_view message){logs.emplace_back(message);};
    const auto schedule=[&](std::function<void()> task){++queued;tasks.post(std::move(task));};
    const auto drain=[&]{queued=0;std::thread ui([&]{tasks.run();});ui.join();};
    const auto tick=[&](PNGIIIcons& icons,std::uint64_t generation=0){icons.beginFrame(true,generation);drain();};
    {
        PNGIIIcons absent(device.Get(),read,log,schedule);tick(absent);
        pngiiCheck(!absent.find(1,"weapon_custom")&&reads.empty()&&!queued,"Absent provider leaves I4 available");
    }
    for(int i=1;i<=2;++i) {
        const auto module=LoadLibraryW(std::filesystem::path(argv[i]).c_str());pngiiCheck(module!=nullptr,"Load incompatible fixture");
        {PNGIIIcons old(device.Get(),read,log,schedule);tick(old);pngiiCheck(!old.find(1,"weapon_custom")&&!queued,"Legacy and incompatible API fall back");}
        FreeLibrary(module);
    }
    const auto module=LoadLibraryW(std::filesystem::path(argv[3]).c_str());pngiiCheck(module!=nullptr,"Load provider fixture");
    const auto ready=reinterpret_cast<void(*)(bool)>(GetProcAddress(module,"PNGII_TestReady"));
    const auto calls=reinterpret_cast<unsigned(*)()>(GetProcAddress(module,"PNGII_TestCalls"));pngiiCheck(ready&&calls,"Find provider controls");
    {
        PNGIIIcons icons(device.Get(),read,log,schedule);tick(icons);
        pngiiCheck(!icons.find(1,"weapon_custom")&&calls()==0,"Drawing queues queries for the UI thread");
        icons.beginFrame(true,0);pngiiCheck(calls()==0&&queued,"Query waits for UI task execution");drain();tick(icons);
        pngiiCheck(!icons.find(1,"weapon_custom")&&reads.empty(),"Not-ready provider uses I4 while waiting");ready(true);
        for(unsigned frame=0;frame<20;++frame)tick(icons);
        const auto png=icons.find(1,"weapon_custom");pngiiCheck(bool(png)&&!png->rgb,"Provider becomes ready and preserves original colors");
        const auto original=texturePixel(device.Get(),context.Get(),png->original),mask=texturePixel(device.Get(),context.Get(),png->mask);
        pngiiCheck(original[0]==255&&original[1]==0&&original[2]==0&&original[3]>0&&original[3]<255,"PNG GPU pixels preserve alpha and color");
        pngiiCheck(mask[0]==255&&mask[1]==255&&mask[2]==255&&mask[3]==original[3],"Tint mask preserves image alpha");
        const auto before=calls();for(unsigned f=0;f<20;++f){pngiiCheck(bool(icons.find(1,"weapon_custom")),"Cache stays ready");tick(icons);}
        pngiiCheck(calls()==before&&reads["Interface/PNGIconInjector/Test/icon.png"]==1,"Cached matches avoid repeated lookup and decoding");
        icons.find(0,"armor_hands");for(unsigned f=0;f<3;++f)tick(icons);
        pngiiCheck(bool(icons.find(0,"armor_hands"))&&reads["Interface/PNGIconInjector/Test/icon.png"]==1,"Label-only hand artwork shares texture cache");
        for(unsigned form:{2,3,4,5,6}){icons.find(form,"weapon_custom");for(unsigned f=0;f<3;++f)tick(icons);}
        for(unsigned form:{2,3,4,6})pngiiCheck(!icons.find(form,"weapon_custom"),"Unmatched, missing, corrupt and failed resources use I4");
        const auto svg=icons.find(5,"weapon_custom");pngiiCheck(bool(svg),"SVG matched artwork loads");
        const auto green=texturePixel(device.Get(),context.Get(),svg->original);
        pngiiCheck(green[0]==0&&green[1]==255&&green[2]==0&&green[3]>=127&&green[3]<=128,"SVG GPU pixels preserve color and transparency");
        const auto queried=calls();for(unsigned f=0;f<20;++f){icons.find(2,"weapon_custom");icons.find(3,"weapon_custom");icons.find(4,"weapon_custom");tick(icons);}
        pngiiCheck(calls()==queried&&reads["Interface/PNGIconInjector/Test/missing.png"]==1,"Misses and failed images are cached");
        icons.beginFrame(true,1);icons.find(1,"weapon_custom");icons.beginFrame(true,1);
        pngiiCheck(queued,"Queue lookup before save transition");icons.beginFrame(true,2);drain();icons.beginFrame(true,2);
        pngiiCheck(!icons.find(1,"weapon_custom"),"Replies from the previous save are discarded");
        for(unsigned f=0;f<3;++f)tick(icons,2);pngiiCheck(bool(icons.find(1,"weapon_custom")),"New save resolves fresh artwork");
        icons.beginFrame(false,2);pngiiCheck(!icons.find(1,"weapon_custom"),"Disabling PNGII immediately returns to I4");
        tick(icons,2);icons.find(1,"weapon_custom");for(unsigned f=0;f<3;++f)tick(icons,2);
        pngiiCheck(bool(icons.find(1,"weapon_custom")),"Re-enabling PNGII refreshes matches");
        icons.beginFrame(true,3);icons.find(1,"weapon_custom");icons.beginFrame(true,3);pngiiCheck(queued,"Queue before renderer destruction");
    }
    const auto beforeDestruction=calls();drain();pngiiCheck(calls()==beforeDestruction,"Destroyed renderer cancels pending UI queries");
    FreeLibrary(module);
    std::cout<<"PASS: PNGII DLL discovery, UI-thread lookup, readiness, cache, save transitions, cancellation, GPU artwork and fallbacks\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
