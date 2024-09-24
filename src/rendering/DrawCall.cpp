/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DrawCall.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderProxy.hpp>

#include <Engine.hpp>

namespace hyperion {

/*! \brief Push \ref{count} instances of the given entity into an entity instance batch.
 *  If not all instances could be pushed to the given draw call's batch, a positive number will be returned.
 *  Otherwise, zero will be returned. */
static uint32 PushEntityToBatch(DrawCall &draw_call, ID<Entity> entity, uint32 count)
{
    AssertThrow(draw_call.batch_index < max_entity_instance_batches);

    EntityInstanceBatch &batch = g_engine->GetRenderData()->entity_instance_batches.Get(draw_call.batch_index);

    if (batch.num_entities < max_entities_per_instance_batch) {
        while (batch.num_entities + count < max_entities_per_instance_batch && count != 0) {
            batch.indices[batch.num_entities++] = uint32(entity.ToIndex());
            draw_call.entity_ids[draw_call.entity_id_count++] = entity;

            --count;
        }

        g_engine->GetRenderData()->entity_instance_batches.MarkDirty(draw_call.batch_index);
    }

    return count;
}

DrawCallCollection::DrawCallCollection(DrawCallCollection &&other) noexcept
    : draw_calls(std::move(other.draw_calls)),
      index_map(std::move(other.index_map))
{
}

DrawCallCollection::~DrawCallCollection()
{
    ResetDrawCalls();
}

void DrawCallCollection::PushDrawCallToBatch(BufferTicket<EntityInstanceBatch> batch_index, DrawCallID id, const RenderProxy &render_proxy)
{
    AssertThrow(render_proxy.mesh.IsValid());

    auto index_map_it = index_map.Find(id.Value());

    if (index_map_it == index_map.End()) {
        index_map_it = index_map.Insert(id.Value(), { }).first;
    }

    const uint32 initial_index_map_size = index_map_it->second.Size();
    
    uint32 index_map_index = 0;

    for (uint32 num_instances = render_proxy.num_instances; num_instances != 0;) {
        DrawCall *draw_call;

        if (index_map_index < initial_index_map_size) {
            // we have elements for the specific DrawCallID -- try to reuse them as much as possible
            draw_call = &draw_calls[index_map_it->second[index_map_index++]];

#ifdef HYP_DEBUG_MODE
            AssertThrow(draw_call->id == id);
            AssertThrow(draw_call->batch_index != 0);
#endif
        } else {
            // check if we need to allocate new batch (if it has not been provided as first argument)
            if (batch_index == 0) {
                batch_index = g_engine->GetRenderData()->entity_instance_batches.AcquireTicket();
            }

            draw_call = &draw_calls.EmplaceBack();
            draw_call->id = id;
            draw_call->draw_command_index = ~0u;
            draw_call->mesh_id = render_proxy.mesh.GetID();
            draw_call->material_id = render_proxy.material.GetID();
            draw_call->skeleton_id = render_proxy.skeleton.GetID();
            draw_call->entity_id_count = 0;
            draw_call->batch_index = batch_index;

            index_map_it->second.PushBack(draw_calls.Size() - 1);
        
            batch_index = 0;
        }

        num_instances = PushEntityToBatch(*draw_call, render_proxy.entity.GetID(), num_instances);
    }

    if (batch_index != 0) {
        // ticket has not been used at this point (always gets set to 0 after used) - need to release it
        g_engine->GetRenderData()->entity_instance_batches.ReleaseTicket(batch_index);
    }
}

DrawCall *DrawCallCollection::TakeDrawCall(DrawCallID id)
{
    const auto it = index_map.Find(id.Value());

    if (it != index_map.End()) {
        while (it->second.Any()) {
            DrawCall &draw_call = draw_calls[it->second.Back()];

            if (draw_call.batch_index != 0) {
                const SizeType num_remaining_entities = max_entities_per_instance_batch - draw_call.entity_id_count;

                if (num_remaining_entities != 0) {
                    /// vvv @TODO: Why?
                    --draw_call.entity_id_count;

                    return &draw_call;
                }
            }

            it->second.PopBack();
        }
    }

    return nullptr;
}

void DrawCallCollection::ResetDrawCalls()
{
    for (const DrawCall &draw_call : draw_calls) {
        if (draw_call.batch_index != 0) {
            EntityInstanceBatch &batch = g_engine->GetRenderData()->entity_instance_batches.Get(draw_call.batch_index);
            batch.num_entities = 0;
            // Memory::MemSet(&batch, 0, sizeof(EntityInstanceBatch));

            g_engine->GetRenderData()->entity_instance_batches.ReleaseTicket(draw_call.batch_index);
        }
    }

    draw_calls.Clear();
    index_map.Clear();
}

} // namespace hyperion