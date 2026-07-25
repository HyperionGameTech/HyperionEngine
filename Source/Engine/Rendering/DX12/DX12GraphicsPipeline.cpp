/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12GraphicsPipeline.hpp>
#include <Rendering/DX12/DX12ShaderInstance.hpp>
#include <Rendering/DX12/DX12RenderInterface.hpp>
#include <Rendering/DX12/DX12DescriptorSet.hpp>
#include <Rendering/DX12/DX12Framebuffer.hpp>
#include <Rendering/DX12/DX12GpuImage.hpp>
#include <Rendering/DX12/DX12Attachment.hpp>
#include <Rendering/DX12/DX12Helpers.hpp>

#include <Rendering/Vertex.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/Shader.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <algorithm>
#include <cstring>

#include <d3d12shader.h>

#include <DX12GraphicsPipeline.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface RI;

namespace {

using PFN_D3DReflect = HRESULT(WINAPI*)(LPCVOID, SIZE_T, REFIID, void**);

PFN_D3DReflect GetD3DReflect()
{
    static PFN_D3DReflect s_pfnD3DReflect = []() -> PFN_D3DReflect
    {
        HMODULE module = LoadLibraryW(L"d3dcompiler.dll");

        if (!module)
        {
            return nullptr;
        }

        return reinterpret_cast<PFN_D3DReflect>(GetProcAddress(module, "D3DReflect"));
    }();

    return s_pfnD3DReflect;
}

uint32 ReflectPixelShaderOutputCount(
    const D3D12_SHADER_BYTECODE& shaderBytecode,
    DXGI_FORMAT* outFormats = nullptr)
{
    if (!shaderBytecode.pShaderBytecode || shaderBytecode.BytecodeLength == 0)
    {
        return 0;
    }

    const PFN_D3DReflect pfnD3DReflect = GetD3DReflect();

    if (!pfnD3DReflect)
    {
        return 0;
    }

    ComPtr<ID3D12ShaderReflection> reflection;

    if (FAILED(pfnD3DReflect(
            shaderBytecode.pShaderBytecode,
            shaderBytecode.BytecodeLength,
            IID_PPV_ARGS(&reflection))))
    {
        return 0;
    }

    D3D12_SHADER_DESC shaderDesc {};

    if (FAILED(reflection->GetDesc(&shaderDesc)))
    {
        return 0;
    }

    uint32 maxRenderTargetIndex = 0;

    for (UINT outputIndex = 0; outputIndex < shaderDesc.OutputParameters; ++outputIndex)
    {
        D3D12_SIGNATURE_PARAMETER_DESC paramDesc {};

        if (FAILED(reflection->GetOutputParameterDesc(outputIndex, &paramDesc)))
        {
            continue;
        }

        if (std::strcmp(paramDesc.SemanticName, "SV_Target") != 0)
        {
            continue;
        }

        const uint32 slotIndex = paramDesc.SemanticIndex;

        if (slotIndex >= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
        {
            continue;
        }

        if (slotIndex + 1 > maxRenderTargetIndex)
        {
            maxRenderTargetIndex = slotIndex + 1;
        }

        if (outFormats != nullptr)
        {
            DXGI_FORMAT compatibleFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

            switch (paramDesc.ComponentType)
            {
            case D3D_REGISTER_COMPONENT_UINT32:
                compatibleFormat = DXGI_FORMAT_R8G8B8A8_UINT;
                break;
            case D3D_REGISTER_COMPONENT_SINT32:
                compatibleFormat = DXGI_FORMAT_R8G8B8A8_SINT;
                break;
            case D3D_REGISTER_COMPONENT_FLOAT32:
            case D3D_REGISTER_COMPONENT_UNKNOWN:
            default:
                compatibleFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            }

            outFormats[slotIndex] = compatibleFormat;
        }
    }

    return maxRenderTargetIndex;
}

} // anonymous namespace

void DX12GraphicsPipeline::BuildVertexAttributes(
    Array<D3D12_INPUT_ELEMENT_DESC, DX12TempAllocator>& outInputElementDescs,
    Array<uint32, DX12TempAllocator>& outBindingStrides)
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

        offset += (uint32)attributeSize;

