/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Octree.hpp>
#include <scene/Entity.hpp>
#include <scene/Node.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/BVHComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/camera/Camera.hpp>

#include <core/algorithm/Every.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

Octree::Octree(const Handle<EntityManager>& entity_manager)
    : OctreeBase(),
      m_entity_manager(entity_manager)
{
}

Octree::Octree(const Handle<EntityManager>& entity_manager, const BoundingBox& aabb)
    : OctreeBase(aabb),
      m_entity_manager(entity_manager)
{
}

Octree::Octree(const Handle<EntityManager>& entity_manager, const BoundingBox& aabb, Octree* parent, uint8 index)
    : OctreeBase(aabb, parent, index),
      m_entity_manager(entity_manager)
{
}

Octree::~Octree() = default;

void Octree::SetEntityManager(const Handle<EntityManager>& entity_manager)
{
    HYP_SCOPE;

    m_entity_manager = entity_manager;

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            static_cast<Octree*>(octant.octree.Get())->SetEntityManager(m_entity_manager);
        }
    }
}

void Octree::Clear()
{
    HYP_SCOPE;

    Array<Entry> entries;
    OctreeBase::Clear(entries, /* undivide */ true);

    if (m_entity_manager)
    {
        AssertThrow(Threads::IsOnThread(m_entity_manager->GetOwnerThreadID()));

        for (Entry& entry : entries)
        {
            if (VisibilityStateComponent* visibility_state_component = m_entity_manager->TryGetComponent<VisibilityStateComponent>(entry.value))
            {
                visibility_state_component->octant_id = OctantID::Invalid();
                visibility_state_component->visibility_state = nullptr;
            }

            m_entity_manager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entry.value);
        }
    }

    RebuildEntriesHash();
}

Octree::InsertResult Octree::Rebuild(const BoundingBox& new_aabb, bool allow_grow)
{
    Array<Entry> new_entries;
    OctreeBase::Clear(new_entries, /* undivide */ true);

    m_aabb = new_aabb;

    for (auto& it : new_entries)
    {
        Entity* entity = it.value;
        AssertThrow(entity != nullptr);

        if (it.aabb.IsValid() && it.aabb.IsFinite())
        {
            if (IsRoot())
            {
                AssertDebug(allow_grow);

                m_aabb = m_aabb.Union(it.aabb);
            }
            else
            {
                // should already be contained in the aabb of non-root octants
                AssertDebug(m_aabb.Contains(it.aabb));
            }
        }

        auto insert_result = Insert(entity, it.aabb, true /* allow rebuild */);

        if (!insert_result.first)
        {
            return insert_result;
        }

        VisibilityStateComponent* visibility_state_component = m_entity_manager->TryGetComponent<VisibilityStateComponent>(entity);

        if (visibility_state_component)
        {
            visibility_state_component->octant_id = insert_result.second;
            visibility_state_component->visibility_state = nullptr;
        }
        else
        {
            m_entity_manager->AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent { .octant_id = insert_result.second, .visibility_state = nullptr });
        }

        m_entity_manager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);
    }

    return {
        { OctreeBase::Result::OCTREE_OK },
        m_octant_id
    };
}

void Octree::PerformUpdates()
{
    HYP_SCOPE;

    AssertThrow(m_state != nullptr);

    if (!m_state->NeedsRebuild())
    {
        // No octant to rebuild, skipping
        return;
    }

    OctreeBase::PerformUpdates();

    RebuildEntriesHash();
}

void Octree::NextVisibilityState()
{
    HYP_SCOPE;

    AssertThrow(IsRoot());

    m_visibility_state.Next();
}

void Octree::CalculateVisibility(const Handle<Camera>& camera)
{
    HYP_SCOPE;

    AssertThrow(IsRoot());

    UpdateVisibilityState(camera, m_visibility_state.validity_marker);
}

void Octree::UpdateVisibilityState(const Handle<Camera>& camera, uint16 validity_marker)
{
    if (!camera.IsValid())
    {
        return;
    }

    const Frustum& frustum = camera->GetFrustum();

    if (!frustum.ContainsAABB(m_aabb))
    {
        return;
    }

    Octree* current = this;
    uint8 child_index = uint8(-1);

    while (true)
    {
        // Process current node.
        current->m_visibility_state.validity_marker = validity_marker;
        current->m_visibility_state.MarkAsValid(camera.GetID());

        if (current->m_is_divided)
        {
            bool descended = false;

            for (uint8 i = child_index + 1; i < 8; i++)
            {
                if (!frustum.ContainsAABB(current->m_octants[i].aabb))
                {
                    continue;
                }

                current = static_cast<Octree*>(current->m_octants[i].octree.Get());
                child_index = uint8(-1);

                descended = true;

                break;
            }
            if (descended)
            {
                continue;
            }
        }

        if (current->m_parent == nullptr)
        {
            break;
        }

        child_index = current->m_octant_id.GetIndex();
        current = static_cast<Octree*>(current->m_parent);
    }
}

