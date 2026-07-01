/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanGpuBuffer.hpp>
#include <Rendering/Vulkan/VulkanAccelerationStructure.hpp>
#include <Rendering/Vulkan/VulkanFence.hpp>
#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/Vulkan/VulkanInstance.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanFeatures.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanDescriptorSet.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/Bindless.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Utilities/Range.hpp>

#include <Core/Math/MathUtil.hpp>

#include <VulkanAccelerationStructure.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

static HYP_FORCE_INLINE VkTransformMatrixKHR ToVkTransform(const Mat4f& matrix)
{
    VkTransformMatrixKHR transform;
    std::memcpy(&transform, &matrix, 3 * 4 * sizeof(float));

    return transform;
}

static VkAccelerationStructureTypeKHR ToVkAccelerationStructureType(AccelerationStructureType type)
{
    switch (type)
    {
    case AccelerationStructureType::BOTTOM_LEVEL:
        return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    case AccelerationStructureType::TOP_LEVEL:
        return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    default:
        return VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
    }
}

#pragma region VulkanAccelerationGeometry

VulkanAccelerationGeometry::VulkanAccelerationGeometry(
    const VulkanGpuBufferRef& packedVerticesBuffer,
    const VulkanGpuBufferRef& packedIndicesBuffer,
    uint32 numVertices, uint32 numIndices,
    const Handle<Material>& material)
    : m_material(material),
      m_packedVerticesBuffer(packedVerticesBuffer),
      m_packedIndicesBuffer(packedIndicesBuffer),
      m_numVertices(numVertices),
      m_numIndices(numIndices),
      m_geometry {},
      m_isCreated(false)
{
}

VulkanAccelerationGeometry::~VulkanAccelerationGeometry()
{
    EnqueueDeletion(std::move(m_packedVerticesBuffer));
    EnqueueDeletion(std::move(m_packedIndicesBuffer));

    m_isCreated = false;
}

bool VulkanAccelerationGeometry::IsCreated() const
{
    return m_isCreated;
}

RendererResult VulkanAccelerationGeometry::Create()
{
    if (m_isCreated)
    {
        return {};
    }

    if (!RI.GetDevice()->GetFeatures().IsRayTracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support rayTracing");
    }

    if (!m_packedVerticesBuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not valid");
    }

    if (!m_packedVerticesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not created");
    }

    if (!m_packedIndicesBuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not valid");
    }

    if (!m_packedIndicesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not created");
    }

    VkDeviceOrHostAddressConstKHR verticesAddress {
        .deviceAddress = m_packedVerticesBuffer->GetBufferDeviceAddress()
    };

    VkDeviceOrHostAddressConstKHR indicesAddress {
        .deviceAddress = m_packedIndicesBuffer->GetBufferDeviceAddress()
    };

    m_geometry = VkAccelerationStructureGeometryKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    m_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    m_geometry.geometry = {
        .triangles = VkAccelerationStructureGeometryTrianglesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = verticesAddress,
            .vertexStride = sizeof(PackedVertex),
            .maxVertex = uint32(m_packedVerticesBuffer->Size() / sizeof(PackedVertex)) - 1,
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = indicesAddress,
            .transformData = { {} } }
    };

    m_isCreated = true;

    return RendererResult();
}

#pragma endregion VulkanAccelerationGeometry

#pragma region AccelerationStructure

VulkanAccelerationStructureBase::VulkanAccelerationStructureBase(const Mat4f& transform)
    : m_transform(transform),
      m_accelerationStructure(VK_NULL_HANDLE),
      m_deviceAddress(0),
      m_flags(ACCELERATION_STRUCTURE_FLAGS_NONE)
{
}