        ++attrIndex;
    }

    Assert(offset == vertexSize);

    outBindingStrides.Resize(0);
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
        commandBuffer->GetCommandList()->OMSetStencilRef(RI.state.stencilReference);
    }
}

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
    Array<D3D12_INPUT_ELEMENT_DESC, DX12TempAllocator> inputElementDescs;
    Array<uint32, DX12TempAllocator> bindingStrides;

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
    psoDesc.RasterizerState.DepthBias = m_depthBias;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = m_depthBiasSlope;
    psoDesc.RasterizerState.DepthClipEnable = m_depthClamp ? FALSE : TRUE;

    psoDesc.DepthStencilState = {};
    psoDesc.DepthStencilState.DepthFunc = ToDX12DepthCompareOp(m_depthCompareOp);
    psoDesc.DepthStencilState.DepthEnable = m_depthTest;
    psoDesc.DepthStencilState.DepthWriteMask = m_depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;

    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

    if (m_stencilFunction.HasValue())
    {
        psoDesc.DepthStencilState.StencilEnable = TRUE;
        psoDesc.DepthStencilState.StencilReadMask = m_stencilCompareMask;
        psoDesc.DepthStencilState.StencilWriteMask = m_stencilWriteMask;

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

    bool hasDSV = false;

    if (m_framebufferDesc.numAttachments > 0)
    {
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
            {
#ifdef HYP_RHI_DEBUG_NAMES
                return HYP_MAKE_ERROR(RendererError, "To many render targets for pipeline {}!", 0, GetDebugName());
#else
                return HYP_MAKE_ERROR(RendererError, "To many render targets for pipeline!", 0);
#endif
            }

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
                rtBlend.SrcBlendAlpha = ToDX12Blend(m_blendFunction.GetSrcAlpha(), /* isAlpha */ true);
                rtBlend.DestBlendAlpha = ToDX12Blend(m_blendFunction.GetDstAlpha(), /* isAlpha */ true);
                rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                rtBlend.LogicOpEnable = FALSE;
            }

            psoDesc.RTVFormats[rtIndex] = ToDXGIFormat(attachmentDesc.format, DX12ViewType::RTV_DSV);
        }
    }

    // If the framebuffer does not provide a depth-stencil attachment, force depth/stencil
    // operations off. Otherwise the debug layer emits: CREATEGRAPHICSPIPELINESTATE_DEPTHSTENCILVIEW_NOT_SET (#680)
    if (!hasDSV)
    {
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
    }

    // If the pixel shader declares more SV_Target outputs than the framebuffer provides color
    // attachments, pad NumRenderTargets / RTVFormats to cover them. (Fixes: CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET)
    {
        DXGI_FORMAT shaderOutputFormats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};

        const uint32 shaderOutputCount = ReflectPixelShaderOutputCount(psoDesc.PS, shaderOutputFormats);

        while (psoDesc.NumRenderTargets < shaderOutputCount)
        {
            const uint32 padIndex = psoDesc.NumRenderTargets++;

            psoDesc.RTVFormats[padIndex] = shaderOutputFormats[padIndex] != DXGI_FORMAT_UNKNOWN
                ? shaderOutputFormats[padIndex]
                : DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    HRESULT res = RI.GetDevice()->CreateGraphicsPipelineState(
        &psoDesc,
        __uuidof(ID3D12PipelineState),
        &m_pipelineState);
#ifdef HYP_RHI_DEBUG_NAMES
    if (FAILED(res))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create DX12 Pipeline State for {}", res, GetDebugName());
    }

    if (Name debugName = GetDebugName())
    {
        WideString ws = *debugName;
        m_pipelineState->SetName(ws.Data());
        m_rootSignature->SetName(ws.Data());
    }
#else
    if (FAILED(res))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create DX12 Pipeline State", res);
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

    const_cast<ShaderInputGroup*>(decl)->RecalculateAllIndices();

    Array<D3D12_ROOT_PARAMETER, DX12TempAllocator> rootParams;
    rootParams.Reserve(16);

    // use LL so we never invalid ptrs
    List<Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator>, DX12TempAllocator> rangeAllocations;

    auto allocateRangeStorage = [&](Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator>&& newRanges) -> const D3D12_DESCRIPTOR_RANGE*
    {
        if (newRanges.Empty())
        {
            return nullptr;
        }

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
            const ShaderInputSet* refSetDecl = RI.globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(setDecl.name);
            AssertDebug(refSetDecl != nullptr, "Invalid reference to global set: {}", setDecl.name);

            pSetDecl = refSetDecl;
        }

        Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator> viewRanges;
        Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator> samplerRanges;

        // Collect dynamic buffer entries (CBV_Dynamic / SRV_Dynamic / UAV_Dynamic) to create as root descriptor params
        Array<const ShaderInput*, DX12TempAllocator> dynamicDeclarations;

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

                Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator>* currRanges = (descDecl.slot == ShaderRegister::SAMPLER ? &samplerRanges : &viewRanges);

                D3D12_DESCRIPTOR_RANGE& range = currRanges->EmplaceBack();
                range.RangeType = ToDX12DescriptorRangeType(descDecl.slot);
                range.NumDescriptors = descDecl.count;
                range.BaseShaderRegister = descDecl.index;
                range.RegisterSpace = (UINT)setDecl.setIndex;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            }
        }

        std::sort(viewRanges.Begin(), viewRanges.End(),
                  [](const D3D12_DESCRIPTOR_RANGE& a, const D3D12_DESCRIPTOR_RANGE& b)
                  {
                      if (a.RangeType != b.RangeType)
                      {
                          return a.RangeType < b.RangeType;
                      }
                      return a.BaseShaderRegister < b.BaseShaderRegister;
                  });

        std::sort(samplerRanges.Begin(), samplerRanges.End(),
                  [](const D3D12_DESCRIPTOR_RANGE& a, const D3D12_DESCRIPTOR_RANGE& b)
                  {
                      return a.BaseShaderRegister < b.BaseShaderRegister;
                  });

        // Sort dynamic declarations by flat index to match DescriptorSetLayout::GetDynamicElements() order
        std::sort(dynamicDeclarations.Begin(), dynamicDeclarations.End(),
                  [pSetDecl](const ShaderInput* a, const ShaderInput* b)
                  {
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
            param.DescriptorTable.pDescriptorRanges = allocateRangeStorage(std::move(viewRanges));
        }

        if (samplerRanges.Any())
        {
            rootIndices.samplerRootIndex = (uint32)rootParams.Size();

            D3D12_ROOT_PARAMETER& param = rootParams.EmplaceBack();
            param = {};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.DescriptorTable.NumDescriptorRanges = (UINT)samplerRanges.Size();
            param.DescriptorTable.pDescriptorRanges = allocateRangeStorage(std::move(samplerRanges));
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

    res = RI.GetDevice()->CreateRootSignature(
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

#ifdef HYP_RHI_DEBUG_NAMES
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
