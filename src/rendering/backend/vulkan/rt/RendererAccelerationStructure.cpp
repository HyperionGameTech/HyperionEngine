#include <rendering/backend/rt/RendererAccelerationStructure.hpp>
#include "../RendererDevice.hpp"
#include "../RendererInstance.hpp"
#include "../RendererFeatures.hpp"

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
    std::vector<PackedIndex>  &&packed_indices
) : m_packed_vertices(std::move(packed_vertices)),
    m_packed_indices(std::move(packed_indices)),
    m_geometry{}
{
}

AccelerationGeometry::~AccelerationGeometry()
{
}

Result AccelerationGeometry::Create(Device *device, Instance *instance)
{
	AssertThrow(m_packed_vertex_buffer == nullptr);
	AssertThrow(m_packed_index_buffer == nullptr);

	if (m_packed_vertices.empty() || m_packed_indices.empty()) {
	    return {Result::RENDERER_ERR, "An acceleration geometry must have nonzero vertex count, index count, and vertex size."};
	}

	if (!device->GetFeatures().SupportsRaytracing()) {
	    return {Result::RENDERER_ERR, "Device does not support raytracing"};
	}

	auto result = Result::OK;

	m_packed_vertex_buffer = std::make_unique<PackedVertexStorageBuffer>();

	HYPERION_BUBBLE_ERRORS(
		m_packed_vertex_buffer->Create(device, m_packed_vertices.size() * sizeof(PackedVertex))
	);

	m_packed_index_buffer = std::make_unique<PackedIndexStorageBuffer>();

	HYPERION_PASS_ERRORS(
		m_packed_index_buffer->Create(device, m_packed_indices.size() * sizeof(PackedIndex)),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

	HYPERION_PASS_ERRORS(
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
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

    VkDeviceOrHostAddressConstKHR vertices_address{
        .deviceAddress = m_packed_vertex_buffer->GetBufferDeviceAddress(device)
    };

	VkDeviceOrHostAddressConstKHR indices_address{
        .deviceAddress = m_packed_index_buffer->GetBufferDeviceAddress(device)
	};

	m_geometry = VkAccelerationStructureGeometryKHR{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	m_geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
	m_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	m_geometry.geometry = {
		.triangles = VkAccelerationStructureGeometryTrianglesDataKHR{
			.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
			.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
			.vertexData    = vertices_address,
			.vertexStride  = sizeof(PackedVertex),
			.maxVertex     = static_cast<uint32_t>(m_packed_vertices.size()),
			.indexType     = VK_INDEX_TYPE_UINT32,
			.indexData     = indices_address,
			.transformData = {{}}
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
    std::vector<uint32_t> &&primitive_counts,
	bool update)
{
	if (update) {
		AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);
	} else {
        AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);
	}

	if (!instance->GetDevice()->GetFeatures().SupportsRaytracing()) {
	    return {Result::RENDERER_ERR, "Device does not support raytracing"};
	}

    auto result = Result::OK;
    Device *device = instance->GetDevice();
	
	VkAccelerationStructureBuildGeometryInfoKHR geometry_info;
    geometry_info = VkAccelerationStructureBuildGeometryInfoKHR{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    geometry_info.type                     = ToVkAccelerationStructureType(type);
    geometry_info.flags                    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    geometry_info.mode                     = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometry_info.geometryCount            = static_cast<uint32_t>(geometries.size());
    geometry_info.pGeometries              = geometries.data();

	VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};

	device->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
	    device->GetDevice(),
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&geometry_info,
		primitive_counts.data(),
		&build_sizes_info
	);

	if (m_buffer == nullptr) {
        m_buffer = std::make_unique<AccelerationStructureBuffer>();
	}

	bool is_newly_created = false;

    HYPERION_BUBBLE_ERRORS(m_buffer->EnsureCapacity(
		device,
		build_sizes_info.accelerationStructureSize,
		&is_newly_created
	));

	if (is_newly_created) {
		if (update) {
		    /* update was true but we need to rebuild from scratch, have to unset the UPDATE flag. */
			geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		}

		if (m_acceleration_structure != VK_NULL_HANDLE) {
		    device->GetFeatures().dyn_functions.vkDestroyAccelerationStructureKHR(
				device->GetDevice(),
				m_acceleration_structure,
				nullptr
			);

			m_acceleration_structure = VK_NULL_HANDLE;
		}

        VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.buffer = m_buffer->buffer;
        create_info.size   = build_sizes_info.accelerationStructureSize;
        create_info.type   = ToVkAccelerationStructureType(type);

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

    VkAccelerationStructureDeviceAddressInfoKHR address_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    address_info.accelerationStructure = m_acceleration_structure;

    m_device_address = device->GetFeatures().dyn_functions.vkGetAccelerationStructureDeviceAddressKHR(
        device->GetDevice(),
        &address_info
    );
	
	auto scratch_buffer = std::make_unique<ScratchBuffer>();
	HYPERION_PASS_ERRORS(scratch_buffer->Create(device, build_sizes_info.buildScratchSize), result);

	if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

    geometry_info.dstAccelerationStructure = m_acceleration_structure;
	geometry_info.srcAccelerationStructure = is_newly_created ? VK_NULL_HANDLE : m_acceleration_structure;
	geometry_info.scratchData              = { .deviceAddress = scratch_buffer->GetBufferDeviceAddress(device) };

    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> range_infos;
    range_infos.resize(geometries.size());

    for (size_t i = 0; i < geometries.size(); i++) {
        range_infos[i] = new VkAccelerationStructureBuildRangeInfoKHR{
            .primitiveCount  = primitive_counts[i],
            .primitiveOffset = 0,
            .firstVertex     = 0,
            .transformOffset = 0
        };
    }

    auto commands = instance->GetSingleTimeCommands();

    commands.Push([&](CommandBuffer *command_buffer) {
        device->GetFeatures().dyn_functions.vkCmdBuildAccelerationStructuresKHR(
            command_buffer->GetCommandBuffer(),
            static_cast<uint32_t>(range_infos.size()),
            &geometry_info,
            range_infos.data()
        );

        HYPERION_RETURN_OK; 
    });

    HYPERION_PASS_ERRORS(commands.Execute(device), result);

    for (auto *range_info : range_infos) {
        delete range_info;
    }

    if (result) {
		HYPERION_PASS_ERRORS(scratch_buffer->Destroy(device), result);
    } else {
		HYPERION_IGNORE_ERRORS(scratch_buffer->Destroy(device));
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
	m_flags = AccelerationStructureFlags(m_flags | ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
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
	        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
			.geometry = {
				.instances = {
			        .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
			        .arrayOfPointers = VK_FALSE,
			        .data            = {.deviceAddress = m_instances_buffer->GetBufferDeviceAddress(instance->GetDevice())}
		        }
			},
			.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
	    }
	};
}

std::vector<uint32_t> TopLevelAccelerationStructure::GetPrimitiveCounts() const
{
    return {static_cast<uint32_t>(m_blas.size())};
}
    
Result TopLevelAccelerationStructure::Create(
	Device *device,
	Instance *instance,
	std::vector<BottomLevelAccelerationStructure *> &&blas
)
{
	AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);
	AssertThrow(m_instances_buffer == nullptr);

	m_blas = std::move(blas);

    auto result = Result::OK;

	HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer(instance));

	HYPERION_PASS_ERRORS(
		CreateAccelerationStructure(
		    instance,
		    GetType(),
		    GetGeometries(instance),
			GetPrimitiveCounts()
	    ),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

	HYPERION_PASS_ERRORS(
		CreateMeshDescriptionsBuffer(
		    instance
	    ),
		result
	);

	return result;
}

Result TopLevelAccelerationStructure::Destroy(Device *device)
{
	auto result = Result::OK;

	if (m_mesh_descriptions_buffer != nullptr) {
	    HYPERION_PASS_ERRORS(m_mesh_descriptions_buffer->Destroy(device), result);
		m_mesh_descriptions_buffer.reset();
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

Result TopLevelAccelerationStructure::CreateOrRebuildInstancesBuffer(Instance *instance)
{
	Device *device = instance->GetDevice();

	std::vector<VkAccelerationStructureInstanceKHR> instances(m_blas.size());

	for (size_t i = 0; i < m_blas.size(); i++) {
		const BottomLevelAccelerationStructure *blas = m_blas[i];

		const auto instance_index = static_cast<uint32_t>(i); /* Index of mesh in mesh descriptions buffer. */

	    instances[i] = VkAccelerationStructureInstanceKHR{
		    .transform                              = ToVkTransform(blas->GetTransform()),
		    .instanceCustomIndex                    = instance_index,
		    .mask                                   = 0xff,
		    .instanceShaderBindingTableRecordOffset = 0,
		    .flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
		    .accelerationStructureReference         = blas->GetDeviceAddress()
	    };
	}

	const size_t instances_buffer_size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

	if (m_instances_buffer == nullptr) {
	    m_instances_buffer = std::make_unique<AccelerationStructureInstancesBuffer>();

	    HYPERION_BUBBLE_ERRORS(m_instances_buffer->Create(device, instances_buffer_size));
	} else {
	    if (m_instances_buffer->size != instances_buffer_size) {
	        DebugLog(
				LogType::Debug,
				"Resizing instances buffer from %llu to %llu\n",
				m_instances_buffer->size,
				instances_buffer_size
			);

		    HYPERION_BUBBLE_ERRORS(m_instances_buffer->Destroy(device));
		    HYPERION_BUBBLE_ERRORS(m_instances_buffer->Create(device, instances_buffer_size));
		}
	}

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

	std::vector<MeshDescription> mesh_descriptions(m_blas.size());

	m_mesh_descriptions_buffer = std::make_unique<StorageBuffer>();
	HYPERION_BUBBLE_ERRORS(m_mesh_descriptions_buffer->Create(
		device,
		sizeof(MeshDescription) * mesh_descriptions.size()
	));

	for (size_t i = 0; i < mesh_descriptions.size(); i++) {
	    const BottomLevelAccelerationStructure *blas = m_blas[i];

		uint64_t vertex_buffer_address = 0,
		         index_buffer_address = 0;

		if (blas->GetGeometries().empty()) {
			DebugLog(
				LogType::Warn,
				"No geometries added to BLAS node %lu. Setting vertex buffer address and index buffer address to zero.\n",
				i
			);
		} else {
		    vertex_buffer_address = blas->GetGeometries()[0]->GetPackedVertexStorageBuffer()->GetBufferDeviceAddress(device);
		    index_buffer_address  = blas->GetGeometries()[0]->GetPackedIndexStorageBuffer()->GetBufferDeviceAddress(device);
		}

		mesh_descriptions[i] = MeshDescription{
			.vertex_buffer_address = vertex_buffer_address,
			.index_buffer_address  = index_buffer_address
		};
	}

	m_mesh_descriptions_buffer->Copy(
		device,
		sizeof(MeshDescription) * mesh_descriptions.size(),
		mesh_descriptions.data()
	);

    HYPERION_RETURN_OK;
}

Result TopLevelAccelerationStructure::RebuildMeshDescriptionsBuffer(Instance *instance)
{
    if (m_mesh_descriptions_buffer != nullptr) {
		HYPERION_BUBBLE_ERRORS(m_mesh_descriptions_buffer->Destroy(instance->GetDevice()));
		m_mesh_descriptions_buffer.reset();
    }

	return CreateMeshDescriptionsBuffer(instance);
}

Result TopLevelAccelerationStructure::UpdateStructure(Instance *instance)
{
	if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING) {
	    return Rebuild(instance);
	}

	Range<UInt> dirty_mesh_descriptions { };

	for (UInt i = 0; i < static_cast<UInt>(m_blas.size()); i++) {
		auto &blas = m_blas[i];
		AssertThrow(blas != nullptr);

		if (blas->GetFlags() & AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE) {
			dirty_mesh_descriptions |= Range { i, i + 1 };
		}

		HYPERION_BUBBLE_ERRORS(blas->UpdateStructure(instance));

		blas->ClearFlag(AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
	}

    if (dirty_mesh_descriptions) {
		// copy mesh descriptions
		return UpdateMeshDescriptionsBuffer(
			instance,
			dirty_mesh_descriptions.GetStart(),
			dirty_mesh_descriptions.GetEnd()
		);
	}

	HYPERION_RETURN_OK;
}

Result TopLevelAccelerationStructure::Rebuild(Instance *instance)
{
	AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

    auto result = Result::OK;

	Device *device = instance->GetDevice();

	HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer(instance));

	HYPERION_PASS_ERRORS(
		CreateAccelerationStructure(
		    instance,
		    GetType(),
		    GetGeometries(instance),
			GetPrimitiveCounts(),
			true
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
	std::vector<uint32_t> primitive_counts(m_geometries.size());

	for (size_t i = 0; i < m_geometries.size(); i++) {
	    const auto &geometry = m_geometries[i];

		HYPERION_PASS_ERRORS(geometry->Create(device, instance), result);

		if (!result) {
		    return result;
		}

		geometries[i]       = geometry->m_geometry;
		primitive_counts[i] = static_cast<uint32_t>(geometry->GetPackedIndices().size() / 3);
	}

	HYPERION_PASS_ERRORS(
		CreateAccelerationStructure(
		    instance,
		    GetType(),
		    std::move(geometries),
		    std::move(primitive_counts)
	    ),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(device));

		return result;
	}

	return result;
}

Result BottomLevelAccelerationStructure::UpdateStructure(Instance *instance)
{
	if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING) {
	    return Rebuild(instance);
	}

	HYPERION_RETURN_OK;
}

Result BottomLevelAccelerationStructure::Rebuild(Instance *instance)
{
	auto result = Result::OK;
	Device *device = instance->GetDevice();

	std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.size());
	std::vector<uint32_t> primitive_counts(m_geometries.size());

	for (size_t i = 0; i < m_geometries.size(); i++) {
	    const auto &geometry = m_geometries[i];

		HYPERION_PASS_ERRORS(geometry->Create(device, instance), result);

		if (!result) {
		    return result;
		}

		geometries[i]       = geometry->m_geometry;
		primitive_counts[i] = static_cast<uint32_t>(geometry->GetPackedIndices().size() / 3);
	}

	HYPERION_PASS_ERRORS(
		CreateAccelerationStructure(
		    instance,
		    GetType(),
		    std::move(geometries),
		    std::move(primitive_counts),
			true
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
