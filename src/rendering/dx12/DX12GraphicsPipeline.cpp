/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12Shader.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12Framebuffer.hpp>
#include <rendering/dx12/DX12Attachment.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/Vertex.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <DX12GraphicsPipeline.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

static D3D12_RASTERIZER_DESC GetDefaultRasterizerDesc()
{
    D3D12_RASTERIZER_DESC desc {};
    desc.FillMode = D3D12_FILL_MODE_SOLID;
    desc.CullMode = D3D12_CULL_MODE_BACK;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    desc.DepthClipEnable = TRUE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;
    desc.ForcedSampleCount = 0;
    desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    return desc;
}

static D3D12_DEPTH_STENCIL_DESC GetDefaultDepthStencilDesc()
{
    D3D12_DEPTH_STENCIL_DESC desc {};
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.StencilEnable = FALSE;
    desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

    desc.FrontFace = {
        D3D12_STENCIL_OP_KEEP,
        D3D12_STENCIL_OP_KEEP,
        D3D12_STENCIL_OP_KEEP,
        D3D12_COMPARISON_FUNC_ALWAYS
    };
    desc.BackFace = desc.FrontFace;

    return desc;
}

static bool GetSemanticNameAndIndex(const VertexAttribute& attribute, const char*& outNameStr, int& outIndex)
{
    outNameStr = nullptr;
    outIndex = 0;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    if (std::strcmp(attribute.name, VertexAttribute::Position.name) == 0)
    {
        outNameStr = "POSITION";
        return true;
    }
    else if (std::strcmp(attribute.name, VertexAttribute::Normal.name) == 0)
    {
        outNameStr = "NORMAL";
        return true;
    }
    else if (std::strcmp(attribute.name, VertexAttribute::TexCoord0.name) == 0)
    {
        outNameStr = "TEXCOORD";
        return true;
    }
    else if (std::strcmp(attribute.name, VertexAttribute::TexCoord1.name) == 0)
    {
        outNameStr = "TEXCOORD";
        outIndex = 1;
        return true;
    }
    else if (std::strcmp(attribute.name, VertexAttribute::Tangent.name) == 0)
    {
        outNameStr = "TANGENT";
        return true;
    }
    else if (std::strcmp(attribute.name, VertexAttribute::Bitangent.name) == 0)
    {
        outNameStr = "BINORMAL";
        return true;
    }
    else if (std::strcmp(attribute.name, VertexAttribute::BoneIndices.name) == 0)
    {
        outNameStr = "BLENDINDICES";
        return true;
    }
    else if (std::strcmp(attribute.name, VertexAttribute::BoneWeights.name) == 0)
    {
        outNameStr = "BLENDWEIGHT";
        return true;
    }

    return false;
}

