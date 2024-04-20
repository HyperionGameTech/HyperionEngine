/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/DrawCall.hpp>
#include <rendering/IndirectDraw.hpp>
#include <Engine.hpp>

namespace hyperion {

static bool PushEntityToBatch(BufferTicket<EntityInstanceBatch> batch_index, ID<Entity> entity_id)
{
    AssertThrow(batch_index < max_entity_instance_batches);

    EntityInstanceBatch &batch = g_engine->GetRenderData()->entity_instance_batches.Get(batch_index);

    if (batch.num_entities >= max_entities_per_instance_batch) {
        return false;
    }

    const uint32 id_index = batch.num_entities++;
    batch.indices[id_index] = uint32(entity_id.ToIndex());
    g_engine->GetRenderData()->entity_instance_batches.MarkDirty(batch_index);

    return true;
}

DrawCallCollection::DrawCallCollection(DrawCallCollection &&other) noexcept
    : draw_calls(std::move(other.draw_calls)),
      index_map(std::move(other.index_map))
{
}

DrawCallCollection &DrawCallCollection::operator=(DrawCallCollection &&other) noexcept
{
    Reset();

    draw_calls = std::move(other.draw_calls);
    index_map = std::move(other.index_map);

    return *this;
}

DrawCallCollection::~DrawCallCollection()
{
    Reset();
}

void DrawCallCollection::PushDrawCall(BufferTicket<EntityInstanceBatch> batch_index, DrawCallID id, EntityDrawData entity_draw_data)
{
    AssertThrow(entity_draw_data.mesh_id.IsValid());

#ifndef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
    AssertThrow(id.Value() != 0);
    AssertThrow(id.HasMaterial());
#endif

    const auto it = index_map.Find(id.Value());

    if (it != index_map.End()) {
        for (const SizeType draw_call_index : it->second) {
            DrawCall &draw_call = draw_calls[draw_call_index];
            AssertThrow(batch_index == 0 ? draw_call.batch_index != 0 : draw_call.batch_index == batch_index);

            if (!PushEntityToBatch(draw_call.batch_index, entity_draw_data.entity_id)) {
                // filled up, continue looking in array,
                // if all are filled, we push a new one
                continue;
            }

            draw_call.entity_ids[draw_call.entity_id_count++] = entity_draw_data.entity_id;

            AssertThrow(draw_call.entity_id_count == g_engine->GetRenderData()->entity_instance_batches.Get(draw_call.batch_index).num_entities);

            return;
        }

        // got here, push new item 
        it->second.PushBack(draw_calls.Size());
    } else {
        index_map.Insert(id.Value(), Array<SizeType> { draw_calls.Size() });
    }

    if (!batch_index) {
        batch_index = g_engine->GetRenderData()->entity_instance_batches.AcquireTicket();
    }

    DrawCall draw_call;
    draw_call.id = id;
    draw_call.draw_command_index = ~0u;
    draw_call.mesh_id = entity_draw_data.mesh_id;
    draw_call.material_id = entity_draw_data.material_id;
    draw_call.skeleton_id = entity_draw_data.skeleton_id;
    draw_call.entity_ids[0] = entity_draw_data.entity_id;
    draw_call.entity_id_count = 1;
    draw_call.batch_index = batch_index;

    PushEntityToBatch(draw_call.batch_index, entity_draw_data.entity_id);

    draw_calls.PushBack(draw_call);
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
                    --draw_call.entity_id_count;

                    return &draw_call;
                }
            }

            it->second.PopBack();
        }
    }

    return nullptr;
}

void DrawCallCollection::Reset()
{
    for (const DrawCall &draw_call : draw_calls) {
        if (draw_call.batch_index) {
            g_engine->GetRenderData()->entity_instance_batches.ReleaseTicket(draw_call.batch_index);
        }
    }

    draw_calls.Clear();
    index_map.Clear();
}

} // namespace hyperion