#pragma once
#include "PNGIIClient.h"
#include "PNGIIImages.h"
#include "ItemIcons.h"
#include "Settings.h"
#include "Presets.h"
#include "pngii_fixture.h"
#include "UIIconTasks.h"
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

inline void pngiiCheck(bool value,const char* why){if(!value)throw std::runtime_error(why);}
namespace pngii_test {
inline std::string selected;
inline unsigned calls=0;
inline std::uint32_t seenForm=0;
inline std::string seenLabel;
inline std::int32_t seenSoul=0;
inline PNGII::API::Result result=PNGII::API::Result::kSuccess;
inline void uiTaskChecks() {
    using namespace std::chrono_literals;
    quickslot::UIIconTasks tasks;
    unsigned completed=0;
    tasks.post([&]{++completed;tasks.post([&]{++completed;});});
    tasks.run();pngiiCheck(completed==1,"UI updates defer work posted during a callback");
    tasks.run();pngiiCheck(completed==2,"The next UI update completes deferred work");

    std::promise<void> entered,release;
    const auto released=release.get_future().share();
    tasks.post([&]{entered.set_value();released.wait();});
    std::thread ui([&]{tasks.run();});
    entered.get_future().wait();
    auto render=std::async(std::launch::async,[&]{tasks.post([&]{++completed;});});
    const bool responsive=render.wait_for(1s)==std::future_status::ready;
    release.set_value();ui.join();render.get();
    pngiiCheck(responsive,"Rendering can queue icons while a UI callback waits for the engine");
    pngiiCheck(completed==2,"Concurrent submissions wait for the next UI update");
    tasks.run();pngiiCheck(completed==3,"Concurrent submissions run once on the UI thread");
}
inline PNGII::API::Result resolve(std::uint32_t form,const char* label,std::int32_t soul,char* output,std::uint32_t capacity,std::uint32_t* required)noexcept {
    ++calls;seenForm=form;seenLabel=label;seenSoul=soul;*required=0;if(capacity)output[0]=0;
    if(form==90){*required=0xffffffff;return PNGII::API::Result::kBufferTooSmall;}
    if(form==91){*required=capacity+1;return PNGII::API::Result::kSuccess;}
    if(form==92){std::memset(output,'x',capacity);*required=capacity;return PNGII::API::Result::kSuccess;}
    if(result!=PNGII::API::Result::kSuccess)return result;
    *required=unsigned(selected.size()+1);
    if(capacity<*required)return PNGII::API::Result::kBufferTooSmall;
    std::memcpy(output,selected.c_str(),*required);return result;
}
inline void checks() {
    uiTaskChecks();
    using namespace quickslot;using Result=PNGII::API::Result;
    const PNGII::API::InterfaceV1 api{1,sizeof(api),resolve};
    selected="Interface/PNGIconInjector/Pack/sword.png";
    auto query=queryPNGII(&api,0x123,"weapon_sword",2);
    pngiiCheck(query.result==Result::kSuccess&&query.path==selected&&seenForm==0x123&&seenLabel=="weapon_sword"&&seenSoul==2,"Query passes form, I4 label and soul status");
    selected="Interface/PNGIconInjector/"+std::string(300,'a')+".svg";calls=0;
    pngiiCheck(queryPNGII(&api,0,"armor_hands").path==selected&&calls==2&&seenSoul==-1,"Resize caller buffer for long paths and label-only queries");
    for(const auto state:{Result::kNoMatch,Result::kNotReady,Result::kError,Result::kInvalidArgument}) {
        result=state;query=queryPNGII(&api,1,"weapon_sword");pngiiCheck(query.result==state&&query.path.empty(),"Preserve provider result for fallback and readiness");
    }
    result=Result::kSuccess;
    for(auto form:{90u,91u,92u})pngiiCheck(queryPNGII(&api,form,"weapon_sword").result==Result::kError,"Reject malformed output sizes and terminators");
    auto bad=api;bad.version=2;pngiiCheck(!pngiiAPIValid(&bad),"Reject incompatible API version");
    bad=api;bad.structSize=8;pngiiCheck(!pngiiAPIValid(&bad),"Reject truncated API table");
    bad=api;bad.resolveIcon=nullptr;pngiiCheck(!pngiiAPIValid(&bad)&&!pngiiAPIValid(nullptr),"Require callable API");
    for(const auto path:{"../x.png","C:/x.png","Interface/PNGIconInjector/../x.png","Interface/PNGIconInjector//x.png","Interface/PNGIconInjector/./x.png","Interface/Elsewhere/x.png","Interface/PNGIconInjector/x.swf","Interface/PNGIconInjector/x.png/"})
        pngiiCheck(!pngiiResourcePath(path),"Reject paths outside PNGII resources and unsupported formats");
    pngiiCheck(pngiiResourcePath("Interface\\PNGIconInjector\\Pack\\icon.SVG")=="Interface/PNGIconInjector/Pack/icon.SVG","Normalize game resource separators");
    selected=std::string("Interface/PNGIconInjector/x\0.png",30);pngiiCheck(queryPNGII(&api,1,"x").result==Result::kError,"Reject embedded terminator");

    const auto png=decodePNGIIImage("icon.png",pngiiPNG);
    pngiiCheck(png&&png.width==256&&png.height==256,"Decode PNG and fit icon canvas");
    const auto center=(128*256+128)*4;
    pngiiCheck(png.pixels[center]==255&&png.pixels[center+1]==0&&png.pixels[center+2]==0&&png.pixels[center+3]>0&&png.pixels[center+3]<255,"PNG resampling preserves straight color at translucent edges");
    pngiiCheck(png.pixels[3]==0&&png.pixels[(73*256+128)*4+3]==0&&png.pixels[(74*256+128)*4+3]>0,"Fit preserves non-square aspect and transparent padding");
    const auto svg=decodePNGIIImage("icon.SVG",{reinterpret_cast<const unsigned char*>(pngiiSVG),sizeof(pngiiSVG)-1});
    pngiiCheck(svg&&svg.pixels[center]==0&&svg.pixels[center+1]==255&&svg.pixels[center+2]==0&&svg.pixels[center+3]>=127&&svg.pixels[center+3]<=128,"SVG CSS fill and alpha decode to straight RGBA");
    pngiiCheck(svg.pixels[(73*256+128)*4+3]==0&&svg.pixels[(182*256+128)*4+3]==0,"SVG fit preserves aspect and padding");
    pngiiCheck(!decodePNGIIImage("bad.png",{})&&!decodePNGIIImage("bad.svg",pngiiPNG)&&!decodePNGIIImage("bad.jpg",pngiiPNG)&&!decodeImageAsset(pngiiPNG,1),"Malformed, unsupported and oversized image fallback");
    Options options;pngiiCheck(options.pngiiIcons,"PNGII preference defaults on");options.pngiiIcons=false;
    pngiiCheck(!decodeOptions(encodeOptions(options)).pngiiIcons,"PNGII option persists");
    auto json=Json::parse(encodeOptions(options));json.erase("pngiiIcons");pngiiCheck(decodeOptions(json.dump()).pngiiIcons,"Existing settings adopt PNGII preference");
    const auto preset=exportPreset(Arsenal{},options,true,[](auto){return Json{};});
    pngiiCheck(!importPreset(preset,Arsenal{},Options{},true,[](const auto&,auto){return 0u;}).options.pngiiIcons,"Presets preserve PNGII option");
    ItemIcon icon{123,456,{}};
    pngiiCheck(itemIconTexture(icon,0,false)==123&&itemIconTexture(icon,1,false)==456&&itemIconTexture(icon,2,false)==456,"Original and tint modes select expected artwork");
    pngiiCheck(itemIconTint(icon,0x80aabbcc,0,false)==0x80ffffff,"Original PNGII artwork keeps its colors and opacity");
    std::cout<<"PASS: PNGII API contract, resource paths, PNG/SVG pixels, colors, settings and presets\n";
}
}
