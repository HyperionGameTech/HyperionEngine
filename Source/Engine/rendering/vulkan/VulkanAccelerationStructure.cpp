/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanAccelerationStructure.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>

#include <rendering/Material.hpp>
#include <rendering/Shared.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/utilities/Range.hpp>

#include <Core/math/MathUtil.hpp>

#include <VulkanAccelerationStructure.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

static VkTransformMatrixKHR ToVkTransform(const Mat4f& matrix)
{
    VkTransformMatrixKHR transform;
    std::memcpy(&transform, matrix.values, sizeof(VkTransformMatrixKHR));

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
    : m_isCreated(false),
      m_packedVerticesBuffer(packedVerticesBuffer),
      m_packedIndicesBuffer(packedIndicesBuffer),
      m_numVertices(numVertices),
      m_numIndices(numIndices),
      m_material(material),
      m_geometry {}
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

    if (!g_renderInterface->GetDevice()->GetFeatures().IsRayTracingSupported())
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
            .transformData = { {} }
        }
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
    m_buffer.Reset();
    m_scratchBuffer.Reset();

    if (m_accelerationStructure != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([accelerationStructure = m_accelerationStructure]()
            {
                g_vulkanDynamicFunctions->vkDestroyAccelerationStructureKHR(
                    g_renderInterface->GetDevice()->GetDevice(),
                    accelerationStructure,
                    VK_NULL_HANDLE);
            }));

        m_accelerationStructure = VK_NULL_HANDLE;
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

    if (!g_renderInterface->GetDevice()->GetFeatures().IsRayTracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support rayTracing");
    }

    if (!geometries)
    {
        return HYP_MAKE_ERROR(RendererError, "Geometries empty");
    }

    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    geometryInfo.type = ToVkAccelerationStructureType(type);
    geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    geometryInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometryInfo.geometryCount = uint32(geometries.Size());
    geometryInfo.pGeometries = geometries.Data();

    Assert(primitiveCounts.Size() == geometries.Size());

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    g_vulkanDynamicFunctions->vkGetAccelerationStructureBuildSizesKHR(
        g_renderInterface->GetDevice()->GetDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &geometryInfo,
        primitiveCounts.Data(),
        &buildSizesInfo);

    const size_t scratchBufferAlignment = g_renderInterface->GetDevice()->GetFeatures().GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
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
        m_buffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::ACCELERATION_STRUCTURE_BUFFER, accelerationStructureSize);

#if HYP_DEBUG_MODE
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
            g_renderInterface->GetCurrentFrame()->OnFrameEnd
                .Bind([oldAccelerationStructure = m_accelerationStructure](...)
                {
                    g_vulkanDynamicFunctions->vkDestroyAccelerationStructureKHR(
                        g_renderInterface->GetDevice()->GetDevice(),
                        oldAccelerationStructure,
                        nullptr);
                })
                .Detach();

            m_accelerationStructure = VK_NULL_HANDLE;
            m_deviceAddress = 0;

            // fetch the corrected acceleration structure and scratch buffer sizes
            // update was true but we need to rebuild from scratch, have to unset the UPDATE flag.
            geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

            g_vulkanDynamicFunctions->vkGetAccelerationStructureBuildSizesKHR(
                g_renderInterface->GetDevice()->GetDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &geometryInfo,
                primitiveCounts.Data(),
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

        VULKAN_CHECK(g_vulkanDynamicFunctions->vkCreateAccelerationStructureKHR(
            g_renderInterface->GetDevice()->GetDevice(),
            &createInfo,
            VK_NULL_HANDLE,
            &m_accelerationStructure));
    }

    Assert(m_accelerationStructure != VK_NULL_HANDLE);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    addressInfo.accelerationStructure = m_accelerationStructure;

    m_deviceAddress = g_vulkanDynamicFunctions->vkGetAccelerationStructureDeviceAddressKHR(
        g_renderInterface->GetDevice()->GetDevice(),
        &addressInfo);

    const size_t scratchSize = (update && !wasRebuilt) ? updateScratchSize : buildScratchSize;

    if (m_scratchBuffer && m_scratchBuffer->Size() < scratchSize)
    {
        EnqueueDeletion(std::move(m_scratchBuffer));
    }

    if (!m_scratchBuffer)
    {
        m_scratchBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::SCRATCH_BUFFER, scratchSize, scratchBufferAlignment);

#if HYP_DEBUG_MODE
        m_scratchBuffer->SetDebugName(NAME("ASScratchBuffer"));
