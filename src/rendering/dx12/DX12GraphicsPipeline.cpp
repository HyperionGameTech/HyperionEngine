/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12Shader.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>
#include <rendering/dx12/DX12DescriptorSet.hpp>

#include <rendering/Vertex.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <DX12GraphicsPipeline.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12GraphicsPipeline

DX12GraphicsPipeline::DX12GraphicsPipeline()
    : GraphicsPipelineBase()
{
}

DX12GraphicsPipeline::DX12GraphicsPipeline(const DX12ShaderRef& shader)
    : GraphicsPipelineBase()
{
    m_shader = shader;
}

DX12GraphicsPipeline::~DX12GraphicsPipeline()
{
}

bool DX12GraphicsPipeline::IsCreated() const
{
    return false;
}

RendererResult DX12GraphicsPipeline::Create()
{
    return Rebuild();
}

void DX12GraphicsPipeline::Bind(CommandBuffer* cmd)
{
    // @TODO
}

void DX12GraphicsPipeline::Bind(CommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent)
{
    // @TODO
}

void DX12GraphicsPipeline::SetPushConstants(const void* data, SizeType size)
{
    // @TODO
}

RendererResult DX12GraphicsPipeline::Rebuild()
{
    // @TODO
    return {};
}

RendererResult DX12GraphicsPipeline::BuildRootSignature()
{
    m_rootSignature.Reset();

    Assert(m_shader != nullptr && m_shader->GetCompiledShader() != nullptr);

    const DescriptorTableDeclaration* decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(decl != nullptr);

    Array<D3D12_ROOT_PARAMETER> rootParams;

    // use LL so we never invalid ptrs
    LinkedList<Array<D3D12_DESCRIPTOR_RANGE>> rangeAllocations;

    auto AllocateRangeStorage = [&](const Array<D3D12_DESCRIPTOR_RANGE>& newRanges) -> const D3D12_DESCRIPTOR_RANGE*
    {
        if (newRanges.Empty())
            return nullptr;

        rangeAllocations.PushBack(newRanges);
        return rangeAllocations.Back().Data();
    };

    for (SizeType setIndex = 0; setIndex < decl->elements.Size(); ++setIndex)
    {
        const DescriptorSetDeclaration& setDecl = decl->elements[setIndex];

        Array<D3D12_DESCRIPTOR_RANGE> viewRanges;
        Array<D3D12_DESCRIPTOR_RANGE> samplerRanges;

        for (uint32 slotTypeIndex = DESCRIPTOR_SLOT_NONE + 1; slotTypeIndex < DESCRIPTOR_SLOT_MAX; ++slotTypeIndex)
        {
            const auto& declarations = setDecl.slots[slotTypeIndex];
            
            if (declarations.Empty())
            {
                continue;
            }
            
            D3D12_DESCRIPTOR_RANGE_TYPE rangeType;

            switch (slotTypeIndex)
            {
            case DESCRIPTOR_SLOT_SRV:
            case DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE:
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; 
                break;
            case DESCRIPTOR_SLOT_UAV: // storage texture
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                break;
            case DESCRIPTOR_SLOT_CBUFF:
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                break;
            case DESCRIPTOR_SLOT_SSBO:
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; 
                break;
            case DESCRIPTOR_SLOT_SAMPLER:
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                break;
            default:
                HYP_UNREACHABLE();
            }

            for (const DescriptorDeclaration& descDecl : declarations)
            {
                D3D12_DESCRIPTOR_RANGE range {};
                range.RangeType = rangeType;
                range.NumDescriptors = descDecl.count;
                range.BaseShaderRegister = descDecl.index;
                range.RegisterSpace = (UINT)setIndex;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                if (slotTypeIndex == DESCRIPTOR_SLOT_SAMPLER)
                {
                    samplerRanges.PushBack(range);
                }
                else
                {
                    viewRanges.PushBack(range);
                }
            }
        }

        if (viewRanges.Any())
        {
            D3D12_ROOT_PARAMETER& param = rootParams.EmplaceBack();
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.DescriptorTable.NumDescriptorRanges = (UINT)viewRanges.Size();
            param.DescriptorTable.pDescriptorRanges = AllocateRangeStorage(viewRanges);
        }

        if (samplerRanges.Any())
        {
            D3D12_ROOT_PARAMETER& param = rootParams.EmplaceBack();
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.DescriptorTable.NumDescriptorRanges = (UINT)samplerRanges.Size();
            param.DescriptorTable.pDescriptorRanges = AllocateRangeStorage(samplerRanges);
        }
    }

    D3D12_ROOT_SIGNATURE_DESC sigDesc = {};
    sigDesc.NumParameters = (UINT)rootParams.Size();
    sigDesc.pParameters = rootParams.Data();
    sigDesc.NumStaticSamplers = 0;
    sigDesc.pStaticSamplers = nullptr;
    sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    
    HRESULT res = D3D12SerializeRootSignature(&sigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

    if (FAILED(res))
    {
        const char* errStr = error ? (const char*)error->GetBufferPointer() : "Unknown";

        return HYP_MAKE_ERROR(RendererError, "Root Signature Serialization Failed! {}", res, errStr);
    }

    res = g_renderBackend->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(ID3D12RootSignature), &m_rootSignature);

    if (FAILED(res))
    {
        return HYP_MAKE_ERROR(RendererError, "CreateRootSignature failed", res);
    }

    return {};
}

#ifdef HYP_DEBUG_MODE
void DX12GraphicsPipeline::SetDebugName(Name name)
{
    GraphicsPipelineBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GraphicsPipeline

} // namespace Hyperion