VulkanAccelerationStructureBase::~VulkanAccelerationStructureBase()
{
    m_geometries.Clear();

    if (m_accelerationStructure != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>(
            [accelerationStructure = m_accelerationStructure]()
            {
                RI.dynamicFunctions.vkDestroyAccelerationStructureKHR(
                    RI.GetDevice()->GetDevice(),
                    accelerationStructure,
                    VK_NULL_HANDLE);
            }));

        m_accelerationStructure = VK_NULL_HANDLE;

        EnqueueDeletion(std::move(m_buffer));
        EnqueueDeletion(std::move(m_scratchBuffer));
    }
}

RendererResult VulkanAccelerationStructureBase::CreateAccelerationStructure(
    AccelerationStructureType type,
    Span<const VkAccelerationStructureGeometryKHR> geometries,
    Span<const uint32> primitiveCounts,
    const bool update,
    RTUpdateStateFlags& outUpdateStateFlags)
{
    if (update)
    {
        Assert(m_accelerationStructure != VK_NULL_HANDLE);
    }
    else
    {
        Assert(m_accelerationStructure == VK_NULL_HANDLE);
    }

    if (!RI.GetDevice()->GetFeatures().IsRayTracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support rayTracing");
    }

    if (!geometries)
    {
        return HYP_MAKE_ERROR(RendererError, "Geometries empty");
    }

    VkAccelerationStructureBuildRangeInfoKHR* pRangeInfos = nullptr;

    VkAccelerationStructureGeometryKHR* pGeometries = (VkAccelerationStructureGeometryKHR*)g_vulkanPool->Allocate(sizeof(VkAccelerationStructureGeometryKHR) * geometries.Size(), alignof(VkAccelerationStructureGeometryKHR));
    Memory::Copy(pGeometries, geometries.Data(), geometries.Size() * sizeof(VkAccelerationStructureGeometryKHR));

    uint32* pPrimitiveCounts = (uint32*)g_vulkanPool->Allocate(sizeof(uint32) * primitiveCounts.Size(), alignof(uint32));
    Memory::Copy(pPrimitiveCounts, primitiveCounts.Data(), primitiveCounts.Size() * sizeof(uint32));

    HYP_DEFER({
        if (pRangeInfos)
        {
            g_vulkanPool->Free(pRangeInfos);
        }

        if (pPrimitiveCounts)
        {
            g_vulkanPool->Free(pPrimitiveCounts);
        }

        if (pGeometries)
        {
            g_vulkanPool->Free(pGeometries);
        }
    });

    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    geometryInfo.type = ToVkAccelerationStructureType(type);
    geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    geometryInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometryInfo.geometryCount = uint32(geometries.Size());
    geometryInfo.pGeometries = pGeometries;

    Assert(primitiveCounts.Size() == geometries.Size());

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    RI.dynamicFunctions.vkGetAccelerationStructureBuildSizesKHR(
        RI.GetDevice()->GetDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &geometryInfo,
        pPrimitiveCounts,
        &buildSizesInfo);

    const size_t scratchBufferAlignment = RI.GetDevice()->GetFeatures().GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
    size_t accelerationStructureSize = MathUtil::NextMultiple(buildSizesInfo.accelerationStructureSize, 256ull);
    size_t buildScratchSize = MathUtil::NextMultiple(buildSizesInfo.buildScratchSize, scratchBufferAlignment);
    size_t updateScratchSize = MathUtil::NextMultiple(buildSizesInfo.updateScratchSize, scratchBufferAlignment);

    bool wasRebuilt = false;

    if (m_buffer && m_buffer->Size() < accelerationStructureSize)
    {
        EnqueueDeletion(std::move(m_buffer));
        wasRebuilt = true;
    }

    if (!m_buffer)
    {
        m_buffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureBuffer, accelerationStructureSize);
#ifdef HYP_RHI_DEBUG_NAMES
        m_buffer->SetDebugName(NAME("ASBuffer"));
#endif

        CheckResultOrReturn(m_buffer->Create());
    }

    if (!update || wasRebuilt)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

        if (wasRebuilt)
        {
            // delete the current acceleration structure once the frame is done, rather than stalling the gpu here
            RI.GetCurrentFrame()->OnFrameEnd.Bind(
                                                [oldAccelerationStructure = m_accelerationStructure](...)
                                                {
                                                    RI.dynamicFunctions.vkDestroyAccelerationStructureKHR(
                                                        RI.GetDevice()->GetDevice(),
                                                        oldAccelerationStructure,
                                                        nullptr);
                                                })
                .Detach();

            m_accelerationStructure = VK_NULL_HANDLE;
            m_deviceAddress = 0;

            // fetch the corrected acceleration structure and scratch buffer sizes
            // update was true but we need to rebuild from scratch, have to unset the UPDATE flag.
            geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

            RI.dynamicFunctions.vkGetAccelerationStructureBuildSizesKHR(
                RI.GetDevice()->GetDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &geometryInfo,
                pPrimitiveCounts,
                &buildSizesInfo);

            accelerationStructureSize = MathUtil::NextMultiple(buildSizesInfo.accelerationStructureSize, 256ull);
            buildScratchSize = MathUtil::NextMultiple(buildSizesInfo.buildScratchSize, scratchBufferAlignment);
            updateScratchSize = MathUtil::NextMultiple(buildSizesInfo.updateScratchSize, scratchBufferAlignment);

            Assert(m_buffer->Size() >= accelerationStructureSize);
        }

        // to be sure it's zeroed out
        m_buffer->Memset(accelerationStructureSize, 0);

        VkAccelerationStructureCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        createInfo.buffer = m_buffer->GetVulkanHandle();
        createInfo.size = accelerationStructureSize;
        createInfo.type = ToVkAccelerationStructureType(type);

        VULKAN_CHECK(RI.dynamicFunctions.vkCreateAccelerationStructureKHR(
            RI.GetDevice()->GetDevice(),
            &createInfo,
            VK_NULL_HANDLE,
            &m_accelerationStructure));
    }

    Assert(m_accelerationStructure != VK_NULL_HANDLE);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    addressInfo.accelerationStructure = m_accelerationStructure;

    m_deviceAddress = RI.dynamicFunctions.vkGetAccelerationStructureDeviceAddressKHR(
        RI.GetDevice()->GetDevice(),
        &addressInfo);

    const size_t scratchSize = (update && !wasRebuilt) ? updateScratchSize : buildScratchSize;

    if (m_scratchBuffer && m_scratchBuffer->Size() < scratchSize)
    {
        EnqueueDeletion(std::move(m_scratchBuffer));
    }

    if (!m_scratchBuffer)
    {
        m_scratchBuffer = RI.MakeGpuBuffer(GpuBufferType::ScratchBuffer, scratchSize, scratchBufferAlignment);
#ifdef HYP_RHI_DEBUG_NAMES
        m_scratchBuffer->SetDebugName(NAME("ASScratchBuffer"));
#endif

        CheckResultOrReturn(m_scratchBuffer->Create());
    }

    geometryInfo.dstAccelerationStructure = m_accelerationStructure;
    geometryInfo.srcAccelerationStructure = (update && !wasRebuilt) ? m_accelerationStructure : VK_NULL_HANDLE;
    geometryInfo.scratchData = { .deviceAddress = m_scratchBuffer->GetBufferDeviceAddress() };

    pRangeInfos = (VkAccelerationStructureBuildRangeInfoKHR*)g_vulkanPool->Allocate(sizeof(VkAccelerationStructureBuildRangeInfoKHR) * geometries.Size(), alignof(VkAccelerationStructureBuildRangeInfoKHR));

    for (size_t i = 0; i < geometries.Size(); i++)
    {
        pRangeInfos[i] = VkAccelerationStructureBuildRangeInfoKHR {
            .primitiveCount = pPrimitiveCounts[i],
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };
    }

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    HYP_DEFER({ cr.Done(); });

    struct BuildAccelerationStructurePayload
    {
        VkAccelerationStructureBuildGeometryInfoKHR geometryInfo;

        VkAccelerationStructureBuildRangeInfoKHR* pRangeInfos;

        VkAccelerationStructureGeometryKHR* pGeometries;
        uint32* pPrimitiveCounts;

        uint32 numGeometries;
    };

    class BuildAccelerationStructureCmd : public CmdBase
    {
    public:
        BuildAccelerationStructurePayload* payload;

        BuildAccelerationStructureCmd(BuildAccelerationStructurePayload* payload)
            : payload(payload)
        {
        }

        static void InvokeStatic(CmdBase* cmd, VulkanCommandBuffer* commandBuffer)
        {
            BuildAccelerationStructureCmd* cmdCasted = static_cast<BuildAccelerationStructureCmd*>(cmd);

            Array<VkAccelerationStructureBuildRangeInfoKHR*, VulkanTempAllocator> rangeInfoPtrs;
            rangeInfoPtrs.Resize(cmdCasted->payload->numGeometries);

            for (uint32 i = 0; i < cmdCasted->payload->numGeometries; i++)
            {
                rangeInfoPtrs[i] = &cmdCasted->payload->pRangeInfos[i];
            }

            RI.dynamicFunctions.vkCmdBuildAccelerationStructuresKHR(
                commandBuffer->GetVulkanHandle(),
                uint32(rangeInfoPtrs.Size()),
                &cmdCasted->payload->geometryInfo,
                rangeInfoPtrs.Data());

            VkMemoryBarrier memoryBarrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
            memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            vkCmdPipelineBarrier(
                commandBuffer->GetVulkanHandle(),
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                0,
                1, &memoryBarrier,
                0, nullptr,
                0, nullptr);

            g_vulkanPool->Free(cmdCasted->payload->pGeometries);
            g_vulkanPool->Free(cmdCasted->payload->pPrimitiveCounts);
            g_vulkanPool->Free(cmdCasted->payload->pRangeInfos);

            g_vulkanPool->Free(cmdCasted->payload);
        }
    };

    BuildAccelerationStructurePayload* payload = (BuildAccelerationStructurePayload*)g_vulkanPool->Allocate<BuildAccelerationStructurePayload>();

    *payload = {};
    payload->geometryInfo = geometryInfo;
    payload->numGeometries = uint32(geometries.Size());
    payload->pRangeInfos = pRangeInfos;
    payload->pGeometries = pGeometries;
    payload->pPrimitiveCounts = pPrimitiveCounts;

    // Set to nullptr so we don't free the memory (ownership handed off to the command)
    pRangeInfos = nullptr;
    pGeometries = nullptr;
    pPrimitiveCounts = nullptr;

    cr << BuildAccelerationStructureCmd(payload);

    ClearFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);

    return RendererResult();
}