#endif

        CheckResultOrReturn(m_scratchBuffer->Create());
    }

    // zero out scratch buffer
    m_scratchBuffer->Memset(m_scratchBuffer->Size(), 0);

    geometryInfo.dstAccelerationStructure = m_accelerationStructure;
    geometryInfo.srcAccelerationStructure = (update && !wasRebuilt) ? m_accelerationStructure : VK_NULL_HANDLE;
    geometryInfo.scratchData = { .deviceAddress = m_scratchBuffer->GetBufferDeviceAddress() };

    Array<VkAccelerationStructureBuildRangeInfoKHR, VulkanAllocator> rangeInfos;
    rangeInfos.Resize(geometries.Size());

    Array<VkAccelerationStructureBuildRangeInfoKHR*, VulkanAllocator> rangeInfoPtrs;
    rangeInfoPtrs.Resize(geometries.Size());

    for (size_t i = 0; i < geometries.Size(); i++)
    {
        rangeInfos[i] = VkAccelerationStructureBuildRangeInfoKHR {
            .primitiveCount = primitiveCounts[i],
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };

        rangeInfoPtrs[i] = &rangeInfos[i];
    }

    VulkanCommandBufferRef commandBuffer = MakeHandle<VulkanCommandBuffer>();
    commandBuffer->Create(g_renderInterface->GetDevice()->GetGraphicsQueue()->commandPools[0]);

    commandBuffer->Begin();

    g_vulkanDynamicFunctions->vkCmdBuildAccelerationStructuresKHR(
        commandBuffer->GetVulkanHandle(),
        uint32(rangeInfoPtrs.Size()),
        &geometryInfo,
        rangeInfoPtrs.Data());

    VkMemoryBarrier memoryBarrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr);

    commandBuffer->End();

    CheckResultOrReturn(commandBuffer->Submit(g_renderInterface->GetDevice()->GetGraphicsQueue(), nullptr, nullptr, nullptr));

    EnqueueDeletion(std::move(commandBuffer));

    ClearFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);

    return RendererResult();
}

