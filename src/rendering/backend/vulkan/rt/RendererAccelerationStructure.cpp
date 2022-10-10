#include <rendering/backend/rt/RendererAccelerationStructure.hpp>
#include "../RendererDevice.hpp"
#include "../RendererInstance.hpp"
#include "../RendererFeatures.hpp"

#include <core/lib/Range.hpp>

namespace hyperion {
namespace renderer {

static VkTransformMatrixKHR ToVkTransform(const Matrix4 &matrix)
{
    VkTransformMatrixKHR transform;
	std::memcpy(&transform, &matrix, sizeof(VkTransformMatrixKHR));

	return transform;
}

VkAccelerationStructureTypeKHR AccelerationStructure::ToVkAccelerationStructureType(AccelerationStructureType type)
{
	switch (type) {
	case AccelerationStructureType::BOTTOM_LEVEL:
		return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	case AccelerationStructureType::TOP_LEVEL:
		return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	default:
		return VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
	}
}

AccelerationGeometry::AccelerationGeometry(
    std::vector<PackedVertex> &&packed_vertices,
    std::vector<PackedIndex> &&packed_indices,
    UInt material_index
) : m_packed_vertices(std::move(packed_vertices)),
    m_packed_indices(std::move(packed_indices)),
	m_material_index(material_index),
    m_geometry { }
{
}

AccelerationGeometry::~AccelerationGeometry()
{
}

Result AccelerationGeometry::Create(Device *device, Instance *instance)
{
	AssertThrow(m_packed_vertex_buffer == nullptr);
	AssertThrow(m_packed_index_buffer == nullptr);

	if (!device->GetFeatures().SupportsRaytracing()) {
	    return { Result::RENDERER_ERR, "Device does not support raytracing" };
	}

	if (m_packed_vertices.empty() || m_packed_indices.empty()) {
	    return { Result::RENDERER_ERR, "An acceleration geometry must have nonzero vertex count, index count, and vertex size." };
	}

	// some assertions to prevent gpu faults
	for (SizeType i = 0; i < m_packed_indices.size(); i++) {
	    AssertThrow(m_packed_indices[i] < m_packed_vertices.size());
	}

	auto result = Result::OK;

	const SizeType packed_vertices_size = m_packed_vertices.size() * sizeof(PackedVertex);
	const SizeType packed_indices_size = m_packed_indices.size() * sizeof(PackedIndex);

	m_packed_vertex_buffer = std::make_unique<PackedVertexStorageBuffer>();

	HYPERION_BUBBLE_ERRORS(
		m_packed_vertex_buffer->Create(device, packed_vertices_size)
	);

	m_packed_index_buffer = std::make_unique<PackedIndexStorageBuffer>();

	HYPERION_PASS_ERRORS(
		m_packed_index_buffer->Create(device, packed_indices_size),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

	StagingBuffer vertices_staging_buffer;
	HYPERION_BUBBLE_ERRORS(vertices_staging_buffer.Create(device, packed_vertices_size));
	vertices_staging_buffer.Memset(device, packed_vertices_size, 0x0); // zero out
    vertices_staging_buffer.Copy(device, m_packed_vertices.size() * sizeof(PackedVertex), m_packed_vertices.data());
	
	if (!result) {
	    HYPERION_IGNORE_ERRORS(vertices_staging_buffer.Destroy(device));

		return result;
	}
	
	StagingBuffer indices_staging_buffer;
	HYPERION_BUBBLE_ERRORS(indices_staging_buffer.Create(device, packed_indices_size));
	indices_staging_buffer.Memset(device, packed_indices_size, 0x0); // zero out
	indices_staging_buffer.Copy(device, m_packed_indices.size() * sizeof(PackedIndex), m_packed_indices.data());

	if (!result) {
	    HYPERION_IGNORE_ERRORS(vertices_staging_buffer.Destroy(device));
	    HYPERION_IGNORE_ERRORS(indices_staging_buffer.Destroy(device));

		return result;
	}

	auto commands = instance->GetSingleTimeCommands();
    commands.Push([&](CommandBuffer *cmd) {
        m_packed_vertex_buffer->CopyFrom(cmd, &vertices_staging_buffer, packed_vertices_size);
        m_packed_index_buffer->CopyFrom(cmd, &indices_staging_buffer, packed_indices_size);

        HYPERION_RETURN_OK;
    });

    HYPERION_PASS_ERRORS(commands.Execute(device), result);

	HYPERION_PASS_ERRORS(vertices_staging_buffer.Destroy(device), result);
	HYPERION_PASS_ERRORS(indices_staging_buffer.Destroy(device), result);
	/*HYPERION_PASS_ERRORS(
		m_packed_vertex_buffer->CopyStaged(
		    instance,
		    m_packed_vertices.data(),
		    m_packed_vertices.size() * sizeof(PackedVertex)
	    ),
		result
	);

	HYPERION_PASS_ERRORS(
        m_packed_index_buffer->CopyStaged(
		    instance,
			m_packed_indices.data(),
		    m_packed_indices.size() * sizeof(PackedIndex)
	    ),
		result
	);*/

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

    VkDeviceOrHostAddressConstKHR vertices_address {
        .deviceAddress = m_packed_vertex_buffer->GetBufferDeviceAddress(device)
    };

    VkDeviceOrHostAddressConstKHR indices_address {
        .deviceAddress = m_packed_index_buffer->GetBufferDeviceAddress(device)
    };

    m_geometry = VkAccelerationStructureGeometryKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    m_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	m_geometry.geometry = {
        .triangles = VkAccelerationStructureGeometryTrianglesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = vertices_address,
            .vertexStride = sizeof(PackedVertex),
            .maxVertex = static_cast<UInt32>(m_packed_vertices.size()),
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = indices_address,
            .transformData = { { } }
        }
    };

    HYPERION_RETURN_OK;
}

Result AccelerationGeometry::Destroy(Device *device)
{
	auto result = Result::OK;

	if (m_packed_vertex_buffer != nullptr) {
	    HYPERION_PASS_ERRORS(m_packed_vertex_buffer->Destroy(device), result);
		m_packed_vertex_buffer.reset();
	}

	if (m_packed_index_buffer != nullptr) {
	    HYPERION_PASS_ERRORS(m_packed_index_buffer->Destroy(device), result);
		m_packed_index_buffer.reset();
	}

	return result;
}

AccelerationStructure::AccelerationStructure()
    : m_acceleration_structure(VK_NULL_HANDLE),
      m_device_address(0),
      m_flags(ACCELERATION_STRUCTURE_FLAGS_NONE)
{
}

AccelerationStructure::~AccelerationStructure()
{
    AssertThrowMsg(
		m_acceleration_structure == VK_NULL_HANDLE,
		"Expected acceleration structure to have been destroyed before destructor call"
	);
}

Result AccelerationStructure::CreateAccelerationStructure(
    Instance *instance,
    AccelerationStructureType type,
    std::vector<VkAccelerationStructureGeometryKHR> &&geometries,
    std::vector<UInt32> &&primitive_counts,
	bool update,
	RTUpdateStateFlags &out_update_state_flags
)
{
    if (update) {
        AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

        // TEMP: we should have two TLASes and two RT descriptor sets I suppose...
        HYPERION_BUBBLE_ERRORS(instance->GetDevice()->Wait());
    } else {
        AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);
    }