#ifdef HYP_RHI_DEBUG_NAMES

void VulkanAccelerationStructureBase::SetDebugName(Name name)
{
    m_debugName = name;

    if (m_accelerationStructure == VK_NULL_HANDLE)
    {
        return;
    }

    const char* strName = name.LookupString();

    if (RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
        objectNameInfo.objectHandle = (uint64)m_accelerationStructure;
        objectNameInfo.pObjectName = strName;

        RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
    }
}

#endif

#pragma endregion AccelerationStructure

#pragma region TLAS

VulkanGpuTlas::VulkanGpuTlas()
    : VulkanAccelerationStructureBase()
{
}

VulkanGpuTlas::~VulkanGpuTlas()
{
    m_instancesBuffer.Reset();
    m_scratchBuffer.Reset();

    for (VulkanGpuBlas* blas : m_blases)
    {
        blas->Release();
    }

    m_blases.Clear();

    if (RI.bindlessStorage != nullptr)
    {
        for (auto& it : m_keyToBlasAndStorageId)
        {
            const uint32 storageId = it.second.second;

            RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2);     // VB
            RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2 + 1); // IB

            RI.bindlessStorage->ReleaseId(BindlessStorage_Buffers, storageId);
        }
    }
}

bool VulkanGpuTlas::IsCreated() const
{
    return m_accelerationStructure != VK_NULL_HANDLE;
}

