/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12ShaderInstance.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12Framebuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Attachment.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/Vertex.hpp>
#include <rendering/Shared.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <Core/math/MathUtil.hpp>

#include <algorithm>

#include <DX12GraphicsPipeline.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface* g_renderInterface;

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

void DX12GraphicsPipeline::BuildVertexAttributes(
    Array<D3D12_INPUT_ELEMENT_DESC>& outInputElementDescs,
    Array<uint32>& outBindingStrides)
{
    static constexpr DXGI_FORMAT SizeToFormat[] = {
        DXGI_FORMAT_UNKNOWN,
        DXGI_FORMAT_R32_FLOAT,
        DXGI_FORMAT_R32G32_FLOAT,
        DXGI_FORMAT_R32G32B32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_FLOAT
    };

    const uint8 mask = m_inputLayout.mask;
    size_t vertexSize = m_inputLayout.VertexSize();

    const uint32 bits = uint32(ByteUtil::BitCount(mask));
    Assert(bits != 0);

    outInputElementDescs.Resize(bits);

    uint32 attrIndex = 0;

    size_t offset = 0;

    FOR_EACH_BIT(mask, bit)
    {
        VertexType vertexType = VertexType(1 << bit);

        if (vertexType == VT_Skeletal)
        {
            outInputElementDescs.Resize(outInputElementDescs.Size() + 1);

            outInputElementDescs[attrIndex] = {
                .SemanticName = "BLENDINDICES",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32_UINT,
                .InputSlot = 0,
                .AlignedByteOffset = UINT(offset),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0
            };

            offset += sizeof(uint32);

            ++attrIndex;

            outInputElementDescs[attrIndex] = {
                .SemanticName = "BLENDWEIGHT",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = UINT(offset),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0
            };

            offset += sizeof(float) * 4;

            ++attrIndex;

            continue;
        }

        size_t attributeSize = VertexUtils::PacketSize(vertexType);
        AssertDebug(attributeSize <= 16);

        const char* semanticName;
        uint32 semanticIndex = 0;
        switch (vertexType)
        {
        case VT_Position:
            semanticName = "POSITION";
            semanticIndex = 0;
            break;
        case VT_Normal:
            semanticName = "NORMAL";
            semanticIndex = 0;
            break;
        case VT_UV0:
            semanticName = "TEXCOORD";
            semanticIndex = 0;
            break;
        case VT_UV1:
            semanticName = "TEXCOORD";
            semanticIndex = 1;
            break;
        default:
            HYP_UNREACHABLE();
        }

        outInputElementDescs[attrIndex] = {
            .SemanticName = semanticName,
            .SemanticIndex = semanticIndex,
            .Format = SizeToFormat[attributeSize / sizeof(float)],
            .InputSlot = 0,
            .AlignedByteOffset = UINT(offset),
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0
        };

        HYP_LOG(RenderingBackend, Debug, "DX12 Input Element [{}]: Semantic={}{}, Format={}, Offset={}",
            attrIndex,
            semanticName,
            semanticIndex,
            (int)SizeToFormat[attributeSize / sizeof(float)],
            offset);

        offset += (uint32)attributeSize;

        ++attrIndex;
    }

    Assert(offset == vertexSize);

    outBindingStrides.Clear();
    outBindingStrides.PushBack(uint32(offset));
}

#pragma region DX12GraphicsPipeline

DX12GraphicsPipeline::DX12GraphicsPipeline()
    : GraphicsPipelineBase()
{
}

DX12GraphicsPipeline::DX12GraphicsPipeline(const DX12ShaderInstanceRef& shaderInstance)
    : GraphicsPipelineBase()
{
    m_shaderInstance = shaderInstance;
}

DX12GraphicsPipeline::~DX12GraphicsPipeline()
{
    m_shaderInstance.Reset();
}

bool DX12GraphicsPipeline::IsCreated() const
{
    return m_pipelineState != nullptr && m_rootSignature != nullptr;
}

RendererResult DX12GraphicsPipeline::Create()
{
    return Rebuild();
}

void DX12GraphicsPipeline::Bind(DX12CommandBuffer* cmd)
{
    Vec2i viewportOffset = Vec2i::Zero();
    Vec2u viewportExtent = m_framebufferDesc.extent;

    Bind(cmd, viewportOffset, viewportExtent);
}