	if (!instance->GetDevice()->GetFeatures().SupportsRaytracing()) {
	    return { Result::RENDERER_ERR, "Device does not support raytracing" };
	}

	if (geometries.empty()) {
	    return { Result::RENDERER_ERR, "Geometries empty" };
	}

    auto result = Result::OK;
    Device *device = instance->GetDevice();
	
	if (m_buffer == nullptr) {
        m_buffer = std::make_unique<AccelerationStructureBuffer>();
	}
	
	VkAccelerationStructureBuildGeometryInfoKHR geometry_info;
    geometry_info = VkAccelerationStructureBuildGeometryInfoKHR{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    geometry_info.type = ToVkAccelerationStructureType(type);
    geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    geometry_info.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometry_info.geometryCount = static_cast<UInt32>(geometries.size());
    geometry_info.pGeometries = geometries.data();

	AssertThrow(primitive_counts.size() == geometries.size());
	
	VkAccelerationStructureBuildSizesInfoKHR build_sizes_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	device->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
	    device->GetDevice(),
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&geometry_info,
		primitive_counts.data(),
		&build_sizes_info
	);

	const SizeType scratch_buffer_alignment = device->GetFeatures().GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
	SizeType acceleration_structure_size = MathUtil::NextMultiple(build_sizes_info.accelerationStructureSize, 256ull);
	SizeType build_scratch_size = MathUtil::NextMultiple(build_sizes_info.buildScratchSize, scratch_buffer_alignment);
	SizeType update_scratch_size = MathUtil::NextMultiple(build_sizes_info.updateScratchSize, scratch_buffer_alignment);