Array<VkAccelerationStructureGeometryKHR, VulkanTempAllocator> VulkanGpuTlas::GetGeometries() const
{
    Assert(m_instancesBuffer != nullptr && m_instancesBuffer->IsCreated());

    return {
        { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
          .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
          .geometry = {
              .instances = {
                  .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                  .arrayOfPointers = VK_FALSE,
                  .data = { .deviceAddress = m_instancesBuffer->GetBufferDeviceAddress() } } },
          .flags = VK_GEOMETRY_OPAQUE_BIT_KHR }
    };
}

Array<uint32, VulkanTempAllocator> VulkanGpuTlas::GetPrimitiveCounts() const
{
    return { uint32(m_blases.Size()) };
}

RendererResult VulkanGpuTlas::Create()
{
    if (IsCreated())
    {
        return {};
    }

    m_meshDescriptionsBuffer.Initialize();

    Assert(m_accelerationStructure == VK_NULL_HANDLE);

    if (m_blases.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Top level acceleration structure must have at least one GpuBlas");
    }

    for (VulkanGpuBlas* blas : m_blases)
    {
        Assert(blas != nullptr);

        CheckResultOrReturn(blas->Create());
    }

    CheckResultOrReturn(BuildInstancesBuffer());

    RTUpdateStateFlags updateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    Array<VkAccelerationStructureGeometryKHR, VulkanTempAllocator> geometries = GetGeometries();
    Array<uint32, VulkanTempAllocator> primitiveCounts = GetPrimitiveCounts();

    CheckResultOrReturn(CreateAccelerationStructure(GetType(), geometries, primitiveCounts, false, updateStateFlags));

    Assert(updateStateFlags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    CheckResultOrReturn(BuildMeshDescriptionsBuffer());
    updateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return RendererResult();
}

void VulkanGpuTlas::AddGpuBlas(uint64 key, VulkanGpuBlas* blas)
{
    Assert(blas != nullptr);

    if (m_keyToBlasAndStorageId.Find(key) != m_keyToBlasAndStorageId.End())
    {
        // already has the GpuBlas
        return;
    }

    AssertDebug(m_blases.Size() < MaxBlases, "Cannot add any more BLASes to TLAS, limit reached");

    if (m_blases.Size() == MaxBlases)
    {
        return;
    }

    Assert(blas->IsCreated());
    Assert(!blas->GetGeometries().Empty());

    for (const VulkanAccelerationGeometry& geometry : blas->GetGeometries())
    {
        Assert(geometry.GetPackedVerticesBuffer().IsValid());
        Assert(geometry.GetPackedIndicesBuffer().IsValid());
    }

    auto& entry = m_keyToBlasAndStorageId[key];
    entry.first = blas;
    entry.second = ~0u;

    blas->AddRef();

    m_blases.PushBack(blas);
    m_keys.PushBack(key);

    SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
}

void VulkanGpuTlas::RemoveGpuBlas(uint64 key)
{
    auto it = m_keyToBlasAndStorageId.Find(key);

    AssertDebug(it != m_keyToBlasAndStorageId.End());

    if (it != m_keyToBlasAndStorageId.End())
    {
        VulkanGpuBlas*& blas = it->second.first;
        uint32 storageId = it->second.second;

        RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2);
        RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2 + 1);

        RI.bindlessStorage->ReleaseId(BindlessStorage_Buffers, storageId);

        auto blasesIt = m_blases.Find(blas);
        Assert(blasesIt != m_blases.End());

        blas->Release();
        blas = nullptr;

        const size_t dist = std::distance(m_blases.Begin(), blasesIt);
        AssertDebug(dist < m_keys.Size());

        m_keys.Erase(m_keys.Begin() + dist);
        m_blases.Erase(blasesIt);

        m_keyToBlasAndStorageId.Erase(it);

        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }
}

