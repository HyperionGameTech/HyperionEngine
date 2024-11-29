/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DrawCall.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderProxy.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderCollection);

HYP_API EntityInstanceBatchHolderMap *GetEntityInstanceBatchHolderMap()
{
    return g_engine->GetEntityInstanceBatchHolderMap();
}

#pragma region DrawCallCollection

DrawCallCollection::DrawCallCollection(DrawCallCollection &&other) noexcept
    : m_impl(other.m_impl),
      m_draw_calls(std::move(other.m_draw_calls)),
      m_index_map(std::move(other.m_index_map))
{
}

DrawCallCollection &DrawCallCollection::operator=(DrawCallCollection &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    ResetDrawCalls();

    m_impl = other.m_impl;
    m_draw_calls = std::move(other.m_draw_calls);
    m_index_map = std::move(other.m_index_map);

    return *this;
}

DrawCallCollection::~DrawCallCollection()
{
    ResetDrawCalls();
}

void DrawCallCollection::PushDrawCallToBatch(uint32 batch_index, DrawCallID id, const RenderProxy &render_proxy)
{
    AssertThrow(render_proxy.mesh.IsValid());

    auto index_map_it = m_index_map.Find(uint64(id));

    if (index_map_it == m_index_map.End()) {
        index_map_it = m_index_map.Insert(uint64(id), { }).first;
    }

    const uint32 initial_index_map_size = index_map_it->second.Size();
    
    uint32 index_map_index = 0;
    uint32 instance_data_offset = 0;

    const uint32 initial_num_instances = render_proxy.NumInstances();
    uint32 num_instances = initial_num_instances;

    GPUBufferHolderBase *entity_instance_batches = m_impl->GetEntityInstanceBatchHolder();
    AssertThrow(entity_instance_batches != nullptr);

    while (num_instances != 0) {
        DrawCall *draw_call;

        if (index_map_index < initial_index_map_size) {
            // we have elements for the specific DrawCallID -- try to reuse them as much as possible
            draw_call = &m_draw_calls[index_map_it->second[index_map_index++]];

#ifdef HYP_DEBUG_MODE
            AssertThrow(draw_call->id == id);
            AssertThrow(draw_call->batch_index != ~0u);
#endif
        } else {
            // check if we need to allocate new batch (if it has not been provided as first argument)
            if (batch_index == ~0u) {
                batch_index = m_impl->AcquireBatchIndex();
            }

            draw_call = &m_draw_calls.EmplaceBack();

            *draw_call = DrawCall { };
            draw_call->id = id;
            draw_call->batch_index = batch_index;
            draw_call->draw_command_index = ~0u;
            draw_call->material_buffer_index = render_proxy.material.IsValid() ? render_proxy.material->GetRenderResources().GetBufferIndex() : ~0u;
            draw_call->mesh = render_proxy.mesh.Get();
            draw_call->material_id = render_proxy.material.GetID();
            draw_call->skeleton_id = render_proxy.skeleton.GetID();
            draw_call->entity_id_count = 0;

            index_map_it->second.PushBack(m_draw_calls.Size() - 1);
        
            batch_index = ~0u;
        }

        if (render_proxy.instance_data.buffers.Any()) {
//             instance_data_stride = render_proxy.instance_data.struct_size;

// #ifdef HYP_DEBUG_MODE
//             AssertThrow(instance_data_stride != 0);
//             AssertThrow(instance_data_stride % render_proxy.instance_data.buffer.Size() == 0);
// #endif

            // instance_data = render_proxy.instance_data.buffer.ToSpan().Slice(instance_data_offset * instance_data_stride);

            // AssertThrow(instance_data.Size() / instance_data_stride == num_instances);
        } else {
            // instance_data_stride = sizeof(Matrix4);
            // instance_data = { reinterpret_cast<const ubyte *>(&Matrix4::identity), 1 };
        }

        const uint32 remaining_instances = PushEntityToBatch(*draw_call, render_proxy.entity.GetID(), render_proxy.instance_data, num_instances, instance_data_offset);

        num_instances = remaining_instances;
        instance_data_offset += initial_num_instances - num_instances;
    }

    if (batch_index != ~0u) {
        // ticket has not been used at this point (always gets set to 0 after used) - need to release it
        m_impl->ReleaseBatchIndex(batch_index);
    }
}

uint32 DrawCallCollection::TakeDrawCallBatchIndex(DrawCallID id)
{
    const auto it = m_index_map.Find(id.Value());

    if (it != m_index_map.End()) {
        for (SizeType draw_call_index : it->second) {
            DrawCall &draw_call = m_draw_calls[draw_call_index];

            if (draw_call.batch_index == ~0u) {
                continue;
            }

            const uint32 batch_index = draw_call.batch_index;

            draw_call = DrawCall { };

            return batch_index;
        }
    }

    return ~0u;
}