void Octree::ResetEntriesHash()
{
    HYP_SCOPE;

    m_entry_hashes = {};
}

void Octree::RebuildEntriesHash(uint32 level)
{
    HYP_SCOPE;

    ResetEntriesHash();

    for (const Entry& entry : m_entries)
    {
        const HashCode entry_hash_code = entry.GetHashCode();
        m_entry_hashes[0].Add(entry_hash_code);

        Array<EntityTag> tags;

        if (m_entity_manager)
        {
            // @FIXME: Having issue here where entity is no longer part of this entitymanager.
            // Must be getting moved to a different one.
            tags = m_entity_manager->GetTags(entry.value);
        }

        for (uint32 i = 0; i < uint32(tags.Size()); i++)
        {
            const uint32 num_combinations = (1u << i);

            for (uint32 k = 0; k < num_combinations; k++)
            {
                uint32 mask = (1u << (uint32(tags[i]) - 1));

                for (uint32 j = 0; j < i; j++)
                {
                    if ((k & (1u << j)) != (1u << j))
                    {
                        continue;
                    }

                    mask |= (1u << (uint32(tags[j]) - 1));
                }

                AssertThrow(m_entry_hashes.Size() > mask);

                m_entry_hashes[mask].Add(entry_hash_code);
            }
        }
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            static_cast<Octree*>(octant.octree.Get())->RebuildEntriesHash(level + 1);
        }
    }

    // Update parent's hash to include this octant's hash
    if (m_parent)
    {
        for (uint32 i = 0; i < uint32(m_entry_hashes.Size()); i++)
        {
            static_cast<Octree*>(m_parent)->m_entry_hashes[i].Add(m_entry_hashes[i]);
        }
    }
}

bool Octree::TestRay(const Ray& ray, RayTestResults& out_results, bool use_bvh) const
{
    HYP_SCOPE;

    bool has_hit = false;

    if (ray.TestAABB(m_aabb))
    {
        for (const Octree::Entry& entry : m_entries)
        {
            RayTestResults aabb_result;

            if (use_bvh)
            {
                if (m_entity_manager && entry.value != nullptr)
                {
                    if (!m_entity_manager->HasEntity(entry.value))
                    {
                        continue;
                    }

                    // If the entity has a BVH associated with it, use that instead of the AABB for more accuracy
                    if (BVHComponent* bvh_component = m_entity_manager->TryGetComponent<BVHComponent>(entry.value))
                    {
                        Matrix4 model_matrix = Matrix4::Identity();
                        Matrix4 normal_matrix = Matrix4::Identity();

                        Ray local_space_ray = ray;

                        if (TransformComponent* transform_component = m_entity_manager->TryGetComponent<TransformComponent>(entry.value))
                        {
                            model_matrix = transform_component->transform.GetMatrix();
                            normal_matrix = model_matrix.Transposed().Inverted();

                            local_space_ray = model_matrix.Inverted() * ray;
                        }

                        RayTestResults local_bvh_results = bvh_component->bvh.TestRay(local_space_ray);

                        if (local_bvh_results.Any())
                        {
                            RayTestResults bvh_results;

                            for (RayHit hit : local_bvh_results)
                            {
                                hit.id = entry.value->GetID().Value();
                                hit.user_data = nullptr;

                                Vec4f transformed_normal = normal_matrix * Vec4f(hit.normal, 0.0f);
                                hit.normal = transformed_normal.GetXYZ().Normalized();

                                Vec4f transformed_position = model_matrix * Vec4f(hit.hitpoint, 1.0f);
                                transformed_position /= transformed_position.w;

                                hit.hitpoint = transformed_position.GetXYZ();

                                hit.distance = (hit.hitpoint - ray.position).Length();

                                bvh_results.AddHit(hit);
                            }

                            out_results.Merge(std::move(bvh_results));

                            has_hit = true;
                        }

                        continue;
                    }
                    else
                    {
                        NodeLinkComponent* node_link_component = m_entity_manager->TryGetComponent<NodeLinkComponent>(entry.value);
                        Handle<Node> node = node_link_component ? node_link_component->node.Lock() : nullptr;

                        HYP_LOG(Octree, Warning, "Entity #{} (node: {}) does not have a BVH component, using AABB instead", entry.value->GetID(), node ? node->GetName() : NAME("<null>"));
                    }
                }
            }

            if (ray.TestAABB(entry.aabb, entry.value->GetID().Value(), nullptr, aabb_result))
            {
                out_results.Merge(std::move(aabb_result));

                has_hit = true;
            }
        }

        if (m_is_divided)
        {
            for (const Octant& octant : m_octants)
            {
                AssertThrow(octant.octree != nullptr);

                if (static_cast<Octree*>(octant.octree.Get())->TestRay(ray, out_results))
                {
                    has_hit = true;
                }
            }
        }
    }

    return has_hit;
}

} // namespace hyperion