bool VulkanGpuTlas::HasGpuBlas(uint64 key)
{
    return m_keyToBlasAndStorageId.Find(key) != m_keyToBlasAndStorageId.End();
}

RendererResult VulkanGpuTlas::BuildInstancesBuffer()
{
    return BuildInstancesBuffer(0, uint32(m_blases.Size()));
}

RendererResult VulkanGpuTlas::BuildInstancesBuffer(uint32 first, uint32 last)
{
    if (last <= first)
    {
        // nothing to update
        return RendererResult();
    }

    last = MathUtil::Min(uint32(m_blases.Size()), last);

    if (m_blases.Empty() || last <= first)
    {
        // no need to update the data inside
        return RendererResult();
    }

    constexpr size_t minInstancesBufferSize = sizeof(VkAccelerationStructureInstanceKHR);
    const size_t instancesBufferSize = MathUtil::Max(minInstancesBufferSize, m_blases.Size() * sizeof(VkAccelerationStructureInstanceKHR));

    bool instancesBufferRecreated = false;

    if (m_instancesBuffer && m_instancesBuffer->Size() < instancesBufferSize)
    {
        EnqueueDeletion(std::move(m_instancesBuffer));
    }

    if (!m_instancesBuffer)
    {
        m_instancesBuffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureInstanceBuffer, instancesBufferSize);
#ifdef HYP_RHI_DEBUG_NAMES
        m_instancesBuffer->SetDebugName(NAME("ASInstancesBuffer"));
#endif

        CheckResultOrReturn(m_instancesBuffer->Create());

        instancesBufferRecreated = true;
    }

    if (instancesBufferRecreated)
    {
        // zero out buffer
        m_instancesBuffer->Memset(m_instancesBuffer->Size(), 0x0);

        // set dirty range to all elements if resized or newly created
        first = 0;
        last = uint32(m_blases.Size());
    }

    Array<VkAccelerationStructureInstanceKHR, VulkanTempAllocator> instances;
    instances.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        VulkanGpuBlas* blas = m_blases[i];
        Assert(blas != nullptr);

        const uint32 instanceIndex = i; /* Index of mesh in mesh descriptions buffer. */

        VkAccelerationStructureInstanceKHR& desc = instances[i - first];
        desc.transform = ToVkTransform(blas->GetTransform());
        desc.instanceCustomIndex = instanceIndex;
        desc.mask = 0xFFu;
        desc.instanceShaderBindingTableRecordOffset = 0;
        desc.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        desc.accelerationStructureReference = blas->GetDeviceAddress();
    }

    Assert(m_instancesBuffer != nullptr);
    Assert(m_instancesBuffer->Size() >= (first + instances.Size()) * sizeof(VkAccelerationStructureInstanceKHR));

    m_instancesBuffer->Copy(
        first * sizeof(VkAccelerationStructureInstanceKHR),
        instances.Size() * sizeof(VkAccelerationStructureInstanceKHR),
        instances.Data());

    return RendererResult();
}

