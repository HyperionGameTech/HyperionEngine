/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12GpuBuffer.hpp>
#include <Rendering/DX12/DX12GpuImage.hpp>
#include <Rendering/DX12/DX12ComputePipeline.hpp>
#include <Rendering/DX12/DX12CommandBuffer.hpp>
#include <Rendering/DX12/DX12RenderInterface.hpp>
#include <Rendering/DX12/DX12ShaderInstance.hpp>
#include <Rendering/DX12/DX12DescriptorSet.hpp>
#include <Rendering/DX12/DX12Helpers.hpp>

#include <Rendering/Shader.hpp>
#include <Rendering/Shared.hpp>

#include <Core/Math/MathUtil.hpp>

#include <algorithm>

#include <DX12ComputePipeline.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12ComputePipeline

DX12ComputePipeline::DX12ComputePipeline()
    : ComputePipelineBase()
{
}

DX12ComputePipeline::DX12ComputePipeline(const DX12ShaderInstanceRef& shaderInstance)
    : ComputePipelineBase(shaderInstance)
{
}

DX12ComputePipeline::~DX12ComputePipeline()
{
    m_shaderInstance.Reset();
}

bool DX12ComputePipeline::IsCreated() const
{
    return m_pipelineState != nullptr && m_rootSignature != nullptr;
}

RendererResult DX12ComputePipeline::Create()
{
    if (!m_shaderInstance)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute shader not provided to pipeline");
    }

    DX12ShaderInstance* shaderInstance = static_cast<DX12ShaderInstance*>(m_shaderInstance.Get());
    Assert(shaderInstance != nullptr && shaderInstance->GetShader() != nullptr);

    if (!shaderInstance->IsCreated())
    {
        CheckResultOrReturn(shaderInstance->Create());
    }

    CheckResultOrReturn(BuildRootSignature());

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.CS = shaderInstance->GetShaderBytecode(ShaderModuleType::Compute);

    if (psoDesc.CS.BytecodeLength == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute shader bytecode not available");
    }

    HRESULT res = RI.GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));

    if (FAILED(res))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create compute pipeline state", res);
    }

#if HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        WideString ws = *debugName;
        m_pipelineState->SetName(ws.Data());
    }
#endif

    return {};
}

RendererResult DX12ComputePipeline::BuildRootSignature()
{
    m_rootSignature.Reset();
    m_descriptorSetRootIndices.Clear();

    Assert(m_shaderInstance != nullptr && m_shaderInstance->GetShader() != nullptr);

    const ShaderInputGroup* decl = m_shaderInstance->GetShader()->GetDescriptorTableDeclaration();
    Assert(decl != nullptr);

    const_cast<ShaderInputGroup*>(decl)->RecalculateAllIndices();

    Array<D3D12_ROOT_PARAMETER> rootParams;

    // use LL so we never invalid ptrs
    TList<Array<D3D12_DESCRIPTOR_RANGE>> rangeAllocations;

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
            const ShaderInputSet* refSetDecl = RI.globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(setDecl.name);
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
            const ShaderInput* descDecl = dynamicDeclarations[i];

            const D3D12_ROOT_PARAMETER_TYPE rootParamType = descDecl->type == ShaderInputType::CBV_Dynamic
                ? D3D12_ROOT_PARAMETER_TYPE_CBV
                : descDecl->type == ShaderInputType::UAV_Dynamic
                    ? D3D12_ROOT_PARAMETER_TYPE_UAV
                    : D3D12_ROOT_PARAMETER_TYPE_SRV;

            D3D12_ROOT_PARAMETER& param = rootParams.EmplaceBack();
            param = {};
            param.ParameterType = rootParamType;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.Descriptor.ShaderRegister = descDecl->index;
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
    sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

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

void DX12ComputePipeline::Bind(CommandBuffer* commandBuffer)
{
    Assert(m_pipelineState != nullptr);
    Assert(m_rootSignature != nullptr);

    DX12CommandBuffer* dx12CommandBuffer = static_cast<DX12CommandBuffer*>(commandBuffer);
    Assert(dx12CommandBuffer != nullptr);

    ID3D12GraphicsCommandList* cmdList = dx12CommandBuffer->GetCommandList();
    Assert(cmdList != nullptr);

    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->SetComputeRootSignature(m_rootSignature.Get());

    dx12CommandBuffer->ResetBoundDescriptorSets();
}

void DX12ComputePipeline::Dispatch(CommandBuffer* commandBuffer, const Vec3u& groupSize) const
{
    Assert(m_pipelineState != nullptr);

    DX12CommandBuffer* dx12CommandBuffer = static_cast<DX12CommandBuffer*>(commandBuffer);
    Assert(dx12CommandBuffer != nullptr);

    ID3D12GraphicsCommandList* cmdList = dx12CommandBuffer->GetCommandList();
    Assert(cmdList != nullptr);

    cmdList->Dispatch(groupSize.x, groupSize.y, groupSize.z);
}

void DX12ComputePipeline::DispatchIndirect(
    CommandBuffer* commandBuffer,
    const DX12GpuBufferRef& indirectBuffer,
    size_t offset) const
{
    Assert(m_pipelineState != nullptr);
    Assert(indirectBuffer != nullptr);

    DX12CommandBuffer* dx12CommandBuffer = static_cast<DX12CommandBuffer*>(commandBuffer);
    Assert(dx12CommandBuffer != nullptr);

    ID3D12GraphicsCommandList* cmdList = dx12CommandBuffer->GetCommandList();
    Assert(cmdList != nullptr);

    // Create command signature lazily per-instance so each pipeline uses its own root signature.
    if (m_dispatchCommandSignature == nullptr)
    {
        D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
        argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        D3D12_COMMAND_SIGNATURE_DESC sigDesc = {};
        sigDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
        sigDesc.NumArgumentDescs = 1;
        sigDesc.pArgumentDescs = &argDesc;

        RI.GetDevice()->CreateCommandSignature(
            &sigDesc,
            m_rootSignature.Get(),
            IID_PPV_ARGS(&m_dispatchCommandSignature));
    }

    cmdList->ExecuteIndirect(
        m_dispatchCommandSignature.Get(),
        1,
        indirectBuffer->GetResource(),
        offset,
        nullptr,
        0);
}

#ifdef HYP_DEBUG_MODE
void DX12ComputePipeline::SetDebugName(Name name)
{
    ComputePipelineBase::SetDebugName(name);

    if (!IsCreated())
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

#pragma endregion DX12ComputePipeline

} // namespace Hyperion