	bool was_rebuilt = false;

    HYPERION_BUBBLE_ERRORS(m_buffer->EnsureCapacity(
		device,
		acceleration_structure_size,
		&was_rebuilt
	));

	// force recreate (TEMP)
	was_rebuilt = true;

	if (was_rebuilt) {
	    out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

		if (update) {
		    // delete the current acceleration structure
		    device->GetFeatures().dyn_functions.vkDestroyAccelerationStructureKHR(
			    device->GetDevice(),
			    m_acceleration_structure,
			    nullptr
		    );

		    m_acceleration_structure = VK_NULL_HANDLE;

			// fetch the corrected acceleration structure and scratch buffer sizes
	        // update was true but we need to rebuild from scratch, have to unset the UPDATE flag. 
		    geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

		    device->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
	            device->GetDevice(),
		        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		        &geometry_info,
		        primitive_counts.data(),
		        &build_sizes_info
	        );

	        acceleration_structure_size = MathUtil::NextMultiple(build_sizes_info.accelerationStructureSize, 256ull);
	        build_scratch_size = MathUtil::NextMultiple(build_sizes_info.buildScratchSize, scratch_buffer_alignment);
	        update_scratch_size = MathUtil::NextMultiple(build_sizes_info.updateScratchSize, scratch_buffer_alignment);

		    AssertThrow(m_buffer->size >= acceleration_structure_size);
		}

		// to be sure it's zeroed out
		m_buffer->Memset(device, acceleration_structure_size, 0);

        VkAccelerationStructureCreateInfoKHR create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        create_info.buffer = m_buffer->buffer;
        create_info.size = acceleration_structure_size;
        create_info.type = ToVkAccelerationStructureType(type);

        HYPERION_VK_PASS_ERRORS(
            device->GetFeatures().dyn_functions.vkCreateAccelerationStructureKHR(
                device->GetDevice(),
		        &create_info,
		        VK_NULL_HANDLE,
		        &m_acceleration_structure
	        ),
		    result
	    );

	    if (!result) {
            HYPERION_IGNORE_ERRORS(Destroy(device));

            return result;
	    }
	}

	AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

    VkAccelerationStructureDeviceAddressInfoKHR address_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    address_info.accelerationStructure = m_acceleration_structure;

    m_device_address = device->GetFeatures().dyn_functions.vkGetAccelerationStructureDeviceAddressKHR(
        device->GetDevice(),
        &address_info
    );

	const SizeType scratch_size = (update && !was_rebuilt) ? update_scratch_size : build_scratch_size;

	if (m_scratch_buffer == nullptr) {
		m_scratch_buffer.reset(new ScratchBuffer);
		
	    HYPERION_PASS_ERRORS(
		    m_scratch_buffer->Create(device, scratch_size, scratch_buffer_alignment),
		    result
	    );
	} else {
	    HYPERION_PASS_ERRORS(
			m_scratch_buffer->EnsureCapacity(device, scratch_size, scratch_buffer_alignment),
			result
		);
	}

	// zero out scratch buffer
	m_scratch_buffer->Memset(device, m_scratch_buffer->size, 0);

    geometry_info.dstAccelerationStructure = m_acceleration_structure;
	geometry_info.srcAccelerationStructure = (update && !was_rebuilt) ? m_acceleration_structure : VK_NULL_HANDLE;
	geometry_info.scratchData = { .deviceAddress = m_scratch_buffer->GetBufferDeviceAddress(device) };
	
    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> range_infos;
    range_infos.resize(geometries.size());

    for (SizeType i = 0; i < geometries.size(); i++) {
        range_infos[i] = new VkAccelerationStructureBuildRangeInfoKHR {
            .primitiveCount = primitive_counts[i],
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };
    }

    auto commands = instance->GetSingleTimeCommands();

    commands.Push([&](CommandBuffer *command_buffer) {
        device->GetFeatures().dyn_functions.vkCmdBuildAccelerationStructuresKHR(
            command_buffer->GetCommandBuffer(),
            static_cast<UInt32>(range_infos.size()),
            &geometry_info,
            range_infos.data()
        );

        HYPERION_RETURN_OK; 
    });

