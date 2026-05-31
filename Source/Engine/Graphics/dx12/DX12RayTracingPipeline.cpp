/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12RayTracingPipeline.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12ShaderInstance.hpp>
#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/Shader.hpp>
#include <rendering/Shared.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/memory/allocator/ArenaAllocator.hpp>

#include <DX12RayTracingPipeline.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12RayTracingPipeline

DX12RayTracingPipeline::DX12RayTracingPipeline()
    : RayTracingPipelineBase()
{
}

DX12RayTracingPipeline::DX12RayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance)
    : RayTracingPipelineBase(shaderInstance)
{
}

DX12RayTracingPipeline::~DX12RayTracingPipeline()
{
    m_shaderInstance.Reset();
}

bool DX12RayTracingPipeline::IsCreated() const
{
    return m_stateObject != nullptr && m_rootSignature != nullptr;
}

RendererResult DX12RayTracingPipeline::Create()
{
    if (!m_shaderInstance)
    {
        return HYP_MAKE_ERROR(RendererError, "Ray tracing shader not provided to pipeline");
    }

    DX12ShaderInstance* shaderInstance = static_cast<DX12ShaderInstance*>(m_shaderInstance.Get());
    Assert(shaderInstance != nullptr && shaderInstance->GetShader() != nullptr);

    if (!shaderInstance->IsCreated())
    {
        CheckResultOrReturn(shaderInstance->Create());
    }

    CheckResultOrReturn(BuildRootSignature());

    ID3D12Device* device = RI.GetDevice();

    ComPtr<ID3D12Device5> device5;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device5))))
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support DXR");
    }

    // Collect all ray tracing shader modules
    struct RayTracingModule
    {
        ShaderModuleType type;
        D3D12_SHADER_BYTECODE bytecode;
    };

    Array<RayTracingModule, FixedAllocator<5>> rtModules;

    for (ShaderModuleType type : { ShaderModuleType::RayGen, ShaderModuleType::Miss, ShaderModuleType::ClosestHit, ShaderModuleType::AnyHit, ShaderModuleType::Intersect })
    {
        auto* blob = shaderInstance->GetShaderBlob(type);
        if (blob && blob->bytecode.BytecodeLength > 0)
        {
            rtModules.PushBack({ type, blob->bytecode });
        }
    }

    if (rtModules.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "No ray tracing shader modules found");
    }

    // Build export names as wide strings for the DXIL library
    Array<D3D12_EXPORT_DESC, DX12TempAllocator> exports;
    exports.Reserve(rtModules.Size());

    Array<D3D12_DXIL_LIBRARY_DESC, DX12TempAllocator> dxilLibraries;
    dxilLibraries.Reserve(rtModules.Size());

    for (const RayTracingModule& mod : rtModules)
    {
        const char* entryName = DefaultEntryPointNames[uint8(mod.type)];
        Assert(entryName[0] != '\0');

        WideString wideEntryName = WideString(entryName);

        // Allocate name storage that outlives the subobjects array
        wchar_t* nameStorage = (wchar_t*)g_dx12Arena->Allocate((wideEntryName.Size() + 1) * sizeof(wchar_t), alignof(wchar_t));
        std::memcpy(nameStorage, wideEntryName.Data(), (wideEntryName.Size() + 1) * sizeof(wchar_t));

        D3D12_EXPORT_DESC& exp = exports.EmplaceBack();
        exp.Name = nameStorage;
        exp.ExportToRename = nullptr;
        exp.Flags = D3D12_EXPORT_FLAG_NONE;

        D3D12_DXIL_LIBRARY_DESC& lib = dxilLibraries.EmplaceBack();
        lib.DXILLibrary = mod.bytecode;
        lib.NumExports = 1;
        lib.pExports = &exports.Back();
    }

    // Determine which exports exist for hit group binding
    bool hasClosestHit = false;
    bool hasAnyHit = false;
    bool hasIntersect = false;

    for (const RayTracingModule& mod : rtModules)
    {
        if (mod.type == ShaderModuleType::ClosestHit)
            hasClosestHit = true;
        else if (mod.type == ShaderModuleType::AnyHit)
            hasAnyHit = true;
        else if (mod.type == ShaderModuleType::Intersect)
            hasIntersect = true;
    }

    // Build hit group definition (single hit group with all available modules)
    D3D12_HIT_GROUP_DESC hitGroupDesc {};
    bool hasHitGroup = false;

    wchar_t hitGroupNameStorage[32] = {};
    wchar_t closestHitStorage[128] = {};
    wchar_t anyHitStorage[128] = {};
    wchar_t intersectStorage[128] = {};

    if (hasClosestHit)
    {
        hasHitGroup = true;

        constexpr WideStringView hgName = L"HitGroup";
        std::memcpy(hitGroupNameStorage, hgName.Data(), MathUtil::Min(hgName.Size(), 31u) * sizeof(wchar_t));

        hitGroupDesc.HitGroupExport = hitGroupNameStorage;
        hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

        if (hasClosestHit)
        {
            WideString ws = WideString(DefaultEntryPointNames[uint8(ShaderModuleType::ClosestHit)]);
            std::memcpy(closestHitStorage, ws.Data(), MathUtil::Min(ws.Size(), 127u) * sizeof(wchar_t));
            hitGroupDesc.ClosestHitShaderImport = closestHitStorage;
        }

        if (hasAnyHit)
        {
            WideString ws = WideString(DefaultEntryPointNames[uint8(ShaderModuleType::AnyHit)]);
            std::memcpy(anyHitStorage, ws.Data(), MathUtil::Min(ws.Size(), 127u) * sizeof(wchar_t));
            hitGroupDesc.AnyHitShaderImport = anyHitStorage;
        }

        if (hasIntersect)
        {
            WideString ws = WideString(DefaultEntryPointNames[uint8(ShaderModuleType::Intersect)]);
            std::memcpy(intersectStorage, ws.Data(), MathUtil::Min(ws.Size(), 127u) * sizeof(wchar_t));
            hitGroupDesc.IntersectionShaderImport = intersectStorage;
        }
    }

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig {};
    shaderConfig.MaxPayloadSizeInBytes = 128; // Would be nice to reduce to 64; but we have some payloads that are a bit larger currently.
    shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(float);

    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig {};
    pipelineConfig.MaxTraceRecursionDepth = 1; // @TODO Revisit

    D3D12_DXIL_LIBRARY_DESC* pLibDescs = (D3D12_DXIL_LIBRARY_DESC*)g_dx12Arena->Allocate(sizeof(D3D12_DXIL_LIBRARY_DESC) * dxilLibraries.Size(), alignof(D3D12_DXIL_LIBRARY_DESC));
    Memory::Copy(pLibDescs, dxilLibraries.Data(), dxilLibraries.Size() * sizeof(D3D12_DXIL_LIBRARY_DESC));

    D3D12_HIT_GROUP_DESC* pHitGroupDesc = HYP_POOL_NEW(g_dx12Arena, D3D12_HIT_GROUP_DESC);
    *pHitGroupDesc = hitGroupDesc;

    D3D12_RAYTRACING_SHADER_CONFIG* pShaderConfig = HYP_POOL_NEW(g_dx12Arena, D3D12_RAYTRACING_SHADER_CONFIG);
    *pShaderConfig = shaderConfig;

    D3D12_RAYTRACING_PIPELINE_CONFIG* pPipelineConfig = HYP_POOL_NEW(g_dx12Arena, D3D12_RAYTRACING_PIPELINE_CONFIG);
    *pPipelineConfig = pipelineConfig;

    // Build subobjects array
    Array<D3D12_STATE_SUBOBJECT, DX12TempAllocator> subobjects;
    subobjects.Reserve(dxilLibraries.Size() + int(hasHitGroup) + 3);

    for (uint32 i = 0; i < uint32(dxilLibraries.Size()); i++)
    {
        D3D12_STATE_SUBOBJECT& sub = subobjects.EmplaceBack();
        sub.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        sub.pDesc = &pLibDescs[i];
    }

    if (hasHitGroup)
    {
        D3D12_STATE_SUBOBJECT& sub = subobjects.EmplaceBack();
        sub.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        sub.pDesc = pHitGroupDesc;
    }

    {
        D3D12_STATE_SUBOBJECT& sub = subobjects.EmplaceBack();
        sub.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        sub.pDesc = pShaderConfig;
    }

    {
        D3D12_STATE_SUBOBJECT& sub = subobjects.EmplaceBack();
        sub.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        sub.pDesc = pPipelineConfig;
    }

    {
        ID3D12RootSignature** ppRootSignature = HYP_POOL_NEW(g_dx12Arena, ID3D12RootSignature*);
        *ppRootSignature = m_rootSignature.Get();

        D3D12_STATE_SUBOBJECT& sub = subobjects.EmplaceBack();
        sub.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
        sub.pDesc = ppRootSignature;
    }

    // Create state object
    D3D12_STATE_OBJECT_DESC stateObjectDesc {};
    stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjectDesc.NumSubobjects = uint32(subobjects.Size());
    stateObjectDesc.pSubobjects = subobjects.Data();

    HRESULT res = device5->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&m_stateObject));

    if (FAILED(res))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create DXR state object", res);
    }

    // Get state object properties for shader identifiers
    res = m_stateObject->QueryInterface(IID_PPV_ARGS(&m_stateObjectProperties));

    if (FAILED(res))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to query ID3D12StateObjectProperties", res);
    }

