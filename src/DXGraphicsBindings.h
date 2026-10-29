#pragma once
#include "DXShaderInstances.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <array>

namespace quickslot {
// Inputs inherited by a Scaleform draw. The caller supplies its output target and viewport.
class DXGraphicsBindings {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    template<class T,std::size_t N> struct References {
        std::array<T*,N> values{};
        ~References(){for(auto* value:values)if(value)value->Release();}
        T** data(){return values.data();}
        UINT size()const{return UINT(N);}
    };
    template<class Shader> struct Stage {
        Ptr<Shader> shader;
        std::array<ID3D11ClassInstance*,256> instances{};
        UINT instanceCount=UINT(instances.size());
        References<ID3D11Buffer,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> buffers;
        std::array<UINT,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> first{},count{};
        References<ID3D11ShaderResourceView,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> resources;
        References<ID3D11SamplerState,D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> samplers;
        ~Stage(){preview::ReleaseShaderInstances(instances,instanceCount);}
        void capture(ID3D11DeviceContext1* c,bool ranges,auto getShader,auto getBuffers,auto getBuffers1,auto getResources,auto getSamplers) {
            (c->*getShader)(shader.GetAddressOf(),instances.data(),&instanceCount);
            if(ranges)(c->*getBuffers1)(0,buffers.size(),buffers.data(),first.data(),count.data());
            else (c->*getBuffers)(0,buffers.size(),buffers.data());
            (c->*getResources)(0,resources.size(),resources.data());
            (c->*getSamplers)(0,samplers.size(),samplers.data());
        }
        void apply(ID3D11DeviceContext1* c,bool ranges,auto setShader,auto setBuffers,auto setBuffers1,auto setResources,auto setSamplers) {
            (c->*setShader)(shader.Get(),instanceCount?instances.data():nullptr,UINT(preview::ShaderInstanceCount(instances,instanceCount)));
            if(ranges)(c->*setBuffers1)(0,buffers.size(),buffers.data(),first.data(),count.data());
            else (c->*setBuffers)(0,buffers.size(),buffers.data());
            (c->*setResources)(0,resources.size(),resources.data());
            (c->*setSamplers)(0,samplers.size(),samplers.data());
        }
    };
    Stage<ID3D11VertexShader> vertex;
    Stage<ID3D11HullShader> hull;
    Stage<ID3D11DomainShader> domain;
    Stage<ID3D11GeometryShader> geometry;
    Stage<ID3D11PixelShader> pixel;
    Ptr<ID3D11InputLayout> layout;
    References<ID3D11Buffer,D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> vertices;
    std::array<UINT,D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> strides{},offsets{};
    Ptr<ID3D11Buffer> indices;
    DXGI_FORMAT indexFormat=DXGI_FORMAT_UNKNOWN;
    UINT indexOffset=0;
    D3D11_PRIMITIVE_TOPOLOGY topology=D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    Ptr<ID3D11RasterizerState> rasterizer;
    std::array<D3D11_RECT,D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors{};
    UINT scissorCount=UINT(scissors.size());
    Ptr<ID3D11BlendState> blend;
    std::array<float,4> blendFactor{};
    UINT sampleMask=0,stencilRef=0;
    Ptr<ID3D11DepthStencilState> depth;
    bool ranges_;
public:
    DXGraphicsBindings(ID3D11DeviceContext1* c,bool ranges):ranges_(ranges) {
        using C=ID3D11DeviceContext1;
        vertex.capture(c,ranges_,&C::VSGetShader,&C::VSGetConstantBuffers,&C::VSGetConstantBuffers1,&C::VSGetShaderResources,&C::VSGetSamplers);
        hull.capture(c,ranges_,&C::HSGetShader,&C::HSGetConstantBuffers,&C::HSGetConstantBuffers1,&C::HSGetShaderResources,&C::HSGetSamplers);
        domain.capture(c,ranges_,&C::DSGetShader,&C::DSGetConstantBuffers,&C::DSGetConstantBuffers1,&C::DSGetShaderResources,&C::DSGetSamplers);
        geometry.capture(c,ranges_,&C::GSGetShader,&C::GSGetConstantBuffers,&C::GSGetConstantBuffers1,&C::GSGetShaderResources,&C::GSGetSamplers);
        pixel.capture(c,ranges_,&C::PSGetShader,&C::PSGetConstantBuffers,&C::PSGetConstantBuffers1,&C::PSGetShaderResources,&C::PSGetSamplers);
        c->IAGetInputLayout(&layout);
        c->IAGetVertexBuffers(0,vertices.size(),vertices.data(),strides.data(),offsets.data());
        c->IAGetIndexBuffer(&indices,&indexFormat,&indexOffset);c->IAGetPrimitiveTopology(&topology);
        c->RSGetState(&rasterizer);c->RSGetScissorRects(&scissorCount,scissors.data());
        c->OMGetBlendState(&blend,blendFactor.data(),&sampleMask);c->OMGetDepthStencilState(&depth,&stencilRef);
    }
    void apply(ID3D11DeviceContext1* c) {
        using C=ID3D11DeviceContext1;
        vertex.apply(c,ranges_,&C::VSSetShader,&C::VSSetConstantBuffers,&C::VSSetConstantBuffers1,&C::VSSetShaderResources,&C::VSSetSamplers);
        hull.apply(c,ranges_,&C::HSSetShader,&C::HSSetConstantBuffers,&C::HSSetConstantBuffers1,&C::HSSetShaderResources,&C::HSSetSamplers);
        domain.apply(c,ranges_,&C::DSSetShader,&C::DSSetConstantBuffers,&C::DSSetConstantBuffers1,&C::DSSetShaderResources,&C::DSSetSamplers);
        geometry.apply(c,ranges_,&C::GSSetShader,&C::GSSetConstantBuffers,&C::GSSetConstantBuffers1,&C::GSSetShaderResources,&C::GSSetSamplers);
        pixel.apply(c,ranges_,&C::PSSetShader,&C::PSSetConstantBuffers,&C::PSSetConstantBuffers1,&C::PSSetShaderResources,&C::PSSetSamplers);
        c->IASetInputLayout(layout.Get());
        c->IASetVertexBuffers(0,vertices.size(),vertices.data(),strides.data(),offsets.data());
        c->IASetIndexBuffer(indices.Get(),indexFormat,indexOffset);c->IASetPrimitiveTopology(topology);
        c->RSSetState(rasterizer.Get());c->RSSetScissorRects(scissorCount,scissors.data());
        c->OMSetBlendState(blend.Get(),blendFactor.data(),sampleMask);c->OMSetDepthStencilState(depth.Get(),stencilRef);
    }
    DXGraphicsBindings(const DXGraphicsBindings&)=delete;
    DXGraphicsBindings& operator=(const DXGraphicsBindings&)=delete;
};
}