    HYPERION_PASS_ERRORS(commands.Execute(device), result);

    for (auto *range_info : range_infos) {
        delete range_info;
    }

	ClearFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);

    HYPERION_RETURN_OK;
}

Result AccelerationStructure::Destroy(Device *device)
{
	AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

	auto result = Result::OK;

	for (auto &geometry : m_geometries) {
	    HYPERION_PASS_ERRORS(geometry->Destroy(device), result);
	}

	HYPERION_PASS_ERRORS(m_buffer->Destroy(device), result);

	if (m_instances_buffer != nullptr) {
	    HYPERION_PASS_ERRORS(m_instances_buffer->Destroy(device), result);
		m_instances_buffer.reset();
	}

	device->GetFeatures().dyn_functions.vkDestroyAccelerationStructureKHR(
		device->GetDevice(),
		m_acceleration_structure,
		VK_NULL_HANDLE
	);

	m_acceleration_structure = VK_NULL_HANDLE;

	return result;
}

void AccelerationStructure::RemoveGeometry(AccelerationGeometry *geometry)
{
	if (geometry == nullptr) {
	    return;
	}

	auto it = std::find_if(
		m_geometries.begin(),
		m_geometries.end(),
		[geometry](const auto &other) {
		    return other.get() == geometry;
		}
	);

	if (it == m_geometries.end()) {
		return;
	}

	/* Todo: Destroy() ? */

	m_geometries.erase(it);

	SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure()
    : AccelerationStructure()
{
}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
	AssertThrowMsg(m_acceleration_structure == VK_NULL_HANDLE, "Acceleration structure should have been destroyed before destructor call");
}

std::vector<VkAccelerationStructureGeometryKHR> TopLevelAccelerationStructure::GetGeometries(Instance *instance) const
{
    return {
	    {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
				.instances = {
    		        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
    		        .arrayOfPointers = VK_FALSE,
                    .data = { .deviceAddress = m_instances_buffer->GetBufferDeviceAddress(instance->GetDevice()) }
				}
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        }
    };
}

std::vector<UInt32> TopLevelAccelerationStructure::GetPrimitiveCounts() const
{
    return { static_cast<UInt32>(m_blas.size()) };
}
    
Result TopLevelAccelerationStructure::Create(
    Device *device,
    Instance *instance,
    std::vector<BottomLevelAccelerationStructure *> &&blas
)
{
    AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);
    AssertThrow(m_instances_buffer == nullptr);

    auto result = Result::OK;

    m_blas = std::move(blas);

    if (m_blas.empty()) {
        // no BLASes in here to create any buffers for
        AssertThrowMsg(false, "Cannot create TLAS without any BLASes. This will be fixed, for now it is not valid");
    }

	// check each BLAS, assert that it is valid.
	for (const auto &blas : m_blas) {
		AssertThrow(blas != nullptr);
		AssertThrow(!blas->GetGeometries().empty());

		for (const auto &geometry : blas->GetGeometries()) {
		    AssertThrow(geometry->GetPackedVertexStorageBuffer() != nullptr);
		    AssertThrow(geometry->GetPackedIndexStorageBuffer() != nullptr);
		}
	}

    HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer(instance));

    RTUpdateStateFlags update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;
    
    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            instance,
            GetType(),
            GetGeometries(instance),
            GetPrimitiveCounts(),
            false,
            update_state_flags
        ),
        result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

	AssertThrow(update_state_flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

	HYPERION_PASS_ERRORS(
		CreateMeshDescriptionsBuffer(
		    instance
	    ),
		result
	);

	update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

	return result;
}

Result TopLevelAccelerationStructure::Destroy(Device *device)
{
	auto result = Result::OK;

	if (m_mesh_descriptions_buffer != nullptr) {
	    HYPERION_PASS_ERRORS(m_mesh_descriptions_buffer->Destroy(device), result);
		m_mesh_descriptions_buffer.reset();
	}

	if (m_scratch_buffer != nullptr) {
	    HYPERION_PASS_ERRORS(m_scratch_buffer->Destroy(device), result);
		m_scratch_buffer.reset();
	}

	/*for (auto &blas : m_blas) {
	    if (blas == nullptr) {
	        continue;
	    }

		HYPERION_PASS_ERRORS(
			blas->Destroy(device),
			result
		);
	}*/

    HYPERION_PASS_ERRORS(
		AccelerationStructure::Destroy(device),
		result
	);

	return result;
}

