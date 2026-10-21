#include "DXIsolation.h"
#include <imgui_impl_dx11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <cstring>
#include <limits>
using Microsoft::WRL::ComPtr;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
namespace {
struct Instance {
    unsigned released=0;
    void Release()noexcept{++released;}
};
void shaderInstanceOwnership() {
    using namespace preview;
    Instance active,unreported,last;
    std::array<Instance*,4> slots{&unreported,nullptr,nullptr,nullptr};
    check(UnexpectedShaderInstances(slots,0),"Detect populated slots with zero returned instances");
    ReleaseShaderInstances(slots,0);
    check(unreported.released==0,"Do not Release an unreported slot");

    slots={&active,nullptr,&unreported,&last};
    ReleaseShaderInstances(slots,1);
    check(active.released==1&&slots[0]==nullptr,"Release a captured instance exactly once");
    check(unreported.released==0&&last.released==0,"Ignore populated slots outside returned count");
    ReleaseShaderInstances(slots,1);
    check(active.released==1,"Repeated cleanup does not double-release");

    slots={nullptr,&active,nullptr,&last};
    check(!UnexpectedShaderInstances(slots,4),"Valid captured range is accepted");
    ReleaseShaderInstances(slots,4);
    check(active.released==2&&last.released==1,"Release full valid range and tolerate null slots");

    slots={&active,nullptr,nullptr,nullptr};
    check(UnexpectedShaderInstances(slots,999),"Detect out-of-range driver counts");
    check(ShaderInstanceCount(slots,(std::numeric_limits<unsigned>::max)())==4,"Clamp count to output capacity");
    ReleaseShaderInstances(slots,(std::numeric_limits<unsigned>::max)());
    check(active.released==3,"Oversized count cannot walk off array");

    std::array<Instance*,4> legacy{&unreported,nullptr,nullptr,nullptr};
    for(auto* instance:legacy)if(instance)instance->Release();
    check(unreported.released==1,"Legacy full-array cleanup releases a zero-count unowned output");
}
}
int main()try {
    shaderInstanceOwnership();
    std::cout<<"PASS: shader class-instance cleanup respects reported counts; legacy regression reproduced\n";
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};
    HRESULT result=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,1,D3D11_SDK_VERSION,&device,&level,&context);
    if(FAILED(result))result=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,1,D3D11_SDK_VERSION,&device,&level,&context);
    check(SUCCEEDED(result),"Create a D3D11 test device");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=320;desc.Height=180;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture,other;ComPtr<ID3D11RenderTargetView> target,second;
    check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture))&&SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&target)),"Create target");
    check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&other))&&SUCCEEDED(device->CreateRenderTargetView(other.Get(),nullptr,&second)),"Create second target");
    ComPtr<ID3DBlob> compiled;ComPtr<ID3D11ComputeShader> shader;
    const char source[]="RWStructuredBuffer<uint> output : register(u0); [numthreads(1,1,1)] void main(uint3 id : SV_DispatchThreadID) { output[id.x]=7; }";
    check(SUCCEEDED(D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&compiled,nullptr)),"Compile test shader");
    check(SUCCEEDED(device->CreateComputeShader(compiled->GetBufferPointer(),compiled->GetBufferSize(),nullptr,&shader)),"Create test shader");
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=64;bd.BindFlags=D3D11_BIND_UNORDERED_ACCESS;bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;bd.StructureByteStride=4;
    ComPtr<ID3D11Buffer> buffer;ComPtr<ID3D11UnorderedAccessView> uav;
    check(SUCCEEDED(device->CreateBuffer(&bd,nullptr,&buffer)),"Create UAV buffer");
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;ud.Buffer.NumElements=16;ud.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_APPEND;
    check(SUCCEEDED(device->CreateUnorderedAccessView(buffer.Get(),&ud,&uav)),"Create append UAV");
    ID3D11RenderTargetView* rt[]{target.Get(),second.Get()};ID3D11UnorderedAccessView* uv=uav.Get();UINT initial=5;
    context->OMSetRenderTargetsAndUnorderedAccessViews(2,rt,nullptr,2,1,&uv,&initial);context->CSSetShader(shader.Get(),nullptr,0);
    D3D11_VIEWPORT viewport{3,4,300,170,.1f,.9f};context->RSSetViewports(1,&viewport);
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={320,180};io.DeltaTime=1.f/60;
    check(ImGui_ImplDX11_Init(device.Get(),context.Get()),"Initialize renderer backend");
    preview::DXIsolation isolation(device.Get(),context.Get());
    for(unsigned frame=0;frame<3;++frame) {
        {preview::DXIsolation::Frame scope(isolation);check(scope.targetSize().x==320,"Use actual UI target size");
         ImGui_ImplDX11_NewFrame();ImGui::NewFrame();ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},{30,30},IM_COL32_WHITE);ImGui::Render();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
         if(frame==1){context->OMSetRenderTargets(0,nullptr,nullptr);context->RSSetViewports(0,nullptr);}
        }
        ID3D11RenderTargetView* restored[2]{};context->OMGetRenderTargets(2,restored,nullptr);
        check(restored[0]==rt[0]&&restored[1]==rt[1],"Restore every engine render target");for(auto* p:restored)p->Release();
        ComPtr<ID3D11ComputeShader> active;context->CSGetShader(&active,nullptr,nullptr);check(active.Get()==shader.Get(),"Restore compute shader cleared by ImGui");
        ComPtr<ID3D11UnorderedAccessView> restoredUAV;context->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,2,1,&restoredUAV);check(restoredUAV.Get()==uav.Get(),"Restore output UAV bindings");
        UINT count=1;D3D11_VIEWPORT actual{};context->RSGetViewports(&count,&actual);check(count==1&&std::memcmp(&actual,&viewport,sizeof(viewport))==0,"Restore viewport across drawing and target changes");
    }
    D3D11_BUFFER_DESC read{};read.ByteWidth=4;read.Usage=D3D11_USAGE_STAGING;read.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;check(SUCCEEDED(device->CreateBuffer(&read,nullptr,&staging)),"Create counter readback");context->CopyStructureCount(staging.Get(),0,uav.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Read UAV counter");const auto counter=*static_cast<UINT*>(mapped.pData);context->Unmap(staging.Get(),0);check(counter==5,"State restoration preserves UAV counters");
    ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();
    std::cout<<"PASS: real D3D11 rendering preserves MRTs, UAVs/counter, compute shader and viewport across frames\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