#if HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        WideString ws = *debugName;
        m_stateObject->SetName(ws.Data());
        m_rootSignature->SetName(ws.Data());
    }
#endif

    CheckResultOrReturn(BuildShaderBindingTables());

    return {};
}

RendererResult DX12RayTracingPipeline::BuildRootSignature()
{
    m_rootSignature.Reset();
    m_descriptorSetRootIndices.Clear();

    Assert(m_shaderInstance != nullptr && m_shaderInstance->GetShader() != nullptr);

    const ShaderInputGroup* decl = m_shaderInstance->GetShader()->GetDescriptorTableDeclaration();
    Assert(decl != nullptr);

    Array<D3D12_ROOT_PARAMETER, DX12TempAllocator> rootParams;

    LinkedList<Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator>, DX12TempAllocator> rangeAllocations;

    auto AllocateRangeStorage = [&](Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator>&& newRanges) -> const D3D12_DESCRIPTOR_RANGE*
    {
        if (newRanges.Empty())
            return nullptr;

        rangeAllocations.PushBack(std::move(newRanges));
        return rangeAllocations.Back().Data();
    };

    uint32 maxSetIndex = 0;
    for (const ShaderInputSet& setDecl : decl->elements)
    {
        if (setDecl.setIndex != ~0u && setDecl.setIndex > maxSetIndex)
        {
            maxSetIndex = setDecl.setIndex;
        }
    }

    m_descriptorSetRootIndices.Resize(maxSetIndex + 1);

    for (size_t setIndex = 0; setIndex < decl->elements.Size(); ++setIndex)
    {
        const ShaderInputSet& setDecl = decl->elements[setIndex];
        const ShaderInputSet* pSetDecl = &setDecl;

        if (setDecl.setIndex == ~0u)
        {
            continue;
        }

        if (setDecl.flags & ShaderInputSetFlags::Reference)
        {
            AssertDebug(!(setDecl.flags & ShaderInputSetFlags::Template), "Not supported");

            const ShaderInputSet* refSetDecl = RI.globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(setDecl.name);
            AssertDebug(refSetDecl != nullptr, "Invalid reference to global set: {}", setDecl.name);

            pSetDecl = refSetDecl;
        }

        Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator> viewRanges;
        Array<D3D12_DESCRIPTOR_RANGE, DX12TempAllocator> samplerRanges;

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

        std::sort(dynamicDeclarations.Begin(), dynamicDeclarations.End(),
            [pSetDecl](const ShaderInput* a, const ShaderInput* b)
            {
                return pSetDecl->CalculateFlatIndex(a->slot, a->name) < pSetDecl->CalculateFlatIndex(b->slot, b->name);
            });

        DescriptorSetRootIndices& rootIndices = m_descriptorSetRootIndices[setDecl.setIndex];
        rootIndices.viewRootIndex = ~0u;
        rootIndices.samplerRootIndex = ~0u;
        rootIndices.dynamicEntryCount = 0;

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

        return HYP_MAKE_ERROR(RendererError, "Root Signature Serialization Failed", res, errStr);
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

RendererResult DX12RayTracingPipeline::BuildShaderBindingTables()
{
    Assert(m_stateObjectProperties != nullptr);

    const uint32 shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const uint32 sbtEntrySize = MathUtil::NextMultiple(shaderIdentifierSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

    auto BuildTableEntry = [&](const wchar_t* exportName, ShaderBindingTableEntry& outEntry, const char* debugName) -> RendererResult
    {
        void* shaderId = m_stateObjectProperties->GetShaderIdentifier(exportName);
        if (!shaderId)
        {
            // Export may not exist (e.g., no miss shader) — leave entry empty
            return {};
        }

        outEntry.buffer = RI.MakeGpuBuffer(GpuBufferType::ShaderBindingTable, sbtEntrySize);
        outEntry.buffer->SetIsCpuAccessible(true);

#if HYP_DEBUG_MODE
        outEntry.buffer->SetDebugName(CreateNameFromDynamicString(debugName));
#endif

        CheckResultOrReturn(outEntry.buffer->Create());

        outEntry.buffer->Copy(shaderIdentifierSize, shaderId);
        outEntry.address = outEntry.buffer->GetResource()->GetGPUVirtualAddress();
        outEntry.size = sbtEntrySize;

        return {};
    };

    CheckResultOrReturn(BuildTableEntry(L"RayGenMain", m_rayGenShaderTable, "RayGenShaderTable"));
    CheckResultOrReturn(BuildTableEntry(L"MissMain", m_missShaderTable, "MissShaderTable"));
    CheckResultOrReturn(BuildTableEntry(L"HitGroup", m_hitGroupShaderTable, "HitGroupShaderTable"));

    return {};
}

void DX12RayTracingPipeline::Bind(CommandBuffer* commandBuffer)
{
    Assert(m_stateObject != nullptr);
    Assert(m_rootSignature != nullptr);

    DX12CommandBuffer* dx12CommandBuffer = static_cast<DX12CommandBuffer*>(commandBuffer);
    Assert(dx12CommandBuffer != nullptr);

    ID3D12GraphicsCommandList* cmdList = dx12CommandBuffer->GetCommandList();
    Assert(cmdList != nullptr);

    ComPtr<ID3D12GraphicsCommandList4> commandList4;
    if (FAILED(cmdList->QueryInterface(IID_PPV_ARGS(&commandList4))))
    {
        return;
    }

    commandList4->SetPipelineState1(m_stateObject.Get());
    cmdList->SetComputeRootSignature(m_rootSignature.Get());

    dx12CommandBuffer->ResetBoundDescriptorSets();
}

void DX12RayTracingPipeline::TraceRays(CommandBuffer* commandBuffer, const Vec3u& extent) const
{
    Assert(m_stateObject != nullptr);

    DX12CommandBuffer* dx12CommandBuffer = static_cast<DX12CommandBuffer*>(commandBuffer);
    Assert(dx12CommandBuffer != nullptr);

    ID3D12GraphicsCommandList* cmdList = dx12CommandBuffer->GetCommandList();
    Assert(cmdList != nullptr);

    ComPtr<ID3D12GraphicsCommandList4> commandList4;
    if (FAILED(cmdList->QueryInterface(IID_PPV_ARGS(&commandList4))))
    {
        return;
    }

    D3D12_DISPATCH_RAYS_DESC dispatchDesc {};
    dispatchDesc.RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable.address;
    dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable.size;

    dispatchDesc.MissShaderTable.StartAddress = m_missShaderTable.address;
    dispatchDesc.MissShaderTable.SizeInBytes = m_missShaderTable.size;
    dispatchDesc.MissShaderTable.StrideInBytes = m_missShaderTable.size;

    dispatchDesc.HitGroupTable.StartAddress = m_hitGroupShaderTable.address;
    dispatchDesc.HitGroupTable.SizeInBytes = m_hitGroupShaderTable.size;
    dispatchDesc.HitGroupTable.StrideInBytes = m_hitGroupShaderTable.size;

    dispatchDesc.Width = extent.x;
    dispatchDesc.Height = extent.y;
    dispatchDesc.Depth = extent.z;

    commandList4->DispatchRays(&dispatchDesc);
}

#ifdef HYP_DEBUG_MODE
void DX12RayTracingPipeline::SetDebugName(Name name)
{
    RayTracingPipelineBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    WideString ws = *name;

    if (m_stateObject)
    {
        m_stateObject->SetName(ws.Data());
    }

    if (m_rootSignature)
    {
        m_rootSignature->SetName(ws.Data());
    }
}
#endif

#pragma endregion DX12RayTracingPipeline

} // namespace Hyperion