void TopLevelAccelerationStructure::AddBLAS(BottomLevelAccelerationStructure *blas)
{
	AssertThrow(blas != nullptr);
	AssertThrow(!blas->GetGeometries().empty());

	for (const auto &geometry : blas->GetGeometries()) {
	    AssertThrow(geometry != nullptr);
		AssertThrow(geometry->GetPackedVertexStorageBuffer() != nullptr);
		AssertThrow(geometry->GetPackedIndexStorageBuffer() != nullptr);
	}

	m_blas.push_back(blas);

    SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
}

void TopLevelAccelerationStructure::RemoveBLAS(BottomLevelAccelerationStructure *blas)
{
    auto it = std::find(m_blas.begin(), m_blas.end(), blas);

	if (it != m_blas.end()) {
	    m_blas.erase(it);

		SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
	}
}

Result TopLevelAccelerationStructure::CreateOrRebuildInstancesBuffer(Instance *instance)
{
    Device *device = instance->GetDevice();

    std::vector<VkAccelerationStructureInstanceKHR> instances(m_blas.size());

    for (SizeType i = 0; i < m_blas.size(); i++) {
        const BottomLevelAccelerationStructure *blas = m_blas[i];
        AssertThrow(blas != nullptr);

        const auto instance_index = static_cast<UInt32>(i); /* Index of mesh in mesh descriptions buffer. */

        instances[i] = VkAccelerationStructureInstanceKHR {
            .transform = ToVkTransform(blas->GetTransform()),
            .instanceCustomIndex = instance_index,
            .mask = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR,
            .accelerationStructureReference = blas->GetDeviceAddress()
        };
    }

	const SizeType instances_buffer_size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    if (m_instances_buffer == nullptr) {
        m_instances_buffer = std::make_unique<AccelerationStructureInstancesBuffer>();

        HYPERION_BUBBLE_ERRORS(m_instances_buffer->Create(device, instances_buffer_size));
    } else if (m_instances_buffer->size != instances_buffer_size) {
        DebugLog(
            LogType::Debug,
            "Resizing instances buffer from %llu to %llu\n",
            m_instances_buffer->size,
            instances_buffer_size
        );

        HYPERION_BUBBLE_ERRORS(m_instances_buffer->Destroy(device));
        HYPERION_BUBBLE_ERRORS(m_instances_buffer->Create(device, instances_buffer_size));
    }

    m_instances_buffer->Copy(
        device,
        instances_buffer_size,
        instances.data()
    );

    HYPERION_RETURN_OK;
}

Result TopLevelAccelerationStructure::UpdateInstancesBuffer(Instance *instance)
{
    Device *device = instance->GetDevice();

    std::vector<VkAccelerationStructureInstanceKHR> instances(m_blas.size());

    for (SizeType i = 0; i < m_blas.size(); i++) {
        const BottomLevelAccelerationStructure *blas = m_blas[i];
        AssertThrow(blas != nullptr);

        const auto instance_index = static_cast<UInt32>(i); /* Index of mesh in mesh descriptions buffer. */

        instances[i] = VkAccelerationStructureInstanceKHR {
            .transform = ToVkTransform(blas->GetTransform()),
            .instanceCustomIndex = instance_index,
            .mask = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blas->GetDeviceAddress()
        };
    }

    const SizeType instances_buffer_size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    AssertThrow(m_instances_buffer != nullptr);
    AssertThrow(m_instances_buffer->size == instances_buffer_size);

    m_instances_buffer->Copy(
        device,
        instances_buffer_size,
        instances.data()
    );

    HYPERION_RETURN_OK;
}

Result TopLevelAccelerationStructure::CreateMeshDescriptionsBuffer(Instance *instance)
{
	AssertThrow(m_mesh_descriptions_buffer == nullptr);

	Device *device = instance->GetDevice();

	m_mesh_descriptions_buffer = std::make_unique<StorageBuffer>();

	HYPERION_BUBBLE_ERRORS(m_mesh_descriptions_buffer->Create(
		device,
		sizeof(MeshDescription) * m_blas.size()
	));

	// zero out buffer
	m_mesh_descriptions_buffer->Memset(device, m_mesh_descriptions_buffer->size, 0);

    return UpdateMeshDescriptionsBuffer(instance);
}