RendererResult VulkanGpuTlas::BuildMeshDescriptionsBuffer()
{
    return BuildMeshDescriptionsBuffer(0u, uint32(m_blases.Size()));
}

RendererResult VulkanGpuTlas::BuildMeshDescriptionsBuffer(uint32 first, uint32 last)
{
    if (last <= first)
    {
        // nothing to update
        return RendererResult();
    }

    last = MathUtil::Min(m_blases.Size(), last);

    if (m_blases.Empty() || last <= first)
    {
        // no need to update the data inside
        return RendererResult();
    }

    Array<MeshDescription, VulkanTempAllocator> meshDescriptions;
    meshDescriptions.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        VulkanGpuBlas* blas = m_blases[i];
        uint64 key = m_keys[i];

        MeshDescription& meshDescription = meshDescriptions[i - first];
        meshDescription = {};

        Assert(blas->GetGeometries().Any(), "No geometries added to GpuBlas node %u!", i);

        Assert(blas->GetGeometries()[0].GetPackedVerticesBuffer().IsValid() && blas->GetGeometries()[0].GetPackedVerticesBuffer()->IsCreated());
        Assert(blas->GetGeometries()[0].GetPackedIndicesBuffer().IsValid() && blas->GetGeometries()[0].GetPackedIndicesBuffer()->IsCreated());

        uint32 storageId = ~0u;

        { // allocate / update resources in bindless storage
            storageId = m_keyToBlasAndStorageId[key].second;

            if (storageId == ~0u)
            {
                storageId = RI.bindlessStorage->AllocateId(BindlessStorage_Buffers);
                AssertDebug(!(storageId & StorageIdDirtyBit));
                storageId |= StorageIdDirtyBit;
            }

            if (storageId & StorageIdDirtyBit)
            {
                storageId &= ~StorageIdDirtyBit;

                m_keyToBlasAndStorageId[key].second = storageId;

                RI.bindlessStorage->AddResource(BindlessStorage_Buffers, storageId * 2, blas->GetGeometries()[0].GetPackedVerticesBuffer());
                RI.bindlessStorage->AddResource(BindlessStorage_Buffers, storageId * 2 + 1, blas->GetGeometries()[0].GetPackedIndicesBuffer());
            }
        }

        meshDescription.bindlessIndex = storageId;
        meshDescription.materialIndex = blas->GetMaterialBinding();
        meshDescription.numIndices = blas->GetGeometries()[0].NumIndices();
        meshDescription.numVertices = blas->GetGeometries()[0].NumVertices();
    }

    m_meshDescriptionsBuffer.Write(
        first * sizeof(MeshDescription),
        meshDescriptions.Size() * sizeof(MeshDescription),
        meshDescriptions.Data());

    m_meshDescriptionsBuffer.FlushBatched();

    return RendererResult();
}