void DX12GraphicsPipeline::Bind(DX12CommandBuffer* commandBuffer, Vec2i viewportOffset, Vec2u viewportExtent)
{
    Assert(m_pipelineState != nullptr);

    commandBuffer->m_boundGraphicsPipeline = this;

    commandBuffer->GetCommandList()->SetPipelineState(m_pipelineState.Get());
    commandBuffer->GetCommandList()->IASetPrimitiveTopology(ToDX12PrimitiveTopology(m_topology));

    commandBuffer->GetCommandList()->SetGraphicsRootSignature(m_rootSignature.Get());

    commandBuffer->ResetBoundDescriptorSets();

    if (viewportExtent != Vec2u::Zero())
    {
        // Clamp viewport to framebuffer extent to prevent D3D12 validation warning
        // about viewport / scissor exceeding render target dimensions (#1378).
        viewportExtent.x = MathUtil::Min(viewportExtent.x, m_framebufferDesc.extent.x);
        viewportExtent.y = MathUtil::Min(viewportExtent.y, m_framebufferDesc.extent.y);

        Viewport viewport;
        viewport.position = viewportOffset;
        viewport.extent = viewportExtent;

        UpdateViewport(commandBuffer, viewport);
    }

    if (m_stencilWrite || m_stencilFunction.HasValue())
    {
        commandBuffer->GetCommandList()->OMSetStencilRef(g_renderInterface->state.stencilReference);
    }
}

void DX12GraphicsPipeline::SetPushConstants(const void* data, size_t size)
{
    HYP_NOT_IMPLEMENTED();
}

HYP_DISABLE_OPTIMIZATION;

void DX12GraphicsPipeline::UpdateViewport(DX12CommandBuffer* commandBuffer, const Viewport& viewport)
{
    D3D12_VIEWPORT d3dViewport {};
    d3dViewport.TopLeftX = float(viewport.position.x);
    d3dViewport.TopLeftY = float(viewport.position.y);
    d3dViewport.Width = float(viewport.extent.x);
    d3dViewport.Height = float(viewport.extent.y);
    d3dViewport.MinDepth = 0.0f;
    d3dViewport.MaxDepth = 1.0f;

    commandBuffer->GetCommandList()->RSSetViewports(1, &d3dViewport);

    D3D12_RECT scissorRect {};
    scissorRect.left = viewport.position.x;
    scissorRect.top = viewport.position.y;
    scissorRect.right = viewport.position.x + int32(viewport.extent.x);
    scissorRect.bottom = viewport.position.y + int32(viewport.extent.y);

    commandBuffer->GetCommandList()->RSSetScissorRects(1, &scissorRect);

    m_viewport = viewport;
}

RendererResult DX12GraphicsPipeline::Rebuild()
{
    Array<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
    Array<uint32> bindingStrides;

    BuildVertexAttributes(inputElementDescs, bindingStrides);

    CheckResultOrReturn(BuildRootSignature());

    Assert(m_rootSignature != nullptr);
    Assert(m_shaderInstance != nullptr);

    m_viewport = Viewport { m_framebufferDesc.extent, Vec2i::Zero() };

    const bool enableBlend = m_blendFunction != BlendFunction::None();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.PrimitiveTopologyType = ToDX12TopologyType(m_topology);

    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.SampleMask = UINT_MAX;

    psoDesc.VS = m_shaderInstance->GetShaderBytecode(ShaderModuleType::Vertex);
    psoDesc.PS = m_shaderInstance->GetShaderBytecode(ShaderModuleType::Pixel);
    psoDesc.GS = m_shaderInstance->GetShaderBytecode(ShaderModuleType::Geometry);

    psoDesc.InputLayout = { inputElementDescs.Data(), (UINT)inputElementDescs.Size() };

    psoDesc.RasterizerState = {};
    psoDesc.RasterizerState.CullMode = ToDX12CullMode(m_faceCullMode);
    psoDesc.RasterizerState.FillMode = (m_fillMode == FM_LINE) ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.RasterizerState.DepthBias = 0.0;//m_depthBias;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = m_depthBiasSlope;
    psoDesc.RasterizerState.DepthClipEnable = m_depthClamp ? FALSE : TRUE;

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
    psoDesc.BlendState.IndependentBlendEnable = enableBlend;

    if (m_framebufferDesc.numAttachments > 0)
    {
        bool hasDSV = false;

        for (uint32 attachmentIndex = 0; attachmentIndex < m_framebufferDesc.numAttachments; attachmentIndex++)
        {
            const AttachmentDesc& attachmentDesc = m_framebufferDesc.attachments[attachmentIndex];

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


            const uint32 rtIndex = psoDesc.NumRenderTargets++;

            D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = psoDesc.BlendState.RenderTarget[rtIndex];
            rtBlend = {};

            rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            if (enableBlend && TextureUtils::FormatSupportsBlending(attachmentDesc.format))
            {
                rtBlend.BlendEnable = TRUE;
                rtBlend.SrcBlend = ToDX12Blend(m_blendFunction.GetSrcColor());
                rtBlend.DestBlend = ToDX12Blend(m_blendFunction.GetDstColor());
                rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
                rtBlend.SrcBlendAlpha = ToDX12Blend(m_blendFunction.GetSrcAlpha());
                rtBlend.DestBlendAlpha = ToDX12Blend(m_blendFunction.GetDstAlpha());
                rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                rtBlend.LogicOpEnable = FALSE;
            }

            psoDesc.RTVFormats[rtIndex] = ToDXGIFormat(attachmentDesc.format, DX12ViewType::RTV_DSV);
        }
    }

    HRESULT res = g_renderInterface->GetDevice()->CreateGraphicsPipelineState(
        &psoDesc,
        __uuidof(ID3D12PipelineState),
        &m_pipelineState
    );

    if (FAILED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create DX12 Pipeline State for {}", res, GetDebugName());

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        WideString ws = *debugName;
        m_pipelineState->SetName(ws.Data());
        m_rootSignature->SetName(ws.Data());
    }
#endif

    return {};
}

