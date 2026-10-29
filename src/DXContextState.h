#pragma once
#include "DXGraphicsBindings.h"
#include <d3d11_1.h>
#include <wrl/client.h>

namespace quickslot {
// Scaleform can touch every pipeline stage while rendering a movie.
class DXContextState {
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context_;
    Microsoft::WRL::ComPtr<ID3DDeviceContextState> state_;
    bool bufferRanges_=false;
public:
    DXContextState(ID3D11Device* device,ID3D11DeviceContext* context) {
        Microsoft::WRL::ComPtr<ID3D11Device1> extended;
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(&extended)))||FAILED(context->QueryInterface(IID_PPV_ARGS(&context_))))return;
        const auto level=device->GetFeatureLevel();D3D_FEATURE_LEVEL chosen;
        extended->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&state_);
        D3D11_FEATURE_DATA_D3D11_OPTIONS options{};
        if(SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS,&options,sizeof(options))))bufferRanges_=options.ConstantBufferOffsetting;
    }
    bool available()const{return context_&&state_;}
    class Frame {
        DXContextState& owner_;
        Microsoft::WRL::ComPtr<ID3DDeviceContextState> previous_;
    public:
        explicit Frame(DXContextState& owner):owner_(owner){
            DXGraphicsBindings inputs(owner_.context_.Get(),owner_.bufferRanges_);
            owner_.context_->SwapDeviceContextState(owner_.state_.Get(),&previous_);
            owner_.context_->ClearState();
            inputs.apply(owner_.context_.Get());
        }
        ~Frame(){owner_.context_->SwapDeviceContextState(previous_.Get(),nullptr);}
        Frame(const Frame&)=delete;Frame& operator=(const Frame&)=delete;
    };
};
}