Result TopLevelAccelerationStructure::UpdateMeshDescriptionsBuffer(Instance *instance)
{
	return UpdateMeshDescriptionsBuffer(instance, 0u, static_cast<UInt>(m_blas.size()));
}

Result TopLevelAccelerationStructure::UpdateMeshDescriptionsBuffer(Instance *instance, UInt first, UInt last)
{
	AssertThrow(m_mesh_descriptions_buffer != nullptr);
	AssertThrow(m_mesh_descriptions_buffer->size >= sizeof(MeshDescription) * m_blas.size());
	AssertThrow(first < m_blas.size());
	AssertThrow(last <= m_blas.size());

	if (last <= first) {
		// nothing to update
		return Result::OK;
	}

	Device *device = instance->GetDevice();

	std::vector<MeshDescription> mesh_descriptions(last - first);

	for (UInt i = first; i < last; i++) {
	    const BottomLevelAccelerationStructure *blas = m_blas[i];

		MeshDescription &mesh_description = mesh_descriptions[i - first];
		Memory::Set(&mesh_description, 0, sizeof(MeshDescription));

		if (blas->GetGeometries().empty()) {
			AssertThrowMsg(
				false,
				"No geometries added to BLAS node %u!\n",
				i
			);

			mesh_description = { };
		} else {
		    mesh_description.vertex_buffer_address = blas->GetGeometries()[0]->GetPackedVertexStorageBuffer()->GetBufferDeviceAddress(device);
		    mesh_description.index_buffer_address = blas->GetGeometries()[0]->GetPackedIndexStorageBuffer()->GetBufferDeviceAddress(device);
			mesh_description.material_index = blas->GetGeometries()[0]->GetMaterialIndex();
			mesh_description.num_indices = static_cast<UInt32>(blas->GetGeometries()[0]->GetPackedIndices().size());
			mesh_description.num_vertices = static_cast<UInt32>(blas->GetGeometries()[0]->GetPackedVertices().size());
		}
	}

	m_mesh_descriptions_buffer->Copy(
		device,
		sizeof(MeshDescription) * first,
		sizeof(MeshDescription) * mesh_descriptions.size(),
		mesh_descriptions.data()
	);

	return Result::OK;
}

Result TopLevelAccelerationStructure::RebuildMeshDescriptionsBuffer(Instance *instance)
{
    if (m_mesh_descriptions_buffer != nullptr) {
		HYPERION_BUBBLE_ERRORS(m_mesh_descriptions_buffer->Destroy(instance->GetDevice()));
		m_mesh_descriptions_buffer.reset();
    }

	return CreateMeshDescriptionsBuffer(instance);
}

Result TopLevelAccelerationStructure::UpdateStructure(Instance *instance, RTUpdateStateFlags &out_update_state_flags)
{
	out_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

	//return Rebuild(instance, out_update_state_flags); // temp

	if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING) {
	    return Rebuild(instance, out_update_state_flags);
	}

	Range<UInt> dirty_mesh_descriptions { };

	for (UInt i = 0; i < static_cast<UInt>(m_blas.size()); i++) {
		auto *blas = m_blas[i];
		AssertThrow(blas != nullptr);

		if (blas->GetFlags() & ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE) {
			dirty_mesh_descriptions |= Range { i, i + 1 };

		    blas->ClearFlag(ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
		}

		RTUpdateStateFlags blas_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;
		HYPERION_BUBBLE_ERRORS(blas->UpdateStructure(instance, blas_update_state_flags));

		if (blas_update_state_flags) {
		    dirty_mesh_descriptions |= Range { i, i + 1 };
		}
	}

    if (dirty_mesh_descriptions) {
		out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

        // update data in instances buffer
        HYPERION_BUBBLE_ERRORS(UpdateInstancesBuffer(instance));

        // copy mesh descriptions
        HYPERION_BUBBLE_ERRORS(UpdateMeshDescriptionsBuffer(
            instance,
            dirty_mesh_descriptions.GetStart(),
            dirty_mesh_descriptions.GetEnd()
        ));

		
        //HYPERION_BUBBLE_ERRORS(RebuildMeshDescriptionsBuffer(instance));
		
	    HYPERION_BUBBLE_ERRORS(CreateAccelerationStructure(
		    instance,
		    GetType(),
		    GetGeometries(instance),
			GetPrimitiveCounts(),
			true,
			out_update_state_flags
	    ));
	}

	HYPERION_RETURN_OK;
}

