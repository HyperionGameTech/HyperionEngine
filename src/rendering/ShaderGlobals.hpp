#ifndef HYPERION_V2_SHADER_GLOBALS_H
#define HYPERION_V2_SHADER_GLOBALS_H

#include "Buffers.hpp"
#include "Bindless.hpp"
#include "DrawProxy.hpp"

#include <core/lib/Queue.hpp>
#include <core/ID.hpp>

#include <math/MathUtil.hpp>

#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <memory>
#include <utility>
#include <string>

namespace hyperion::v2 {

using renderer::ShaderProgram;
using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::IndirectBuffer;

class Engine;
class Entity;

using EntityBatchIndex = UInt;

struct ShaderGlobals
{
    ShaderGlobals(SizeType num_buffers)
        : scenes(num_buffers),
          lights(num_buffers),
          objects(num_buffers),
          materials(num_buffers),
          skeletons(num_buffers),
          shadow_maps(num_buffers),
          env_probes(num_buffers),
          immediate_draws(num_buffers),
          entity_instance_batches(num_buffers)
    {
    }

    ShaderGlobals(const ShaderGlobals &other) = delete;
    ShaderGlobals &operator=(const ShaderGlobals &other) = delete;

    void Create();
    void Destroy();

    ShaderData<StorageBuffer, SceneShaderData, max_scenes> scenes;
    ShaderData<StorageBuffer, LightShaderData, max_lights> lights;
    ShaderData<StorageBuffer, ObjectShaderData, max_entities> objects;
    ShaderData<StorageBuffer, MaterialShaderData, max_materials> materials;
    ShaderData<StorageBuffer, SkeletonShaderData, max_skeletons> skeletons;
    ShaderData<StorageBuffer, ShadowShaderData, max_shadow_maps> shadow_maps;
    ShaderData<StorageBuffer, EnvProbeShaderData, max_env_probes> env_probes;
    ShaderData<StorageBuffer, ImmediateDrawShaderData, max_immediate_draws> immediate_draws;
    ShaderData<StorageBuffer, EntityInstanceBatch, max_entity_instance_batches> entity_instance_batches;
    BindlessStorage textures;

    UniformBuffer cubemap_uniforms;

    struct EntityInstanceBatchData
    {
        EntityBatchIndex current_index = 1; // reserve first index (0)
        Queue<EntityBatchIndex> free_indices;
    } entity_instance_batch_data;

    EntityBatchIndex NewEntityBatch()
    {
        if (entity_instance_batch_data.free_indices.Any()) {
            return entity_instance_batch_data.free_indices.Pop();
        }

        return entity_instance_batch_data.current_index++;
    }

    void FreeEntityBatch(EntityBatchIndex batch_index)
    {
        if (batch_index == 0) {
            return;
        }

        ResetBatch(batch_index);

        entity_instance_batch_data.free_indices.Push(batch_index);
    }

    void ResetBatch(EntityBatchIndex batch_index)
    {
        if (batch_index == 0) {
            return;
        }

        AssertThrow(batch_index < max_entity_instance_batches);

        EntityInstanceBatch &batch = entity_instance_batches.Get(batch_index);
        Memory::Set(&batch, 0, sizeof(EntityInstanceBatch));

        entity_instance_batches.MarkDirty(batch_index);
    }

    bool PushEntityToBatch(EntityBatchIndex batch_index, ID<Entity> entity_id)
    {
        AssertThrow(batch_index < max_entity_instance_batches);

        EntityInstanceBatch &batch = entity_instance_batches.Get(batch_index);

        if (batch.num_entities >= max_entities_per_instance_batch) {
            return false;
        }

        const UInt32 id_index = batch.num_entities++;
        batch.indices[id_index] = UInt32(entity_id.ToIndex());
        entity_instance_batches.MarkDirty(batch_index);

        return true;
    }

    void SetEntityInBatch(EntityBatchIndex batch_index, SizeType id_index, ID<Entity> entity_id)
    {
        AssertThrow(batch_index < max_entity_instance_batches);
        AssertThrow(id_index < max_entities_per_instance_batch);

        EntityInstanceBatch &batch = entity_instance_batches.Get(batch_index);
        batch.num_entities = MathUtil::Max(batch.num_entities, UInt32(id_index + 1));
        batch.indices[id_index] = UInt32(entity_id.ToIndex());

        entity_instance_batches.MarkDirty(batch_index);
    }
};

} // namespace hyperion::v2

#endif