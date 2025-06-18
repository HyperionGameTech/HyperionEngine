/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DrawCall.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderProxy.hpp>

#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderCollection);

extern RenderGlobalState* g_render_global_state;

HYP_API GPUBufferHolderMap* GetGPUBufferHolderMap()
{
    return g_render_global_state->GPUBufferHolderMap.Get();
}

#pragma region DrawCallCollection

DrawCallCollection::DrawCallCollection(DrawCallCollection&& other) noexcept
    : m_impl(other.m_impl),
      m_draw_calls(std::move(other.m_draw_calls)),
      m_instanced_draw_calls(std::move(other.m_instanced_draw_calls)),
      m_index_map(std::move(other.m_index_map))
{
}

DrawCallCollection& DrawCallCollection::operator=(DrawCallCollection&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    ResetDrawCalls();

    m_impl = other.m_impl;
    m_draw_calls = std::move(other.m_draw_calls);
    m_instanced_draw_calls = std::move(other.m_instanced_draw_calls);
    m_index_map = std::move(other.m_index_map);

    return *this;
}

DrawCallCollection::~DrawCallCollection()
{
    ResetDrawCalls();
}

void DrawCallCollection::PushRenderProxy(DrawCallID id, const RenderProxy& render_proxy)
{
    AssertDebug(render_proxy.mesh.IsValid());
    AssertDebug(render_proxy.material.IsValid());

    DrawCall& draw_call = m_draw_calls.EmplaceBack();
    draw_call.id = id;
    draw_call.render_mesh = &render_proxy.mesh->GetRenderResource();
    draw_call.render_material = &render_proxy.material->GetRenderResource();
    draw_call.render_skeleton = render_proxy.skeleton.IsValid() ? &render_proxy.skeleton->GetRenderResource() : nullptr;
    draw_call.entity_id = render_proxy.entity.GetID();
    draw_call.draw_command_index = ~0u;
}

void DrawCallCollection::PushRenderProxyInstanced(EntityInstanceBatch* batch, DrawCallID id, const RenderProxy& render_proxy)
{
    // Auto-instancing: check if we already have a drawcall we can use for the given DrawCallID.
    auto index_map_it = m_index_map.Find(uint64(id));

    if (index_map_it == m_index_map.End())
    {
        index_map_it = m_index_map.Insert(uint64(id), {}).first;
    }

    const uint32 initial_index_map_size = index_map_it->second.Size();

    uint32 index_map_index = 0;
    uint32 instance_offset = 0;

    const uint32 initial_num_instances = render_proxy.instance_data.num_instances;
    uint32 num_instances = initial_num_instances;

    AssertDebug(initial_num_instances > 0);

    GPUBufferHolderBase* entity_instance_batches = m_impl->GetEntityInstanceBatchHolder();
    AssertThrow(entity_instance_batches != nullptr);

    while (num_instances != 0)
    {
        InstancedDrawCall* draw_call;

        if (index_map_index < initial_index_map_size)
        {
            // we have elements for the specific DrawCallID -- try to reuse them as much as possible
            draw_call = &m_instanced_draw_calls[index_map_it->second[index_map_index++]];

            AssertDebug(draw_call->id == id);
            AssertDebug(draw_call->batch != nullptr);
        }
        else
        {
            // check if we need to allocate new batch (if it has not been provided as first argument)
            if (batch == nullptr)
            {
                batch = m_impl->AcquireBatch();
            }

            AssertDebug(batch->batch_index != ~0u);

            draw_call = &m_instanced_draw_calls.EmplaceBack();

            *draw_call = InstancedDrawCall {};
            draw_call->id = id;
            draw_call->batch = batch;
            draw_call->draw_command_index = ~0u;
            draw_call->render_mesh = &render_proxy.mesh->GetRenderResource();
            draw_call->render_material = &render_proxy.material->GetRenderResource();
            draw_call->render_skeleton = render_proxy.skeleton.IsValid() ? &render_proxy.skeleton->GetRenderResource() : nullptr;
            draw_call->count = 0;

            index_map_it->second.PushBack(m_instanced_draw_calls.Size() - 1);

            // Used, set it to nullptr so it doesn't get released
            batch = nullptr;
        }

        const uint32 remaining_instances = PushEntityToBatch(*draw_call, render_proxy.entity.GetID(), render_proxy.instance_data, num_instances, instance_offset);

        instance_offset += num_instances - remaining_instances;
        num_instances = remaining_instances;
    }

    if (batch != nullptr)
    {
        // ticket has not been used at this point (always gets set to 0 after used) - need to release it
        m_impl->ReleaseBatch(batch);
    }
}

EntityInstanceBatch* DrawCallCollection::TakeDrawCallBatch(DrawCallID id)
{
    const auto it = m_index_map.Find(id.Value());

    if (it != m_index_map.End())
    {
        for (SizeType draw_call_index : it->second)
        {
            InstancedDrawCall& draw_call = m_instanced_draw_calls[draw_call_index];

            if (!draw_call.batch)
            {
                continue;
            }

            EntityInstanceBatch* batch = draw_call.batch;

            draw_call.batch = nullptr;

            return batch;
        }
    }

    return nullptr;
}

void DrawCallCollection::ResetDrawCalls()
{
    GPUBufferHolderBase* entity_instance_batches = m_impl->GetEntityInstanceBatchHolder();
    AssertDebug(entity_instance_batches != nullptr);

    for (InstancedDrawCall& draw_call : m_instanced_draw_calls)
    {
        if (draw_call.batch != nullptr)
        {
            const uint32 batch_index = draw_call.batch->batch_index;
            AssertDebug(batch_index != ~0u);

            *draw_call.batch = EntityInstanceBatch { batch_index };

            m_impl->ReleaseBatch(draw_call.batch);

            draw_call.batch = nullptr;
        }
    }

    m_draw_calls.Clear();
    m_instanced_draw_calls.Clear();
    m_index_map.Clear();
}

