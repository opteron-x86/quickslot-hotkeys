#include "PNGIIIcons.h"
#include "PNGIIClient.h"
#include "PNGIIImages.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <deque>
#include <format>
#include <map>
#include <mutex>
#include <set>

namespace quickslot {
struct PNGIIIcons::Impl {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device;
    ReadResource read;
    Log log;
    UITask schedule;
    const PNGII::API::InterfaceV1* api=nullptr;
    bool discovered=false,enabled=false,reported=false,queryReported=false;
    std::uint64_t frame=0,generation=0,retryAfter=0,epoch=0;
    struct Request {std::uint32_t form;std::string label,key;};
    struct Reply {Request request;PNGIIQuery query;std::uint64_t epoch;};
    struct Inbox {std::mutex mutex;std::vector<Reply> replies;};
    std::shared_ptr<Inbox> inbox=std::make_shared<Inbox>();
    std::set<std::string> inFlight;
    struct Match {std::string path;std::uint64_t used=0;};
    struct Texture {Ptr<ID3D11ShaderResourceView> original,mask;std::uint64_t used=0;};
    std::map<std::string,Match> matches;
    std::map<std::string,Texture> textures;
    std::deque<Request> pending;
    Impl(ID3D11Device* d,ReadResource r,Log l,UITask task):device(d),read(std::move(r)),log(std::move(l)),schedule(std::move(task)){}
    void discover() {
        discovered=true;
        const auto module=GetModuleHandleW(L"PNGIconInjector.dll");if(!module)return;
        const auto request=reinterpret_cast<PNGII::API::RequestAPIFunction>(GetProcAddress(module,PNGII::API::kExportName));
        const auto* candidate=request?static_cast<const PNGII::API::InterfaceV1*>(request(PNGII::API::kVersion1)):nullptr;
        if(pngiiAPIValid(candidate)){api=candidate;log("PNGII icon API v1 available");}
        else log("PNGII icon API v1 unavailable; using I4 artwork");
    }
    template<class Map> void trim(Map& values,std::size_t limit) {
        while(values.size()>=limit)values.erase(std::min_element(values.begin(),values.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;}));
    }
    Ptr<ID3D11ShaderResourceView> upload(const ImageAsset& image) {
        Ptr<ID3D11Texture2D> texture;Ptr<ID3D11ShaderResourceView> view;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=image.width;desc.Height=image.height;desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA data{image.pixels.data(),unsigned(image.width)*4,0};
        if(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)))device->CreateShaderResourceView(texture.Get(),nullptr,&view);
        return view;
    }
    Texture load(const std::string& path) {
        Texture result;result.used=frame;
        ImageAsset image;
        if(!reported)log(std::format("PNGII loading artwork: {}",path));
        try{image=decodePNGIIImage(path,read(path));}catch(const std::exception&){}
        if(image) {
            result.original=upload(image);
            for(std::size_t i=0;i<image.pixels.size();i+=4)image.pixels[i]=image.pixels[i+1]=image.pixels[i+2]=255;
            result.mask=upload(image);
        }
        if(!result.original||!result.mask)log(std::format("PNGII artwork unavailable: {}; using I4 artwork",path));
        else if(!reported){log(std::format("PNGII artwork ready: {}",path));reported=true;}
        return result;
    }
    void beginFrame(bool use,std::uint64_t currentGeneration) {
        ++frame;
        if(enabled!=use||generation!=currentGeneration){++epoch;matches.clear();textures.clear();pending.clear();inFlight.clear();retryAfter=0;generation=currentGeneration;queryReported=false;}
        enabled=use;if(!enabled)return;
        if(!discovered)discover();
        if(!api)return;
        std::vector<Reply> replies;
        {std::scoped_lock lock(inbox->mutex);replies.swap(inbox->replies);}
        for(auto& reply:replies) {
            if(reply.epoch!=epoch)continue;
            auto& request=reply.request;auto& query=reply.query;inFlight.erase(request.key);
            if(query.result==PNGII::API::Result::kNotReady){retryAfter=frame+15;pending.push_back(std::move(request));continue;}
            if(query.result!=PNGII::API::Result::kSuccess&&query.result!=PNGII::API::Result::kNoMatch)
                log(std::format("PNGII icon query failed for {:08X} [{}], status {}; using I4 artwork",request.form,request.label,unsigned(query.result)));
            trim(matches,2048);matches.insert_or_assign(request.key,Match{std::move(query.path),frame});
            if(!matches.at(request.key).path.empty())pending.push_back(std::move(request));
        }
        for(unsigned work=0;work<8&&!pending.empty();++work) {
            auto request=std::move(pending.front());pending.pop_front();
            auto match=matches.find(request.key);
            if(match==matches.end()) {
                if(frame<retryAfter||inFlight.size()>=8){pending.push_front(std::move(request));break;}
                if(!inFlight.insert(request.key).second)continue;
                const bool trace=!queryReported;queryReported=true;
                if(trace)log("PNGII icon lookup queued for menu update");
                schedule([receiver=std::weak_ptr(inbox),request=std::move(request),provider=api,requestEpoch=epoch,trace,log=log]()mutable {
                    if(auto target=receiver.lock()) {
                        if(trace)log(std::format("PNGII icon lookup started: {:08X} [{}]",request.form,request.label));
                        auto query=queryPNGII(provider,request.form,request.label);
                        if(trace)log(std::format("PNGII icon lookup completed: status {}, path {}",unsigned(query.result),query.path));
                        std::scoped_lock lock(target->mutex);
                        target->replies.push_back({std::move(request),std::move(query),requestEpoch});
                    }
                });
                continue;
            }
            if(match->second.path.empty()||textures.contains(match->second.path))continue;
            trim(textures,128);textures.emplace(match->second.path,load(match->second.path));break;
        }
    }
    std::optional<ItemIcon> find(std::uint32_t form,std::string_view label) {
        if(!enabled||!api||label.empty()||label.size()>128)return {};
        const auto key=std::to_string(form)+'\n'+std::string(label);
        if(auto match=matches.find(key);match!=matches.end()) {
            match->second.used=frame;
            if(match->second.path.empty())return {};
            if(auto texture=textures.find(match->second.path);texture!=textures.end()) {
                texture->second.used=frame;
                if(texture->second.original&&texture->second.mask)
                    return ItemIcon{reinterpret_cast<ImTextureID>(texture->second.original.Get()),reinterpret_cast<ImTextureID>(texture->second.mask.Get()),{}};
                return {};
            }
        }
        if(!inFlight.contains(key)&&pending.size()<128&&std::none_of(pending.begin(),pending.end(),[&](const auto& request){return request.key==key;}))pending.push_back({form,std::string(label),key});
        return {};
    }
};
PNGIIIcons::PNGIIIcons(ID3D11Device* device,ReadResource read,Log log,UITask task):impl_(std::make_unique<Impl>(device,std::move(read),std::move(log),std::move(task))){}
PNGIIIcons::~PNGIIIcons()=default;
void PNGIIIcons::beginFrame(bool enabled,std::uint64_t generation){impl_->beginFrame(enabled,generation);}
std::optional<ItemIcon> PNGIIIcons::find(std::uint32_t form,std::string_view label){return impl_->find(form,label);}
}