static DXGI_FORMAT VertexAttributeTypeToDXGIFormat(const VertexAttribute& attribute)
{
    if (std::strcmp(attribute.name, VertexAttribute::BoneIndices.name) == 0)
    {
        return DXGI_FORMAT_R32G32B32A32_UINT;
    }

    switch (attribute.size)
    {
    case 16:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case 12:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case 8:
        return DXGI_FORMAT_R32G32_FLOAT;
    case 4:
        return DXGI_FORMAT_R32_FLOAT;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

static RendererResult BuildInputElementDesc(const VertexAttributeSet& vertexAttributes, Array<D3D12_INPUT_ELEMENT_DESC>& outInputElementDescs)
{
    outInputElementDescs.Clear();

    if (vertexAttributes == 0)
        return HYP_MAKE_ERROR(RendererError, "VertexAttributes empty! Cannot create input element descs");

    const Array<const VertexAttribute*> attributes = vertexAttributes.BuildAttributes();

    FlatMap<uint32, uint32> bindingOffsets;

    for (const VertexAttribute* attribute : attributes)
    {
        AssertDebug(attribute != nullptr);

        const char* semanticName = nullptr;
        int semanticIndex = -1;

        if (!GetSemanticNameAndIndex(*attribute, semanticName, semanticIndex))
            return HYP_MAKE_ERROR(RendererError, "Failed to get semantic name/index for attribute type {}", 0, attribute->name);

        AssertDebug(semanticName != nullptr && semanticIndex >= 0);

        D3D12_INPUT_ELEMENT_DESC& desc = outInputElementDescs.EmplaceBack();
        desc.SemanticName = semanticName;
        desc.SemanticIndex = UINT(semanticIndex);
        desc.Format = VertexAttributeTypeToDXGIFormat(*attribute);
        desc.InputSlot = attribute->binding;
        desc.AlignedByteOffset = bindingOffsets[attribute->binding];
        desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        desc.InstanceDataStepRate = 0;

        bindingOffsets[attribute->binding] += attribute->size;
    }

    return {};
}

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
    CheckResultOrReturn(BuildRootSignature());

    Assert(m_rootSignature != nullptr);
    Assert(m_shader != nullptr);

    Array<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
    CheckResultOrReturn(BuildInputElementDesc(m_vertexAttributes, inputElementDescs));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.PrimitiveTopologyType = ToDX12TopologyType(m_topology);

    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    DX12Shader* dxShader = static_cast<DX12Shader*>(m_shader.Get());
    psoDesc.VS = dxShader->GetShaderBytecode(SMT_VERTEX);
    psoDesc.PS = dxShader->GetShaderBytecode(SMT_FRAGMENT);
    psoDesc.GS = dxShader->GetShaderBytecode(SMT_GEOMETRY);

    psoDesc.InputLayout = { inputElementDescs.Data(), (UINT)inputElementDescs.Size() };

    psoDesc.RasterizerState = GetDefaultRasterizerDesc(); 
    psoDesc.RasterizerState.CullMode = ToDX12CullMode(m_faceCullMode);
    psoDesc.RasterizerState.FillMode = (m_fillMode == FM_LINE) ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE; 

    psoDesc.DepthStencilState = GetDefaultDepthStencilDesc();
    psoDesc.DepthStencilState.DepthEnable = m_depthTest;
    psoDesc.DepthStencilState.DepthWriteMask = m_depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    
    if (m_stencilFunction.HasValue())
    {
        psoDesc.DepthStencilState.StencilEnable = TRUE;
        psoDesc.DepthStencilState.StencilReadMask = 0xFF;
        psoDesc.DepthStencilState.StencilWriteMask = 0xFF;

        D3D12_DEPTH_STENCILOP_DESC stencilOp {};
        stencilOp.StencilFailOp = ToDX12StencilOp(m_stencilFunction->failOp);
        stencilOp.StencilPassOp = ToDX12StencilOp(m_stencilFunction->passOp);
        stencilOp.StencilDepthFailOp = ToDX12StencilOp(m_stencilFunction->depthFailOp);
        stencilOp.StencilFunc = ToDX12ComparisonFunction(m_stencilFunction->compareOp);

        psoDesc.DepthStencilState.FrontFace = stencilOp;
        psoDesc.DepthStencilState.BackFace = stencilOp;
    }

    psoDesc.NumRenderTargets = 0;
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;

    if (m_renderTargetDesc.numAttachments > 0)
    {
        bool hasDSV = false;

        for (uint32 attachmentIndex = 0; attachmentIndex < m_renderTargetDesc.numAttachments; attachmentIndex++)
        {
            const AttachmentDesc& attachmentDesc = m_renderTargetDesc.attachments[attachmentIndex];

            if (TextureUtils::IsDepthFormat(attachmentDesc.format))
            {
                if (hasDSV)
                    return HYP_MAKE_ERROR(RendererError, "Pipeline cannot have multiple depth stencil targets!");

                psoDesc.DSVFormat = ToDXGIFormat(attachmentDesc.format, DX12ViewType::RTV_DSV);
                hasDSV = true;

                continue;
            }

            if (psoDesc.NumRenderTargets >= 8)
                return HYP_MAKE_ERROR(RendererError, "To many render targets for pipeline {}!", 0, GetDebugName());

            const bool blendEnabled = (m_blendFunction != BlendFunction::None());

            const uint32 rtIndex = psoDesc.NumRenderTargets++;

            D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = psoDesc.BlendState.RenderTarget[rtIndex];
            rtBlend = {};
            rtBlend.BlendEnable = blendEnabled;
            rtBlend.SrcBlend = ToDX12Blend(m_blendFunction.GetSrcColor());
            rtBlend.DestBlend = ToDX12Blend(m_blendFunction.GetDstColor());
            rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
            rtBlend.SrcBlendAlpha = ToDX12Blend(m_blendFunction.GetSrcAlpha());
            rtBlend.DestBlendAlpha = ToDX12Blend(m_blendFunction.GetDstAlpha());
            rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            rtBlend.LogicOpEnable = FALSE;
            rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            psoDesc.RTVFormats[rtIndex] = ToDXGIFormat(attachmentDesc.format, DX12ViewType::RTV_DSV);
        }
    }

    HRESULT res = g_renderBackend->GetDevice()->CreateGraphicsPipelineState(
        &psoDesc, 
        __uuidof(ID3D12PipelineState),
        &m_pipelineState
    );

    if (FAILED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create DX12 Pipeline State for {}", res, GetDebugName());

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

        for (uint32 slotTypeIndex = NONE + 1; slotTypeIndex < MAX; ++slotTypeIndex)
        {
            const auto& declarations = setDecl.slots[slotTypeIndex];
            
            if (declarations.Empty())
            {
                continue;
            }
            
            D3D12_DESCRIPTOR_RANGE_TYPE rangeType;

            switch (slotTypeIndex)
            {
            case SRV:
            case ACCELERATION_STRUCTURE:
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; 
                break;
            case UAV: // storage texture
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                break;
            case CBUFF:
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                break;
            case SSBO:
                rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; 
                break;
            case SAMPLER:
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

                if (slotTypeIndex == SAMPLER)
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