void DrawCallCollection::ResetDrawCalls()
{
    GPUBufferHolderBase *entity_instance_batches = m_impl->GetEntityInstanceBatchHolder();
    AssertThrow(entity_instance_batches != nullptr);

    for (const DrawCall &draw_call : m_draw_calls) {
        if (draw_call.batch_index != ~0u) {
            entity_instance_batches->ResetElement(draw_call.batch_index);

            m_impl->ReleaseBatchIndex(draw_call.batch_index);
        }
    }

    m_draw_calls.Clear();
    m_index_map.Clear();
}

uint32 DrawCallCollection::PushEntityToBatch(DrawCall &draw_call, ID<Entity> entity, const MeshInstanceData &mesh_instance_data, uint32 num_instances, uint32 instance_data_offset)
{
#ifdef HYP_DEBUG_MODE // Sanity checks
    // AssertThrow(instance_data_offset < mesh_instance_data.NumInstances());

    // AssertThrow(instance_data.Size() / instance_data_stride <= num_instances);

    // // The instance data buffer should be all the bytes after `indices`.
    // AssertThrow(mesh_instance_data + offsetof(EntityInstanceBatch, transforms) == m_impl->GetBatchSizeOf());

    AssertThrow(draw_call.batch_index < m_impl->GetNumBatches());

    for (uint32 buffer_index = 0; buffer_index < uint32(mesh_instance_data.buffers.Size()); buffer_index++) {
        AssertThrow(mesh_instance_data.buffers[buffer_index].Size() / mesh_instance_data.buffer_struct_sizes[buffer_index] == mesh_instance_data.num_instances);
    }
#endif

    EntityInstanceBatch &batch = m_impl->GetBatch(draw_call.batch_index);
    const SizeType batch_sizeof = m_impl->GetBatchSizeOf();

    bool dirty = false;

    if (mesh_instance_data.buffers.Any()) {
        uint32 instance_index = instance_data_offset;

        while (batch.num_entities < max_entities_per_instance_batch && num_instances != 0) {
            const uint32 entity_index = batch.num_entities++;

            batch.indices[entity_index] = uint32(entity.ToIndex());

            // Starts at the offset of `transforms` in EntityInstanceBatch - data in buffers is expected to be
            // after the `indices` element
            uint32 field_offset = offsetof(EntityInstanceBatch, transforms);

            for (uint32 buffer_index = 0; buffer_index < uint32(mesh_instance_data.buffers.Size()); buffer_index++) {
                const uint32 buffer_struct_size = mesh_instance_data.buffer_struct_sizes[buffer_index];
                const uint32 buffer_struct_alignment = mesh_instance_data.buffer_struct_alignments[buffer_index];

                field_offset = ByteUtil::AlignAs(field_offset, buffer_struct_alignment);

                void *dst_ptr = reinterpret_cast<void *>((uintptr_t(&batch)) + field_offset + (entity_index * buffer_struct_size));
                void *src_ptr = reinterpret_cast<void *>(uintptr_t(mesh_instance_data.buffers[buffer_index].Data()) + (instance_index * buffer_struct_size));

#ifdef HYP_DEBUG_MODE
                // sanity checks
                AssertThrow((uintptr_t(dst_ptr) + buffer_struct_size) - uintptr_t(&batch) <= batch_sizeof);
#endif

                Memory::MemCpy(dst_ptr, src_ptr, buffer_struct_size);

                field_offset += max_entities_per_instance_batch * buffer_struct_size;
            }

            instance_index++;

            draw_call.entity_ids[draw_call.entity_id_count++] = entity;

            --num_instances;

            dirty = true;
        }
    } else {
        while (batch.num_entities < max_entities_per_instance_batch && num_instances != 0) {
            const uint32 entity_index = batch.num_entities++;

            batch.indices[entity_index] = uint32(entity.ToIndex());
            batch.transforms[entity_index] = Matrix4::identity;

            draw_call.entity_ids[draw_call.entity_id_count++] = entity;

            --num_instances;

            dirty = true;
        }
    }

    if (dirty) {
        m_impl->GetEntityInstanceBatchHolder()->MarkDirty(draw_call.batch_index);
    }

    return num_instances;
}

#pragma endregion DrawCallCollection

#pragma region DrawCallCollectionImpl

static TypeMap<UniquePtr<IDrawCallCollectionImpl>> g_draw_call_collection_impl_map = { };
static Mutex g_draw_call_collection_impl_map_mutex = { };

HYP_API IDrawCallCollectionImpl *GetDrawCallCollectionImpl(TypeID type_id)
{
    Mutex::Guard guard(g_draw_call_collection_impl_map_mutex);

    auto it = g_draw_call_collection_impl_map.Find(type_id);

    return it != g_draw_call_collection_impl_map.End() ? it->second.Get() : nullptr;
}

HYP_API IDrawCallCollectionImpl *SetDrawCallCollectionImpl(TypeID type_id, UniquePtr<IDrawCallCollectionImpl> &&impl)
{
    Mutex::Guard guard(g_draw_call_collection_impl_map_mutex);

    auto it = g_draw_call_collection_impl_map.Set(type_id, std::move(impl)).first;
    
    return it->second.Get();
}

#pragma endregion DrawCallCollectionImpl

} // namespace hyperion