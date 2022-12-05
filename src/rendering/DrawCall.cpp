#include <rendering/DrawCall.hpp>
#include <rendering/IndirectDraw.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

DrawCallCollection::DrawCallCollection(DrawCallCollection &&other) noexcept
    : draw_calls(std::move(other.draw_calls)),
      index_map(std::move(other.index_map))
{
}

DrawCallCollection &DrawCallCollection::operator=(DrawCallCollection &&other) noexcept
{
    // for (auto &draw_call : draw_calls) {
    //     if (draw_call.batch_index) {
    //         Engine::Get()->shader_globals->FreeEntityBatch(draw_call.batch_index);
    //     }
    // }
    Reset();

    draw_calls = std::move(other.draw_calls);
    index_map = std::move(other.index_map);

    return *this;
}

DrawCallCollection::~DrawCallCollection()
{
    Reset();
}

void DrawCallCollection::Push(EntityBatchIndex batch_index, DrawCallID id, IndirectDrawState &indirect_draw_state, const EntityDrawProxy &entity)
{
    if constexpr (!use_indexed_array_for_object_data) {
        AssertThrow(id.Value() != 0);
        AssertThrow(id.HasMaterial());
    }

    auto it = index_map.find(id.Value());

    if (it != index_map.end()) {
        for (SizeType draw_call_index : it->second) {
            DrawCall &draw_call = draw_calls[draw_call_index];
            AssertThrow(batch_index == 0 ? draw_call.batch_index != 0 : draw_call.batch_index == batch_index);

            if (!Engine::Get()->shader_globals->PushEntityToBatch(draw_call.batch_index, entity.entity_id)) {
                // filled up, continue looking in array,
                // if all are filled, we push a new one
                continue;
            }

            draw_call.entity_ids[draw_call.entity_id_count++] = entity.entity_id;

            AssertThrow(draw_call.entity_id_count == Engine::Get()->shader_globals->entity_instance_batches.Get(draw_call.batch_index).num_entities);

            return;
        }

        // got here, push new item 
        it->second.PushBack(draw_calls.Size());
    } else {
        index_map.insert({ id.Value(), Array<SizeType> { draw_calls.Size() } });
    }

    DrawCall draw_call;

    draw_call.id = id;
    draw_call.draw_command_index = ~0u;

    draw_call.skeleton_id = entity.skeleton_id;
    draw_call.material_id = entity.material_id;

    draw_call.entity_ids[0] = entity.entity_id;
    draw_call.entity_id_count = 1;

    draw_call.mesh = entity.mesh;

    draw_call.batch_index = batch_index == 0 ? Engine::Get()->shader_globals->NewEntityBatch() : batch_index;
    Engine::Get()->shader_globals->PushEntityToBatch(draw_call.batch_index, entity.entity_id);

    draw_calls.PushBack(draw_call);
}

DrawCall *DrawCallCollection::TakeDrawCall(DrawCallID id)
{
    auto it = index_map.find(id.Value());

    if (it != index_map.end()) {
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
            Engine::Get()->shader_globals->FreeEntityBatch(draw_call.batch_index);
        }
    }

    draw_calls.Clear();
    index_map.clear();
}

} // namespace hyperion::v2