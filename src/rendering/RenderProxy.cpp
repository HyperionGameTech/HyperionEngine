#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderSkeleton.hpp>

#include <scene/Entity.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

extern HYP_API SafeDeleter* g_safe_deleter;

void MeshInstanceData_PostLoad(MeshInstanceData& mesh_instance_data)
{
    // Ensure at least one instance
    mesh_instance_data.num_instances = MathUtil::Max(mesh_instance_data.num_instances, 1u);

    if (mesh_instance_data.buffers.Any())
    {
        for (uint32 buffer_index = 0; buffer_index < MathUtil::Min(MeshInstanceData::max_buffers, uint32(mesh_instance_data.buffers.Size())); buffer_index++)
        {
            if (mesh_instance_data.buffers[buffer_index].Size() / mesh_instance_data.buffer_struct_sizes[buffer_index] != mesh_instance_data.num_instances)
            {
                HYP_LOG(Rendering, Warning, "Expected mesh instance data to have a buffer size that is equal to (buffer struct size / number of instances). Buffer size: {}, Buffer struct size: {}, Num instances: {}",
                    mesh_instance_data.buffers[buffer_index].Size(), mesh_instance_data.buffer_struct_sizes[buffer_index],
                    mesh_instance_data.num_instances);
            }
        }

        if (mesh_instance_data.buffers.Size() > MeshInstanceData::max_buffers)
        {
            HYP_LOG(Rendering, Warning, "MeshInstanceData has more buffers than the maximum allowed: {} > {}", mesh_instance_data.buffers.Size(), MeshInstanceData::max_buffers);
        }
    }
}

#pragma region RenderProxy

void RenderProxy::IncRefs() const
{
    if (material.IsValid())
    {
        material->GetRenderResource().IncRef();
    }

    if (mesh.IsValid())
    {
        mesh->GetRenderResource().IncRef();
    }

    if (skeleton.IsValid())
    {
        skeleton->GetRenderResource().IncRef();
    }
}

void RenderProxy::DecRefs() const
{
    if (material.IsValid())
    {
        material->GetRenderResource().DecRef();
    }

    if (mesh.IsValid())
    {
        mesh->GetRenderResource().DecRef();
    }

    if (skeleton.IsValid())
    {
        skeleton->GetRenderResource().DecRef();
    }
}

#pragma endregion RenderProxy

} // namespace hyperion