RendererResult VulkanGpuTlas::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(outUpdateStateFlags);
    }

    Range<uint32> dirtyRange = Range<uint32>::Invalid();

    for (uint32 i = 0; i < uint32(m_blases.Size()); i++)
    {
        VulkanGpuBlas* blas = m_blases[i];
        Assert(blas != nullptr);

        RTUpdateStateFlags blasUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;
        CheckResultOrReturn(blas->UpdateStructure(blasUpdateStateFlags));

        if (blasUpdateStateFlags)
        {
            dirtyRange |= Range { i, i + 1 };
        }
    }

    if (dirtyRange)
    {
        CheckResultOrReturn(BuildInstancesBuffer(dirtyRange.GetStart(), dirtyRange.GetEnd()));
        CheckResultOrReturn(BuildMeshDescriptionsBuffer(dirtyRange.GetStart(), dirtyRange.GetEnd()));

        Array<VkAccelerationStructureGeometryKHR, VulkanTempAllocator> geometries = GetGeometries();
        Array<uint32, VulkanTempAllocator> primitiveCounts = GetPrimitiveCounts();

        CheckResultOrReturn(CreateAccelerationStructure(GetType(), geometries, primitiveCounts, true, outUpdateStateFlags));

        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS | RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;
    }

    return RendererResult();
}

RendererResult VulkanGpuTlas::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    Assert(m_accelerationStructure != VK_NULL_HANDLE);

    // check each GpuBlas, assert that it is valid.
    for (VulkanGpuBlas* blas : m_blases)
    {
        Assert(blas != nullptr);
        Assert(blas->IsCreated());
        Assert(!blas->GetGeometries().Empty());

        for (const VulkanAccelerationGeometry& geometry : blas->GetGeometries())
        {
            Assert(geometry.GetPackedVerticesBuffer().IsValid());
            Assert(geometry.GetPackedIndicesBuffer().IsValid());
        }
    }

    CheckResultOrReturn(BuildInstancesBuffer());
    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;

    Array<VkAccelerationStructureGeometryKHR, VulkanTempAllocator> geometries = GetGeometries();
    Array<uint32, VulkanTempAllocator> primitiveCounts = GetPrimitiveCounts();

    CheckResultOrReturn(CreateAccelerationStructure(
        GetType(),
        geometries, primitiveCounts,
        true,
        outUpdateStateFlags));

    CheckResultOrReturn(BuildMeshDescriptionsBuffer());
    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return RendererResult();
}

