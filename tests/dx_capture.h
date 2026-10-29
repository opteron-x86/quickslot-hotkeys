#pragma once
#include "DXContextState.h"
#include <d3dcompiler.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

// Draw using cached inputs, as an engine renderer can do between adjacent UI movies.
void iconCapturePixels(ID3D11Device* device,ID3D11DeviceContext* c) {
    using Microsoft::WRL::ComPtr;
    std::cout<<std::unitbuf;
    constexpr UINT width=37,height=23;
    const char code[]=
        "cbuffer Position : register(b4) { float4 placement; };"
        "cbuffer Color : register(b5) { float4 tint; };"
        "Texture2D source : register(t3); SamplerState pointSampler : register(s2);"
        "float4 vs(float2 pos : POSITION) : SV_Position { return float4(pos*placement.xy+placement.zw,0,1); }"
        "float4 ps() : SV_Target { return source.Sample(pointSampler,float2(0.5,0.5))*tint; }";
    ComPtr<ID3DBlob> vs,ps;
    check(SUCCEEDED(D3DCompile(code,std::strlen(code),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,nullptr)),"Compile capture vertex shader");
    check(SUCCEEDED(D3DCompile(code,std::strlen(code),nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,nullptr)),"Compile capture pixel shader");
    ComPtr<ID3D11VertexShader> vertex;ComPtr<ID3D11PixelShader> pixel;ComPtr<ID3D11InputLayout> layout;
    check(SUCCEEDED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex)),"Create capture vertex shader");
    check(SUCCEEDED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel)),"Create capture pixel shader");
    const D3D11_INPUT_ELEMENT_DESC element{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};
    check(SUCCEEDED(device->CreateInputLayout(&element,1,vs->GetBufferPointer(),vs->GetBufferSize(),&layout)),"Create capture input layout");
    const auto buffer=[&](const void* data,UINT bytes,UINT flags) {
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=bytes;desc.BindFlags=flags;
        D3D11_SUBRESOURCE_DATA initial{data,0,0};ComPtr<ID3D11Buffer> result;
        check(SUCCEEDED(device->CreateBuffer(&desc,&initial,&result)),"Create capture input buffer");return result;
    };
    const float positions[]{0,0,-1,1,3,1,-1,-3},transform[]{1,1,0,0};
    const std::uint16_t indices[]{0,0,1,2};
    auto vertices=buffer(positions,sizeof(positions),D3D11_BIND_VERTEX_BUFFER);
    auto index=buffer(indices,sizeof(indices),D3D11_BIND_INDEX_BUFFER);
    auto position=buffer(transform,sizeof(transform),D3D11_BIND_CONSTANT_BUFFER);
    std::array<float,128> colors{};auto color=buffer(colors.data(),sizeof(colors),D3D11_BIND_CONSTANT_BUFFER);
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=1;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    const std::array<std::uint8_t,4> texel{64,128,192,255};D3D11_SUBRESOURCE_DATA initial{texel.data(),4,0};
    ComPtr<ID3D11Texture2D> source;ComPtr<ID3D11ShaderResourceView> resource;
    check(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&source))&&SUCCEEDED(device->CreateShaderResourceView(source.Get(),nullptr,&resource)),"Create capture source texture");
    D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;check(SUCCEEDED(device->CreateSamplerState(&sd,&sampler)),"Create capture sampler");
    D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.ScissorEnable=TRUE;rd.DepthClipEnable=TRUE;
    ComPtr<ID3D11RasterizerState> raster;check(SUCCEEDED(device->CreateRasterizerState(&rd,&raster)),"Create capture rasterizer");
    desc.Width=width;desc.Height=height;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> host,target,staging;ComPtr<ID3D11RenderTargetView> hostView,targetView;
    check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&host))&&SUCCEEDED(device->CreateRenderTargetView(host.Get(),nullptr,&hostView)),"Create capture host target");
    check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&target))&&SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,&targetView)),"Create capture destination");
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"Create capture readback");
    ComPtr<ID3D11DeviceContext1> extended;check(SUCCEEDED(c->QueryInterface(IID_PPV_ARGS(&extended))),"Get extended capture context");
    D3D11_FEATURE_DATA_D3D11_OPTIONS options{};
    const bool ranges=SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS,&options,sizeof(options)))&&options.ConstantBufferOffsetting;
    quickslot::DXContextState state(device,c);check(state.available(),"Create capture state");
    for(unsigned frame=0;frame<4;++frame) {
        const bool blankControl=frame==0,useRange=ranges&&frame==2;
        colors.fill(0);const unsigned start=useRange?64:0;
        colors[start]=float(frame+1)/4;colors[start+1]=colors[start+2]=colors[start+3]=1;
        c->UpdateSubresource(color.Get(),0,nullptr,colors.data(),0,0);
        c->ClearState();c->VSSetShader(vertex.Get(),nullptr,0);c->PSSetShader(pixel.Get(),nullptr,0);
        auto* vb=vertices.Get();const UINT stride=8,offset=8;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        c->IASetInputLayout(layout.Get());c->IASetIndexBuffer(index.Get(),DXGI_FORMAT_R16_UINT,2);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        auto* pcb=position.Get();auto* ccb=color.Get();c->VSSetConstantBuffers(4,1,&pcb);
        const UINT first=16,count=16;
        if(useRange)extended->PSSetConstantBuffers1(5,1,&ccb,&first,&count);else c->PSSetConstantBuffers(5,1,&ccb);
        auto* srv=resource.Get();auto* ss=sampler.Get();c->PSSetShaderResources(3,1,&srv);c->PSSetSamplers(2,1,&ss);
        c->RSSetState(raster.Get());const D3D11_RECT scissor{2,3,width-2,height-3};c->RSSetScissorRects(1,&scissor);
        auto* hv=hostView.Get();c->OMSetRenderTargets(1,&hv,nullptr);
        const D3D11_VIEWPORT hostViewport{1,2,31,17,0,1};c->RSSetViewports(1,&hostViewport);
        // Exercise the D3D11.0 constant-buffer path independently of driver capabilities.
        if(frame==3){quickslot::DXGraphicsBindings inputs(extended.Get(),false);c->ClearState();inputs.apply(extended.Get());c->OMSetRenderTargets(1,&hv,nullptr);c->RSSetViewports(1,&hostViewport);}
        {
            quickslot::DXContextState::Frame capture(state);
            ComPtr<ID3D11VertexShader> activeVS;ComPtr<ID3D11PixelShader> activePS;
            c->VSGetShader(&activeVS,nullptr,nullptr);c->PSGetShader(&activePS,nullptr,nullptr);
            check(activeVS.Get()==vertex.Get()&&activePS.Get()==pixel.Get(),"Capture inherits cached graphics shaders");
            auto* tv=targetView.Get();c->OMSetRenderTargets(1,&tv,nullptr);
            const D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};c->RSSetViewports(1,&viewport);
            const float clear[]{0,0,0,0};c->ClearRenderTargetView(targetView.Get(),clear);
            if(blankControl){ID3D11Buffer* missing=nullptr;c->PSSetConstantBuffers(5,1,&missing);}
            c->DrawIndexed(3,0,0);c->CopyResource(staging.Get(),target.Get());
            c->ClearState();
        }
        ComPtr<ID3D11Buffer> restoredColor;ComPtr<ID3D11ShaderResourceView> restoredSource;ComPtr<ID3D11SamplerState> restoredSampler;
        ComPtr<ID3D11RenderTargetView> restoredTarget;
        c->PSGetConstantBuffers(5,1,&restoredColor);c->PSGetShaderResources(3,1,&restoredSource);c->PSGetSamplers(2,1,&restoredSampler);c->OMGetRenderTargets(1,&restoredTarget,nullptr);
        check(restoredColor.Get()==color.Get()&&restoredSource.Get()==resource.Get()&&restoredSampler.Get()==sampler.Get()&&restoredTarget.Get()==hostView.Get(),"Capture restores host resources and target after ClearState");
        if(useRange){UINT restoredFirst=0,restoredCount=0;ComPtr<ID3D11Buffer> rangedBuffer;extended->PSGetConstantBuffers1(5,1,&rangedBuffer,&restoredFirst,&restoredCount);check(rangedBuffer.Get()==color.Get()&&restoredFirst==first&&restoredCount==count,"Capture restores constant-buffer ranges");}
        UINT viewportCount=1;D3D11_VIEWPORT restoredViewport{};c->RSGetViewports(&viewportCount,&restoredViewport);
        check(viewportCount==1&&std::memcmp(&restoredViewport,&hostViewport,sizeof(hostViewport))==0,"Capture restores host viewport");
        D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Read capture pixels");
        unsigned visible=0,colored=0,bad=0;
        for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x) {
            const auto* p=static_cast<const std::uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4;
            visible+=p[3]!=0;colored+=bool(p[0]||p[1]||p[2]);
            if(p[3])bad+=p[0]!=16*(frame+1)||p[1]!=128||p[2]!=192||p[3]!=255;
        }
        c->Unmap(staging.Get(),0);
        const auto expected=blankControl?0:(width-4)*(height-6);
        check(visible==expected&&colored==expected&&bad==0,"Capture produces expected color and alpha using cached inputs");
        std::cout<<"PASS: capture frame "<<frame<<", alpha="<<visible<<", RGB="<<colored<<", buffer_range="<<useRange<<'\n';
    }
    c->ClearState();
}