RendererResult DX12GraphicsPipeline::BuildRootSignature()
{
    m_rootSignature.Reset();
    m_descriptorSetRootIndices.Clear();

    Assert(m_shaderInstance != nullptr && m_shaderInstance->GetShader() != nullptr);

    const ShaderInputGroup* decl = m_shaderInstance->GetShader()->GetDescriptorTableDeclaration();
    Assert(decl != nullptr);

    Array<D3D12_ROOT_PARAMETER> rootParams;

    // use LL so we never invalid ptrs
    LinkedList<Array<D3D12_DESCRIPTOR_RANGE>> rangeAllocations;

    auto AllocateRangeStorage = [&](Array<D3D12_DESCRIPTOR_RANGE>&& newRanges) -> const D3D12_DESCRIPTOR_RANGE*
    {
        if (newRanges.Empty())
            return nullptr;

        rangeAllocations.PushBack(std::move(newRanges));
        return rangeAllocations.Back().Data();
    };

    // Reserve space for root indices mapping
    // Each descriptor set index maps to an HLSL register space (setIndex -> spaceN)
    // This aligns with how ShaderCompiler.cpp generates #define _{SetName}_SPACE space{N}
    // Find the maximum set index to size the array appropriately
    uint32 maxSetIndex = 0;
    for (const ShaderInputSet& setDecl : decl->elements)
    {
        if (setDecl.setIndex != ~0u && setDecl.setIndex > maxSetIndex)
        {
            maxSetIndex = setDecl.setIndex;
        }
    }

    m_descriptorSetRootIndices.Resize(maxSetIndex + 1);

    // Iterate over each descriptor set and create descriptor ranges
    // In HLSL, each descriptor set corresponds to a register space (space0, space1, etc.)
    for (size_t setIndex = 0; setIndex < decl->elements.Size(); ++setIndex)
    {
        const ShaderInputSet& setDecl = decl->elements[setIndex];
        // pointer to the "real" descriptor set (since we can use `Reference` flag to indicate we should grab one of the global descriptor sets)
        const ShaderInputSet* pSetDecl = &setDecl;

        // Skip empty slots in the elements array
        if (setDecl.setIndex == ~0u)
        {
            continue;
        }

        if (setDecl.flags & ShaderInputSetFlags::Reference)
        {
            AssertDebug(!(setDecl.flags & ShaderInputSetFlags::Template), "Not supported");

            // it's a reference to a global descriptor set -- we need to grab that one.
            const ShaderInputSet* refSetDecl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(setDecl.name);
            AssertDebug(refSetDecl != nullptr, "Invalid reference to global set: {}", setDecl.name);

            pSetDecl = refSetDecl;
        }

        Array<D3D12_DESCRIPTOR_RANGE> viewRanges;
        Array<D3D12_DESCRIPTOR_RANGE> samplerRanges;

        // Collect dynamic buffer entries (CBV_Dynamic / SRV_Dynamic / UAV_Dynamic) to create as root descriptor params
        Array<const ShaderInput*> dynamicDeclarations;

        for (uint8 slotIndex = 0; slotIndex < NumDescriptorSlots; slotIndex++)
        {
            const auto& declarations = pSetDecl->slots[slotIndex];

            if (declarations.Empty())
            {
                continue;
            }

            for (const ShaderInput& descDecl : declarations)
            {
                // Skip descriptors excluded by compile-time conditions (matches DescriptorSetLayout constructor behavior)
                if (descDecl.cond != nullptr && !descDecl.cond())
                {
                    continue;
                }

                if (descDecl.isDynamic && descDecl.category == ShaderResourceCategory::Buffer && descDecl.slot != ShaderRegister::SAMPLER)
                {
                    dynamicDeclarations.PushBack(&descDecl);
                    continue;
                }

                Array<D3D12_DESCRIPTOR_RANGE>* currRanges = (descDecl.slot == ShaderRegister::SAMPLER ? &samplerRanges : &viewRanges);

                D3D12_DESCRIPTOR_RANGE& range = currRanges->EmplaceBack();
                range.RangeType = ToDX12DescriptorRangeType(descDecl.slot);
                range.NumDescriptors = descDecl.count;
                range.BaseShaderRegister = descDecl.index;
                range.RegisterSpace = (UINT)setDecl.setIndex;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            }
        }

        std::sort(viewRanges.Begin(), viewRanges.End(),
            [](const D3D12_DESCRIPTOR_RANGE& a, const D3D12_DESCRIPTOR_RANGE& b) {
                if (a.RangeType != b.RangeType)
                {
                    return a.RangeType < b.RangeType;
                }
                return a.BaseShaderRegister < b.BaseShaderRegister;
            });

        std::sort(samplerRanges.Begin(), samplerRanges.End(),
            [](const D3D12_DESCRIPTOR_RANGE& a, const D3D12_DESCRIPTOR_RANGE& b) {
                return a.BaseShaderRegister < b.BaseShaderRegister;
            });

        // Sort dynamic declarations by flat index to match DescriptorSetLayout::GetDynamicElements() order
        std::sort(dynamicDeclarations.Begin(), dynamicDeclarations.End(),
            [pSetDecl](const ShaderInput* a, const ShaderInput* b) {
                return pSetDecl->CalculateFlatIndex(a->slot, a->name) < pSetDecl->CalculateFlatIndex(b->slot, b->name);
            });

        DescriptorSetRootIndices& rootIndices = m_descriptorSetRootIndices[setDecl.setIndex];
        rootIndices.viewRootIndex = ~0u;
        rootIndices.samplerRootIndex = ~0u;
        rootIndices.dynamicEntryCount = 0;

        // Create root descriptor params for dynamic entries (before descriptor tables)
        for (size_t i = 0; i < dynamicDeclarations.Size() && i < DescriptorSetRootIndices::MaxDynamicEntries; i++)
        {
            const ShaderInput& descDecl = *dynamicDeclarations[i];

            const D3D12_ROOT_PARAMETER_TYPE rootParamType = descDecl.type == ShaderInputType::CBV_Dynamic
                ? D3D12_ROOT_PARAMETER_TYPE_CBV
                : descDecl.type == ShaderInputType::UAV_Dynamic
                    ? D3D12_ROOT_PARAMETER_TYPE_UAV
                    : D3D12_ROOT_PARAMETER_TYPE_SRV;

            D3D12_ROOT_PARAMETER& param = rootParams.EmplaceBack();
            param = {};
            param.ParameterType = rootParamType;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.Descriptor.ShaderRegister = descDecl.index;
            param.Descriptor.RegisterSpace = (UINT)setDecl.setIndex;

            rootIndices.dynamicEntryRootParamIndices[i] = (uint32)(rootParams.Size() - 1);
            rootIndices.dynamicEntryCount++;
        }

        if (viewRanges.Any())
        {
            rootIndices.viewRootIndex = (uint32)rootParams.Size();

            D3D12_ROOT_PARAMETER& param = rootParams.EmplaceBack();
            param = {};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.DescriptorTable.NumDescriptorRanges = (UINT)viewRanges.Size();
            param.DescriptorTable.pDescriptorRanges = AllocateRangeStorage(std::move(viewRanges));
        }

        if (samplerRanges.Any())
        {
            rootIndices.samplerRootIndex = (uint32)rootParams.Size();

            D3D12_ROOT_PARAMETER& param = rootParams.EmplaceBack();
            param = {};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.DescriptorTable.NumDescriptorRanges = (UINT)samplerRanges.Size();
            param.DescriptorTable.pDescriptorRanges = AllocateRangeStorage(std::move(samplerRanges));
        }
    }

    D3D12_ROOT_SIGNATURE_DESC sigDesc {};
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

    res = g_renderInterface->GetDevice()->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        __uuidof(ID3D12RootSignature),
        &m_rootSignature);

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

    if (!name.IsValid())
    {
        return;
    }

    WideString ws = *name;

    if (m_pipelineState)
    {
        m_pipelineState->SetName(ws.Data());
    }

    if (m_rootSignature)
    {
        m_rootSignature->SetName(ws.Data());
    }
}
#endif

#pragma endregion DX12GraphicsPipeline

} // namespace Hyperion