void VulkanAccelerationStructureBase::RemoveGeometry(uint32 index)
{
    const auto it = m_geometries.Begin() + index;

    if (it >= m_geometries.End())
    {
        return;
    }

    EnqueueDeletion(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

void VulkanAccelerationStructureBase::RemoveGeometry(const VulkanAccelerationGeometryRef& geometry)
{
    if (geometry == nullptr)
    {
        return;
    }

    const auto it = m_geometries.Find(geometry);

    if (it == m_geometries.End())
    {
        return;
    }

    EnqueueDeletion(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

void VulkanAccelerationStructureBase::SetDebugName(Name name)
{
    m_debugName = name;

#if HYP_DEBUG_MODE

    if (m_accelerationStructure == VK_NULL_HANDLE)
    {
        return;
    }

    const char* strName = name.LookupString();

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
    objectNameInfo.objectHandle = (uint64)m_accelerationStructure;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(g_renderInterface->GetDevice()->GetDevice(), &objectNameInfo);

#endif
}

#pragma endregion AccelerationStructure

#pragma region TLAS

VulkanGpuTlas::VulkanGpuTlas()
    : VulkanAccelerationStructureBase()
{
}

VulkanGpuTlas::~VulkanGpuTlas()
{
    m_instancesBuffer.Reset();
    m_meshDescriptionsBuffer.Reset();
    m_scratchBuffer.Reset();

    for (VulkanGpuBlas* blas : m_blases)
    {
        blas->Release();
    }

    m_blases.Clear();

    for (auto& it : m_keyToBlasAndStorageId)
    {
        const uint32 storageId = it.second.second;

        g_renderInterface->bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2);       // VB
        g_renderInterface->bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2 + 1);   // IB

        g_renderInterface->bindlessStorage->ReleaseId(BindlessStorage_Buffers, storageId);
    }
}

bool VulkanGpuTlas::IsCreated() const
{
    return m_accelerationStructure != VK_NULL_HANDLE;
}

Array<VkAccelerationStructureGeometryKHR, VulkanAllocator> VulkanGpuTlas::GetGeometries() const
{
    Assert(m_instancesBuffer != nullptr && m_instancesBuffer->IsCreated());

    return {
        { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
                .instances = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                    .arrayOfPointers = VK_FALSE,
                    .data = { .deviceAddress = m_instancesBuffer->GetBufferDeviceAddress() }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        }
    };
}

Array<uint32, VulkanAllocator> VulkanGpuTlas::GetPrimitiveCounts() const
{
    return { uint32(m_blases.Size()) };
}

RendererResult VulkanGpuTlas::Create()
{
    if (IsCreated())
    {
        return {};
    }

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

    Array<VkAccelerationStructureGeometryKHR, VulkanAllocator> geometries = GetGeometries();
    Array<uint32, VulkanAllocator> primitiveCounts = GetPrimitiveCounts();

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

    Assert(blas->IsCreated());
    Assert(!blas->GetGeometries().Empty());

    for (const VulkanAccelerationGeometryRef& geometry : blas->GetGeometries())
    {
        Assert(geometry != nullptr);
        Assert(geometry->GetPackedVerticesBuffer() != nullptr);
        Assert(geometry->GetPackedIndicesBuffer() != nullptr);
    }

    /*if (IsCreated())
    {
        // If the TLAS is already created, we need to ensure that the GpuBlas is created as well.
        if (!vulkanBlas->IsCreated())
        {
            Assert(vulkanBlas->Create());
        }
    }*/

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
        VulkanGpuBlas* blas = it->second.first;
        uint32 storageId = it->second.second;

        g_renderInterface->bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2);
        g_renderInterface->bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2 + 1);

        g_renderInterface->bindlessStorage->ReleaseId(BindlessStorage_Buffers, storageId);

        auto blasesIt = m_blases.Find(blas);
        Assert(blasesIt != m_blases.End());

        blas->Release();

        auto keysIt = m_keys.Begin() + std::distance(m_blases.Begin(), blasesIt);
        m_keys.Erase(keysIt);

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

    /// Temporarily commented out
    // if (m_blas.Empty() || last <= first)
    //{
    //     // no need to update the data inside
    //     return RendererResult();
    // }

    constexpr size_t minInstancesBufferSize = sizeof(VkAccelerationStructureInstanceKHR);
    const size_t instancesBufferSize = MathUtil::Max(minInstancesBufferSize, m_blases.Size() * sizeof(VkAccelerationStructureInstanceKHR));

    bool instancesBufferRecreated = false;

    if (m_instancesBuffer && m_instancesBuffer->Size() < instancesBufferSize)
    {
        EnqueueDeletion(std::move(m_instancesBuffer));
    }

    if (!m_instancesBuffer)
    {
        m_instancesBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER, instancesBufferSize);
#if HYP_DEBUG_MODE
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

    Array<VkAccelerationStructureInstanceKHR, VulkanAllocator> instances;
    instances.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        VulkanGpuBlas* blas = m_blases[i];
        Assert(blas != nullptr);

        const uint32 instanceIndex = i; /* Index of mesh in mesh descriptions buffer. */

        instances[i - first] = VkAccelerationStructureInstanceKHR {
            .transform = ToVkTransform(blas->GetTransform()),
            .instanceCustomIndex = instanceIndex,
            .mask = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blas->GetDeviceAddress()
        };
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

    constexpr size_t minMeshDescriptionsBufferSize = sizeof(MeshDescription);
    const size_t meshDescriptionsBufferSize = MathUtil::Max(minMeshDescriptionsBufferSize, sizeof(MeshDescription) * m_blases.Size());

    bool meshDescriptionsBufferRecreated = false;
    
    if (m_meshDescriptionsBuffer && m_meshDescriptionsBuffer->Size() < meshDescriptionsBufferSize)
    {
        EnqueueDeletion(std::move(m_meshDescriptionsBuffer));
    }

    if (!m_meshDescriptionsBuffer)
    {
        m_meshDescriptionsBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, meshDescriptionsBufferSize);
#if HYP_DEBUG_MODE
        m_meshDescriptionsBuffer->SetDebugName(NAME("ASMeshDescriptionsBuffer"));
#endif
        m_meshDescriptionsBuffer->SetIsCpuAccessible(true);
        CheckResultOrReturn(m_meshDescriptionsBuffer->Create());

        meshDescriptionsBufferRecreated = true;
    }

    if (meshDescriptionsBufferRecreated)
    {
        // zero out buffer
        m_meshDescriptionsBuffer->Memset(m_meshDescriptionsBuffer->Size(), 0x0);

        // set dirty range to all elements if resized or newly created
        first = 0;
        last = uint32(m_blases.Size());
    }

    if (m_blases.Empty() || last <= first)
    {
        // no need to update the data inside
        return RendererResult();
    }

    Array<MeshDescription, VulkanAllocator> meshDescriptions;
    meshDescriptions.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        VulkanGpuBlas* blas = m_blases[i];
        uint64 key = m_keys[i];

        MeshDescription& meshDescription = meshDescriptions[i - first];
        meshDescription = {};

        Assert(blas->GetGeometries().Any(), "No geometries added to GpuBlas node %u!", i);

        Assert(blas->GetGeometries()[0]->GetPackedVerticesBuffer() && blas->GetGeometries()[0]->GetPackedVerticesBuffer()->IsCreated());
        Assert(blas->GetGeometries()[0]->GetPackedIndicesBuffer() && blas->GetGeometries()[0]->GetPackedIndicesBuffer()->IsCreated());

        uint32 storageId = ~0u;

        { // allocate / update resources in bindless storage
            storageId = m_keyToBlasAndStorageId[key].second;

            if (storageId == ~0u)
            {
                storageId = g_renderInterface->bindlessStorage->AllocateId(BindlessStorage_Buffers);
                AssertDebug(!(storageId & StorageIdDirtyBit));
                storageId |= StorageIdDirtyBit;
            }

            if (storageId & StorageIdDirtyBit)
            {
                storageId &= ~StorageIdDirtyBit;

                m_keyToBlasAndStorageId[key].second = storageId;
                
                g_renderInterface->bindlessStorage->AddResource(BindlessStorage_Buffers, storageId * 2, blas->GetGeometries()[0]->GetPackedVerticesBuffer());
                g_renderInterface->bindlessStorage->AddResource(BindlessStorage_Buffers, storageId * 2 + 1, blas->GetGeometries()[0]->GetPackedIndicesBuffer());
            }
        }

        meshDescription.bindlessIndex = storageId;
        meshDescription.materialIndex = blas->GetMaterialBinding();
        meshDescription.numIndices = blas->GetGeometries()[0]->NumIndices();
        meshDescription.numVertices = blas->GetGeometries()[0]->NumVertices();
    }
    
    Assert(m_meshDescriptionsBuffer != nullptr);
    Assert(m_meshDescriptionsBuffer->Size() >= (first + meshDescriptions.Size()) * sizeof(MeshDescription));

    m_meshDescriptionsBuffer->Copy(
        first * sizeof(MeshDescription),
        meshDescriptions.Size() * sizeof(MeshDescription),
        meshDescriptions.Data());

    m_meshDescriptionsBuffer->Flush(first * sizeof(MeshDescription), meshDescriptions.Size() * sizeof(MeshDescription));

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

        Array<VkAccelerationStructureGeometryKHR, VulkanAllocator> geometries = GetGeometries();
        Array<uint32, VulkanAllocator> primitiveCounts = GetPrimitiveCounts();

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

        for (const VulkanAccelerationGeometryRef& geometry : blas->GetGeometries())
        {
            Assert(geometry != nullptr);
            Assert(geometry->GetPackedVerticesBuffer() != nullptr);
            Assert(geometry->GetPackedIndicesBuffer() != nullptr);
        }
    }

    CheckResultOrReturn(BuildInstancesBuffer());
    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;

    Array<VkAccelerationStructureGeometryKHR, VulkanAllocator> geometries = GetGeometries();
    Array<uint32, VulkanAllocator> primitiveCounts = GetPrimitiveCounts();

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

    m_geometries.PushBack(MakeHandle<VulkanAccelerationGeometry>(
        m_packedVerticesBuffer,
        m_packedIndicesBuffer,
        numVertices,
        numIndices,
        m_material));
}

VulkanGpuBlas::~VulkanGpuBlas()
{
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
    if (IsCreated())
    {
        return {};
    }

    RendererResult result;

    Array<VkAccelerationStructureGeometryKHR, VulkanAllocator> geometries(m_geometries.Size());
    Array<uint32, VulkanAllocator> primitiveCounts(m_geometries.Size());

    if (m_geometries.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create GpuBlas with zero geometries");
    }

    for (size_t i = 0; i < m_geometries.Size(); i++)
    {
        const VulkanAccelerationGeometryRef& geometry = m_geometries[i];

        if (!geometry->IsCreated())
        {
            CheckResultOrReturn(geometry->Create());
        }

        geometries[i] = geometry->m_geometry;
        primitiveCounts[i] = uint32(geometry->GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);

        if (primitiveCounts[i] == 0)
        {
            return HYP_MAKE_ERROR(RendererError, "Cannot create GpuBlas -- geometry has zero indices");
        }
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