Result TopLevelAccelerationStructure::Rebuild(Instance *instance, RTUpdateStateFlags &out_update_state_flags)
{
	AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

    auto result = Result::OK;

	Device *device = instance->GetDevice();

	// check each BLAS, assert that it is valid.
	for (const auto &blas : m_blas) {
		AssertThrow(blas != nullptr);
		AssertThrow(!blas->GetGeometries().empty());

		for (const auto &geometry : blas->GetGeometries()) {
			AssertThrow(geometry != nullptr);
		    AssertThrow(geometry->GetPackedVertexStorageBuffer() != nullptr);
		    AssertThrow(geometry->GetPackedIndexStorageBuffer() != nullptr);
		}
	}

	HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer(instance));

	HYPERION_PASS_ERRORS(
		CreateAccelerationStructure(
		    instance,
		    GetType(),
		    GetGeometries(instance),
			GetPrimitiveCounts(),
			true,
			out_update_state_flags
	    ),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

	HYPERION_PASS_ERRORS(
		RebuildMeshDescriptionsBuffer(
		    instance
	    ),
		result
	);

	out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

	return result;
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure()
    : AccelerationStructure()
{
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure() = default;

Result BottomLevelAccelerationStructure::Create(Device *device, Instance *instance)
{
	auto result = Result::OK;

	std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.size());
	std::vector<UInt32> primitive_counts(m_geometries.size());

	if (m_geometries.empty()) {
	    return { Result::RENDERER_ERR, "Cannot create BLAS with zero geometries" };
	}

	for (SizeType i = 0; i < m_geometries.size(); i++) {
	    const auto &geometry = m_geometries[i];

		if (geometry->GetPackedIndices().size() % 3 != 0) {
		    return { Result::RENDERER_ERR, "Invalid triangle mesh!" };
		}

		HYPERION_PASS_ERRORS(geometry->Create(device, instance), result);

		if (!result) {
			HYPERION_IGNORE_ERRORS(Destroy(device));
		    return result;
		}

		geometries[i] = geometry->m_geometry;
		primitive_counts[i] = static_cast<UInt32>(geometry->GetPackedIndices().size() / 3);

	    if (primitive_counts[i] == 0) {
	        return { Result::RENDERER_ERR, "Cannot create BLAS -- geometry has zero indices" };
	    }
	}

	RTUpdateStateFlags update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

	HYPERION_PASS_ERRORS(
		CreateAccelerationStructure(
		    instance,
		    GetType(),
		    std::move(geometries),
		    std::move(primitive_counts),
			false,
			update_state_flags
	    ),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

	AssertThrow(update_state_flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

	return result;
}

Result BottomLevelAccelerationStructure::UpdateStructure(Instance *instance, RTUpdateStateFlags &out_update_state_flags)
{
	out_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

	if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING) {
	    return Rebuild(instance, out_update_state_flags);
	}

	HYPERION_RETURN_OK;
}

Result BottomLevelAccelerationStructure::Rebuild(Instance *instance, RTUpdateStateFlags &out_update_state_flags)
{
	auto result = Result::OK;
	Device *device = instance->GetDevice();

	std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.size());
	std::vector<UInt32> primitive_counts(m_geometries.size());

	for (SizeType i = 0; i < m_geometries.size(); i++) {
	    const auto &geometry = m_geometries[i];
		AssertThrow(geometry != nullptr);

#if 0
		if (geometry->GetPackedIndices().size() % 3 != 0) {
		    return { Result::RENDERER_ERR, "Invalid triangle mesh!" };
		}

		HYPERION_BUBBLE_ERRORS(geometry->Destroy(device));
		HYPERION_BUBBLE_ERRORS(geometry->Create(device, instance));
#endif

		geometries[i] = geometry->m_geometry;
		primitive_counts[i] = static_cast<UInt32>(geometry->GetPackedIndices().size() / 3);
	}

	HYPERION_PASS_ERRORS(
		CreateAccelerationStructure(
		    instance,
		    GetType(),
		    std::move(geometries),
		    std::move(primitive_counts),
			true,
			out_update_state_flags
	    ),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

	return result;
}

} // namespace renderer
} // namespace hyperion
