#include <rendering/MeshInstanceData.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

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

} // namespace hyperion