uint32 DrawCallCollection::PushEntityToBatch(InstancedDrawCall& draw_call, ID<Entity> entity, const MeshInstanceData& mesh_instance_data, uint32 num_instances, uint32 instance_offset)
{
#ifdef HYP_DEBUG_MODE // Sanity check
    AssertThrow(num_instances <= mesh_instance_data.num_instances);

    for (uint32 buffer_index = 0; buffer_index < uint32(mesh_instance_data.buffers.Size()); buffer_index++)
    {
        AssertThrow(mesh_instance_data.buffers[buffer_index].Size() / mesh_instance_data.buffer_struct_sizes[buffer_index] == mesh_instance_data.num_instances);
    }
#endif

    const SizeType batch_sizeof = m_impl->GetBatchSizeOf();

    bool dirty = false;

    if (mesh_instance_data.buffers.Any())
    {
        while (draw_call.batch->num_entities < max_entities_per_instance_batch && num_instances != 0)
        {
            const uint32 entity_index = draw_call.batch->num_entities++;

            draw_call.batch->indices[entity_index] = uint32(entity.ToIndex());

            // Starts at the offset of `transforms` in EntityInstanceBatch - data in buffers is expected to be
            // after the `indices` element
            uint32 field_offset = offsetof(EntityInstanceBatch, transforms);

            for (uint32 buffer_index = 0; buffer_index < uint32(mesh_instance_data.buffers.Size()); buffer_index++)
            {
                const uint32 buffer_struct_size = mesh_instance_data.buffer_struct_sizes[buffer_index];
                const uint32 buffer_struct_alignment = mesh_instance_data.buffer_struct_alignments[buffer_index];

                AssertDebugMsg(mesh_instance_data.buffers[buffer_index].Size() % buffer_struct_size == 0,
                    "Buffer size is not a multiple of buffer struct size! Buffer size: %u, Buffer struct size: %u",
                    mesh_instance_data.buffers[buffer_index].Size(), buffer_struct_size);

                field_offset = ByteUtil::AlignAs(field_offset, buffer_struct_alignment);

                void* dst_ptr = reinterpret_cast<void*>((uintptr_t(draw_call.batch)) + field_offset + (entity_index * buffer_struct_size));
                void* src_ptr = reinterpret_cast<void*>(uintptr_t(mesh_instance_data.buffers[buffer_index].Data()) + (instance_offset * buffer_struct_size));

                // sanity checks
                AssertDebugMsg((uintptr_t(dst_ptr) + buffer_struct_size) - uintptr_t(draw_call.batch) <= batch_sizeof,
                    "Buffer struct size is larger than batch size! Buffer struct size: %u, Buffer struct alignment: %u, Batch size: %u, Entity index: %u, Field offset: %u",
                    buffer_struct_size, buffer_struct_alignment, batch_sizeof, entity_index, field_offset);
                AssertDebugMsg(mesh_instance_data.buffers[buffer_index].Size() >= (instance_offset + 1) * buffer_struct_size,
                    "Buffer size is not large enough to copy data! Buffer size: %u, Buffer struct size: %u, Instance offset: %u",
                    mesh_instance_data.buffers[buffer_index].Size(), buffer_struct_size, instance_offset);

                Memory::MemCpy(dst_ptr, src_ptr, buffer_struct_size);

                field_offset += max_entities_per_instance_batch * buffer_struct_size;
            }

            instance_offset++;

            draw_call.entity_ids[draw_call.count++] = entity;

            --num_instances;

            dirty = true;
        }
    }
    else
    {
        while (draw_call.batch->num_entities < max_entities_per_instance_batch && num_instances != 0)
        {
            const uint32 entity_index = draw_call.batch->num_entities++;

            draw_call.batch->indices[entity_index] = uint32(entity.ToIndex());
            draw_call.batch->transforms[entity_index] = Matrix4::identity;

            draw_call.entity_ids[draw_call.count++] = entity;

            --num_instances;

            dirty = true;
        }
    }

    if (dirty)
    {
        m_impl->GetEntityInstanceBatchHolder()->MarkDirty(draw_call.batch->batch_index);
    }

    return num_instances;
}

#pragma endregion DrawCallCollection

#pragma region DrawCallCollectionImpl

static TypeMap<UniquePtr<IDrawCallCollectionImpl>> g_draw_call_collection_impl_map = {};
static Mutex g_draw_call_collection_impl_map_mutex = {};

HYP_API IDrawCallCollectionImpl* GetDrawCallCollectionImpl(TypeID type_id)
{
    Mutex::Guard guard(g_draw_call_collection_impl_map_mutex);

    auto it = g_draw_call_collection_impl_map.Find(type_id);

    return it != g_draw_call_collection_impl_map.End() ? it->second.Get() : nullptr;
}

HYP_API IDrawCallCollectionImpl* SetDrawCallCollectionImpl(TypeID type_id, UniquePtr<IDrawCallCollectionImpl>&& impl)
{
    Mutex::Guard guard(g_draw_call_collection_impl_map_mutex);

    auto it = g_draw_call_collection_impl_map.Set(type_id, std::move(impl)).first;

    return it->second.Get();
}

#pragma endregion DrawCallCollectionImpl

} // namespace hyperion