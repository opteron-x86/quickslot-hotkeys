#pragma once
#include <imgui.h>
#include <d3d11_1.h>
#include <wrl/client.h>
#include <array>
#include <stdexcept>

namespace preview {

class DXIsolation {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11DeviceContext> context;
    UINT uavCount=8;
public:
    DXIsolation(ID3D11Device* device,ID3D11DeviceContext* immediate):context(immediate) {
        if(!device||!immediate)throw std::runtime_error("DirectX context unavailable");
        if(device->GetFeatureLevel()>=D3D_FEATURE_LEVEL_11_1)uavCount=D3D11_1_UAV_SLOT_COUNT;
    }
    class Frame {
        DXIsolation& owner;
        std::array<ID3D11RenderTargetView*,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets{};
        Ptr<ID3D11DepthStencilView> depth;
        std::array<ID3D11UnorderedAccessView*,D3D11_1_UAV_SLOT_COUNT> uavs{};
        Ptr<ID3D11HullShader> hull;Ptr<ID3D11DomainShader> domain;Ptr<ID3D11ComputeShader> compute;
        std::array<ID3D11ClassInstance*,256> hullInstances{},domainInstances{},computeInstances{};
        UINT hullCount=256,domainCount=256,computeCount=256,viewportCount=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        std::array<D3D11_VIEWPORT,D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
    public:
        explicit Frame(DXIsolation& value,bool requireTarget=true):owner(value) {
            auto* c=owner.context.Get();
            c->OMGetRenderTargets(UINT(targets.size()),targets.data(),depth.GetAddressOf());
            if(!targets[0]&&requireTarget){for(auto* p:targets)if(p)p->Release();throw std::runtime_error("Skyrim has no active UI render target");}
            c->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,owner.uavCount,uavs.data());
            c->HSGetShader(hull.GetAddressOf(),hullInstances.data(),&hullCount);
            c->DSGetShader(domain.GetAddressOf(),domainInstances.data(),&domainCount);
            c->CSGetShader(compute.GetAddressOf(),computeInstances.data(),&computeCount);
            c->RSGetViewports(&viewportCount,viewports.data());
        }
        ~Frame() {
            auto* c=owner.context.Get();UINT count=UINT(targets.size());while(count&&!targets[count-1])--count;
            std::array<UINT,D3D11_1_UAV_SLOT_COUNT> counts;counts.fill(UINT(-1));
            c->OMSetRenderTargetsAndUnorderedAccessViews(count,targets.data(),depth.Get(),count,owner.uavCount-count,uavs.data()+count,counts.data()+count);
            c->HSSetShader(hull.Get(),hullInstances.data(),hullCount);
            c->DSSetShader(domain.Get(),domainInstances.data(),domainCount);
            c->CSSetShader(compute.Get(),computeInstances.data(),computeCount);
            c->RSSetViewports(viewportCount,viewports.data());
            for(auto* p:targets)if(p)p->Release();for(auto* p:uavs)if(p)p->Release();
            for(auto* p:hullInstances)if(p)p->Release();for(auto* p:domainInstances)if(p)p->Release();for(auto* p:computeInstances)if(p)p->Release();
        }
        Frame(const Frame&)=delete;Frame& operator=(const Frame&)=delete;
        void bindTarget() {}
        ImVec2 targetSize()const {
            if(!targets[0])return {};
            Ptr<ID3D11Resource> resource;Ptr<ID3D11Texture2D> texture;targets[0]->GetResource(resource.GetAddressOf());
            if(FAILED(resource.As(&texture)))return {};
            D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);return {float(desc.Width),float(desc.Height)};
        }
    };
};
}
