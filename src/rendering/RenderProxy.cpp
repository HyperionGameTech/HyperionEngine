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

extern HYP_API SafeDeleter *g_safe_deleter;

void MeshInstanceData_PostLoad(MeshInstanceData &mesh_instance_data)
{
    // Ensure at least one instance
    mesh_instance_data.num_instances = MathUtil::Max(mesh_instance_data.num_instances, 1u);

    if (mesh_instance_data.buffers.Any()) {
        for (uint32 buffer_index = 0; buffer_index < MathUtil::Min(MeshInstanceData::max_buffers, uint32(mesh_instance_data.buffers.Size())); buffer_index++) {
            if (mesh_instance_data.buffers[buffer_index].Size() / mesh_instance_data.buffer_struct_sizes[buffer_index] != mesh_instance_data.num_instances) {
                HYP_LOG(Rendering, Warning, "Expected mesh instance data to have a buffer size that is equal to (buffer struct size / number of instances). Buffer size: {}, Buffer struct size: {}, Num instances: {}",
                    mesh_instance_data.buffers[buffer_index].Size(), mesh_instance_data.buffer_struct_sizes[buffer_index],
                    mesh_instance_data.num_instances);
            }
        }

        if (mesh_instance_data.buffers.Size() > MeshInstanceData::max_buffers) {
            HYP_LOG(Rendering, Warning, "MeshInstanceData has more buffers than the maximum allowed: {} > {}", mesh_instance_data.buffers.Size(), MeshInstanceData::max_buffers);
        }
    }
}

#pragma region RenderProxy

void RenderProxy::ClaimRenderResources() const
{
    // if (material) {
    //     material->Claim();
    // }

    // if (mesh) {
    //     mesh->Claim();
    // }

    // if (skeleton) {
    //     skeleton->Claim();
    // }
}

void RenderProxy::UnclaimRenderResources() const
{
    // if (material) {
    //     material->Unclaim();
    // }

    // if (mesh) {
    //     mesh->Unclaim();
    // }

    // if (skeleton) {
    //     skeleton->Unclaim();
    // }
}

#pragma endregion RenderProxy

#pragma region RenderProxyList

void RenderProxyList::Reserve(SizeType capacity)
{
    m_proxies.Reserve(capacity);
}

RenderProxyEntityMap::Iterator RenderProxyList::Add(ID<Entity> entity, RenderProxy &&proxy)
{
    RenderProxyEntityMap::Iterator iter = m_proxies.End();

    AssertThrowMsg(!m_next_entities.Test(entity.ToIndex()), "Entity #%u already marked to be added for this iteration!", entity.Value());

    if (HasProxyForEntity(entity)) {
        iter = m_proxies.FindAs(entity);
    }

    if (iter != m_proxies.End()) {
        // if (proxy != iter->second) {

        // Advance if version has changed
        if (proxy.version != iter->second.version) {
            // Sanity check - must not contain duplicates or it will mess up safe releasing the previous RenderProxy objects
            AssertDebug(!m_changed_entities.Test(entity.ToIndex()));

            // Mark as changed if it is found in the previous iteration
            m_changed_entities.Set(entity.ToIndex(), true);

            iter->second = std::move(proxy);
        }
    } else {
        // sanity check - if not in previous iteration, it must not be in the changed list
        AssertDebug(!m_changed_entities.Test(entity.ToIndex()));

        iter = m_proxies.Insert(entity, std::move(proxy)).first;
    }

    m_next_entities.Set(entity.ToIndex(), true);

    return iter;
}

bool RenderProxyList::MarkToKeep(ID<Entity> entity)
{
    if (!m_previous_entities.Test(entity.ToIndex())) {
        return false;
    }

    m_next_entities.Set(entity.ToIndex(), true);

    return true;
}

void RenderProxyList::MarkToRemove(ID<Entity> entity)
{
    m_next_entities.Set(entity.ToIndex(), false);
}

void RenderProxyList::GetAllEntities(Array<ID<Entity>> &out_entities)
{
    HYP_SCOPE;

    Bitset all_entities = m_previous_entities | m_next_entities;

    out_entities.Reserve(all_entities.Count());

    SizeType first_set_bit_index;

    while ((first_set_bit_index = all_entities.FirstSetBitIndex()) != -1) {
        out_entities.PushBack(ID<Entity>::FromIndex(first_set_bit_index));

        all_entities.Set(first_set_bit_index, false);
    }
}

void RenderProxyList::GetRemovedEntities(Array<ID<Entity>> &out_entities, bool include_changed)
{
    HYP_SCOPE;

    Bitset removed_bits = GetRemovedEntities();

    if (include_changed) {
        removed_bits |= GetChangedEntities();
    }

    out_entities.Reserve(removed_bits.Count());

    SizeType first_set_bit_index;

    while ((first_set_bit_index = removed_bits.FirstSetBitIndex()) != -1) {
        out_entities.PushBack(ID<Entity>::FromIndex(first_set_bit_index));

        removed_bits.Set(first_set_bit_index, false);
    }
}

void RenderProxyList::GetAddedEntities(Array<RenderProxy *> &out, bool include_changed)
{
    HYP_SCOPE;
    
    Bitset newly_added_bits = GetAddedEntities();

    if (include_changed) {
        newly_added_bits |= GetChangedEntities();
    }

    out.Reserve(newly_added_bits.Count());

    Bitset::BitIndex first_set_bit_index;

    while ((first_set_bit_index = newly_added_bits.FirstSetBitIndex()) != Bitset::not_found) {
        const ID<Entity> id = ID<Entity>::FromIndex(first_set_bit_index);

        auto it = m_proxies.Find(id);
        AssertThrow(it != m_proxies.End());

        out.PushBack(&it->second);

        newly_added_bits.Set(first_set_bit_index, false);
    }
}

RenderProxy *RenderProxyList::GetProxyForEntity(ID<Entity> entity)
{
    const auto it = m_proxies.Find(entity);

    if (it == m_proxies.End()) {
        return nullptr;
    }

    return &it->second;
}

void RenderProxyList::Advance(RenderProxyListAdvanceAction action)
{
    HYP_SCOPE;

    { // Remove proxies for removed bits
        for (Bitset::BitIndex index : GetRemovedEntities()) {
            const ID<Entity> entity = ID<Entity>::FromIndex(index);

            const auto it = m_proxies.Find(entity);

            if (it != m_proxies.End()) {
                g_safe_deleter->SafeRelease(std::move(it->second));

                m_proxies.Erase(it);
            }
        }
    }

    switch (action) {
    case RenderProxyListAdvanceAction::CLEAR:
        // Next state starts out zeroed out -- and next call to Advance will remove proxies for these objs
        m_previous_entities = std::move(m_next_entities);

        break;
    case RenderProxyListAdvanceAction::PERSIST:
        m_previous_entities = m_next_entities;

        break;
    default:
        HYP_FAIL("Invalid RenderProxyListAdvanceAction");

        break;
    }

    m_changed_entities.Clear();
}

#pragma endregion RenderProxyList

} // namespace hyperion