#pragma endregion TLAS

#pragma region GpuBlas

VulkanGpuBlas::VulkanGpuBlas(
    const VulkanGpuBufferRef& packedVerticesBuffer,
    const VulkanGpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Mat4f& transform)
    : VulkanAccelerationStructureBase(transform),
      m_packedVerticesBuffer(packedVerticesBuffer),
      m_packedIndicesBuffer(packedIndicesBuffer)
{
    m_material = material;

    m_geometries.EmplaceBack(
        m_packedVerticesBuffer,
        m_packedIndicesBuffer,
        numVertices,
        numIndices,
        m_material);
}

VulkanGpuBlas::~VulkanGpuBlas()
{
    HYP_LOG_TEMP("DESTROY ACCELERATION STRUCTURE {} {} {}", InstanceClass()->GetName(), Id(), m_packedVerticesBuffer->GetDebugName());

    EnqueueDeletion(std::move(m_material));
    EnqueueDeletion(std::move(m_packedVerticesBuffer));
    EnqueueDeletion(std::move(m_packedIndicesBuffer));
}

bool VulkanGpuBlas::IsCreated() const
{
    return m_accelerationStructure != VK_NULL_HANDLE;
}

RendererResult VulkanGpuBlas::Create()
{
    HYP_LOG_TEMP("CREATE ACCELERATION STRUCTURE {} {} {}", InstanceClass()->GetName(), Id(), m_packedVerticesBuffer->GetDebugName());

    if (IsCreated())
    {
        return {};
    }

    RendererResult result;

    Array<VkAccelerationStructureGeometryKHR, VulkanTempAllocator> geometries(m_geometries.Size());
    Array<uint32, VulkanTempAllocator> primitiveCounts(m_geometries.Size());

    if (m_geometries.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create GpuBlas with zero geometries");
    }

    size_t geometryIdx = 0;

    for (VulkanAccelerationGeometry& geometry : m_geometries)
    {
        if (!geometry.IsCreated())
        {
            CheckResultOrReturn(geometry.Create());
        }

        geometries[geometryIdx] = geometry.m_geometry;
        primitiveCounts[geometryIdx] = uint32(geometry.GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);

        if (primitiveCounts[geometryIdx] == 0)
        {
            return HYP_MAKE_ERROR(RendererError, "Cannot create GpuBlas -- geometry has zero indices");
        }

        ++geometryIdx;
    }

    RTUpdateStateFlags updateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    CheckResultOrReturn(CreateAccelerationStructure(GetType(), geometries, primitiveCounts, false, updateStateFlags));
    Assert(updateStateFlags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    return RendererResult();
}

RendererResult VulkanGpuBlas::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MATERIAL;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(outUpdateStateFlags);
    }

    return RendererResult();
}

RendererResult VulkanGpuBlas::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    HYP_NOT_IMPLEMENTED();

#if 0
    Array<VkAccelerationStructureGeometryKHR, VulkanAllocator> geometries(m_geometries.Size());
    Array<uint32, VulkanAllocator> primitiveCounts(m_geometries.Size());

    for (size_t i = 0; i < m_geometries.Size(); i++)
    {
        const VulkanAccelerationGeometryRef& geometry = m_geometries[i];
        Assert(geometry != nullptr);

        geometries[i] = geometry->m_geometry;
        primitiveCounts[i] = uint32(geometry->GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);
    }

    CheckResultOrReturn(CreateAccelerationStructure(GetType(), geometries, primitiveCounts, true, outUpdateStateFlags));

    m_flags &= ~(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING | ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);

    return RendererResult();
#endif
}

#pragma endregion GpuBlas

} // namespace Hyperion
