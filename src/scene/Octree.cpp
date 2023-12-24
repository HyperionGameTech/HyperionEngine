#include "Octree.hpp"
#include "Entity.hpp"
#include <Engine.hpp>
#include <scene/camera/Camera.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

const BoundingBox Octree::default_bounds = BoundingBox({ -250.0f }, { 250.0f });

Bool Octree::IsVisible(
    const Octree *root,
    const Octree *child
)
{
    return child->m_visibility_state.ValidToParent(
        root->m_visibility_state,
        (root->GetRoot()->visibility_cursor.load(std::memory_order_relaxed) + VisibilityState::cursor_size - 1) % VisibilityState::cursor_size
    );
}

Bool Octree::IsVisible(
    const Octree *root,
    const Octree *child,
    UInt8 cursor
)
{
    return child->m_visibility_state.ValidToParent(
        root->m_visibility_state,
        cursor
    );
}

Octree::Octree(const BoundingBox &aabb)
    : Octree(nullptr, aabb, 0)
{
    m_root = new Root;
}

Octree::Octree(Octree *parent, const BoundingBox &aabb, UInt8 index)
    : m_parent(nullptr),
      m_aabb(aabb),
      m_is_divided(false),
      m_root(nullptr),
      m_visibility_state{},
      m_index(index)
{
    if (parent != nullptr) {
        SetParent(parent); // call explicitly to set root ptr
    }

    InitOctants();
}

Octree::~Octree()
{
    if (IsRoot()) {
        delete m_root;
    }

    AssertThrowMsg(m_nodes.Empty(), "Expected nodes to be emptied before octree destructor");
}

void Octree::SetParent(Octree *parent)
{
    m_parent = parent;

    if (m_parent != nullptr) {
        m_root = m_parent->m_root;
    } else {
        m_root = nullptr;
    }
}

Bool Octree::EmptyDeep(Int depth, UInt8 octant_mask) const
{
    if (!Empty()) {
        return false;
    }

    if (!m_is_divided) {
        return true;
    }

    if (depth != 0) {
        return m_octants.Every([depth, octant_mask](const Octant &octant) {
            if (octant_mask & (1u << octant.octree->m_index)) {
                return octant.octree->EmptyDeep(depth - 1);
            }

            return true;
        });
    }

    return true;
}

void Octree::InitOctants()
{
    const Vector3 divided_aabb_dimensions = m_aabb.GetExtent() / 2.0f;

    for (UInt x = 0; x < 2; x++) {
        for (UInt y = 0; y < 2; y++) {
            for (UInt z = 0; z < 2; z++) {
                const UInt index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + divided_aabb_dimensions * Vector3(Float(x), Float(y), Float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vector3(Float(x), Float(y), Float(z)) + Vector3(1.0f))
                    )
                };
            }
        }
    }
}

void Octree::Divide()
{
    AssertThrow(!m_is_divided);

    for (SizeType i = 0; i < m_octants.Size(); i++) {
        auto &octant = m_octants[i];

        AssertThrow(octant.octree == nullptr);

        octant.octree.Reset(new Octree(this, octant.aabb, UInt8(i)));
    }

    m_is_divided = true;
    RebuildNodesHash();
}

void Octree::Undivide()
{
    AssertThrow(m_is_divided);
    AssertThrowMsg(m_nodes.Empty(), "Undivide() should be called on octrees with no remaining nodes");

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->m_is_divided) {
            octant.octree->Undivide();
        }

        octant.octree.Reset();
    }

    m_is_divided = false;
}

void Octree::CollapseParents()
{
    if (m_is_divided || !Empty()) {
        return;
    }

    Octree *iteration = m_parent,
        *highest_empty = nullptr;

    while (iteration != nullptr && iteration->Empty()) {
        for (const Octant &octant : iteration->m_octants) {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree.Get() == highest_empty) {
                /* Do not perform a check on our node, as we've already
                 * checked it by going up the chain.
                 * As `iter` becomes the parent of this node we're currently working with,
                 * we will not have to perform duplicate EmptyDeep() checks on any octants because of this check.
                 */
                continue;
            }

            if (!octant.octree->EmptyDeep()) {
                goto undivide;
            }
        }

        highest_empty = iteration;
        iteration = iteration->m_parent;
    }

undivide:
    if (highest_empty != nullptr) {
        highest_empty->Undivide();
    }
}

void Octree::Clear()
{
    Array<Node> nodes;

    Clear(nodes);
}

void Octree::Clear(Array<Node> &out_nodes)
{
    ClearInternal(out_nodes);

    if (m_is_divided) {
        Undivide();
    }
}

void Octree::ClearInternal(Array<Node> &out_nodes)
{
    for (auto &node : m_nodes) {
        node.entity->OnRemovedFromOctree(this);

        if (m_root != nullptr) {
            auto it = m_root->node_to_octree.find(node.entity);

            if (it != m_root->node_to_octree.end()) {
                m_root->node_to_octree.erase(it);
            }
        }

        out_nodes.PushBack(std::move(node));
    }

    m_nodes.Clear();

    if (!m_is_divided) {
        return;
    }

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        octant.octree->ClearInternal(out_nodes);
    }
}

Octree::Result Octree::Insert(Entity *entity)
{
    AssertThrow(entity != nullptr);

    const BoundingBox &entity_aabb = entity->GetWorldAABB();

    if (!m_aabb.Contains(entity_aabb)) {
        auto rebuild_result = RebuildExtendInternal(entity_aabb);

        if (!rebuild_result) {
            DebugLog(
                LogType::Warn,
                "Failed to rebuild octree when inserting entity #%lu\n",
                entity->GetID().value
            );

            return rebuild_result;
        }
    }

    // stop recursing if our aabb is too small
    if (m_aabb.GetExtent().Length() >= min_aabb_size) {
        for (Octant &octant : m_octants) {
            if (octant.aabb.Contains(entity_aabb)) {
                if (!m_is_divided) {
                    Divide();
                }

                AssertThrow(octant.octree != nullptr);

                const Octree::Result result = octant.octree->Insert(entity);

                RebuildNodesHash();

                return result;
            }
        }
    }

    return InsertInternal(entity);
}

Octree::Result Octree::InsertInternal(Entity *entity)
{
    m_nodes.PushBack(Node {
        .entity = entity,
        .aabb = entity->GetWorldAABB()
    });

    if (m_root != nullptr) {
        AssertThrowMsg(m_root->node_to_octree.find(entity) == m_root->node_to_octree.end(), "Entity must not already be in octree hierarchy.");

        m_root->node_to_octree[entity] = this;
    }

    entity->OnAddedToOctree(this);

    RebuildNodesHash();

    return {};
}

Octree::Result Octree::Remove(Entity *entity)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(entity);

        if (it == m_root->node_to_octree.end()) {
            return {Result::OCTREE_ERR, "Not found in node map"};
        }

        if (auto *octree = it->second) {
            return octree->RemoveInternal(entity);
        }

        return {Result::OCTREE_ERR, "Could not be removed from any sub octants"};
    }

    if (!m_aabb.Contains(entity->GetWorldAABB())) {
        return {Result::OCTREE_ERR, "AABB does not contain object aabb"};
    }

    return RemoveInternal(entity);
}

Octree::Result Octree::RemoveInternal(Entity *entity)
{
    const auto it = FindNode(entity);

    if (it == m_nodes.End()) {
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->RemoveInternal(entity)) {
                    return {};
                }
            }
        }

        return {Result::OCTREE_ERR, "Could not be removed from any sub octants"};
    }

    if (m_root != nullptr) {
        m_root->node_to_octree.erase(entity);
    }

    entity->OnRemovedFromOctree(this);

    m_nodes.Erase(it);
    RebuildNodesHash();

    if (!m_is_divided && m_nodes.Empty()) {
        Octree *last_empty_parent = nullptr;

        if (Octree *parent = m_parent) {
            const Octree *child = this;

            while (parent->EmptyDeep(DEPTH_SEARCH_INF, 0xff & ~(1 << child->m_index))) { // do not search this branch of the tree again
                last_empty_parent = parent;

                if (parent->m_parent == nullptr) {
                    break;
                }

                child  = parent;
                parent = child->m_parent;
            }
        }

        if (last_empty_parent != nullptr) {
            AssertThrow(last_empty_parent->EmptyDeep(DEPTH_SEARCH_INF));

            /* At highest empty parent octant, call Undivide() to collapse nodes */
            last_empty_parent->Undivide();
        }
    }

    return {};
}

Octree::Result Octree::Move(Entity *entity, const Array<Node>::Iterator *it)
{
    const BoundingBox &new_aabb = entity->GetWorldAABB();

    const Bool is_root = IsRoot();
    const Bool contains = m_aabb.Contains(new_aabb);

    if (!contains) {
        // NO LONGER CONTAINS AABB

        if (is_root) {
            // have to rebuild, invalidating child octants.
            // which we have a Contains() check for child nodes walking upwards

#if HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Debug,
                "In root, but does not contain node aabb, so rebuilding octree. %lu\n",
                entity->GetID().value
            );
#endif

            return RebuildExtendInternal(new_aabb);
        }

        // not root
#if HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Debug,
                "Moving entity #%lu into the closest fitting (or root) parent\n",
                entity->GetID().value
            );
#endif


        Bool inserted = false;

        /* Contains is false at this point */
        Octree *parent = m_parent;
        Octree *last_parent = m_parent;

        while (parent != nullptr) {
            last_parent = parent;

            if (parent->m_aabb.Contains(new_aabb)) {
                
                if (it != nullptr) {
                    if (m_root != nullptr) {
                        m_root->node_to_octree.erase(entity);
                    }

                    m_nodes.Erase(*it);
                    RebuildNodesHash();
                }

                inserted = Bool(parent->Move(entity));

                break;
            }

            parent = parent->m_parent;
        }

        if (inserted) { // succesfully inserted, safe to call CollapseParents()
            /* Node has now been added to it's appropriate octant -- remove any potential empty octants */
            CollapseParents();

            return {};
        }

        // not inserted because no Move() was called on parents (because they don't contain AABB),
        // have to _manually_ call Move() which will go to the above branch for the parent octant,
        // this invalidating `this`

#if HYP_OCTREE_DEBUG
                DebugLog(
                    LogType::Debug,
                    "In child, no parents contain AABB so calling Move() on last valid octant (root). This will invalidate `this`.. %lu\n",
                    entity->GetID().value
                );
#endif

        AssertThrow(last_parent != nullptr);

        return last_parent->Move(entity);
    }

    // CONTAINS AABB HERE

    // if our AABB is too small, stop recursing...
    if (m_aabb.GetExtent().Length() >= min_aabb_size) {
        for (Octant &octant : m_octants) {
            if (octant.aabb.Contains(new_aabb)) {
                /* we /can/ go a level deeper than current, so no matter what we dispatch the
                * event that the entity was 'removed' from this octant, as it will be added
                * deeper regardless.
                * note: we don't call OnRemovedFromOctree() on the entity, we don't want the Entity's
                * m_octree ptr to be nullptr at any point, which could cause flickering when we go to render
                * stuff. that's the purpose for this whole method, to set it in one pass
                */
                if (it != nullptr) {
                    if (m_root != nullptr) {
                        m_root->node_to_octree.erase(entity);
                    }

                    m_nodes.Erase(*it);
                }
                
                if (!m_is_divided) {
                    Divide();
                }
                
                AssertThrow(octant.octree != nullptr);

                const auto octant_move_result = octant.octree->Move(entity, nullptr);
                AssertThrow(octant_move_result);

                RebuildNodesHash();

                return octant_move_result;
            }
        }
    }

    if (it != nullptr) { /* Not moved */
        auto &entity_it = *it;

        entity_it->aabb = new_aabb;
    } else { /* Moved into new octant */
        // force this octant to be visible to prevent flickering
        CopyVisibilityState(g_engine->GetWorld()->GetOctree().GetVisibilityState());

        m_nodes.PushBack(Node {
            .entity = entity,
            .aabb = entity->GetWorldAABB()
        });

        if (m_root != nullptr) {
            AssertThrowMsg(m_root->node_to_octree.find(entity) == m_root->node_to_octree.end(), "Entity must not already be in octree hierarchy.");

            m_root->node_to_octree[entity] = this;
        }

        entity->OnMovedToOctant(this);
    }

    RebuildNodesHash();
    
    return {};
}

Octree::Result Octree::Update(Entity *entity)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(entity);

        if (it == m_root->node_to_octree.end()) {
            return { Result::OCTREE_ERR, "Object not found in node map!" };
        }

        if (Octree *octree = it->second) {
            return octree->UpdateInternal(entity);
        }

        return { Result::OCTREE_ERR, "Object has no octree in node map!" };
    }

    return UpdateInternal(entity);
}

Octree::Result Octree::UpdateInternal(Entity *entity)
{
    const auto it = FindNode(entity);

    if (it == m_nodes.End()) {
        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->UpdateInternal(entity)) {
                    return {};
                }
            }
        }

        return { Result::OCTREE_ERR, "Could not update in any sub octants" };
    }

    const BoundingBox &new_aabb = entity->GetWorldAABB();
    const BoundingBox &old_aabb = it->aabb;

    if (new_aabb == old_aabb) {
        /* AABB has not changed - no need to update */
        return { };
    }

    /* AABB has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    return Move(entity, &it);
}

Octree::Result Octree::Rebuild(const BoundingBox &new_aabb)
{
    DebugLog(LogType::Debug, "Rebuild octree\n");

    Array<Node> new_nodes;
    Clear(new_nodes);

    m_aabb = new_aabb;

    for (auto &node : new_nodes) {
        if (auto *entity = node.entity) {
            auto insert_result = Insert(entity);

            if (!insert_result) {
                return insert_result;
            }
        }
    }
    
    // force all entities visible to prevent flickering
    ForceVisibilityStates();

    return {};
}

Octree::Result Octree::RebuildExtendInternal(const BoundingBox &extend_include_aabb)
{
    // have to grow the aabb by rebuilding the octree
    BoundingBox new_aabb(m_aabb);
    // extend the new aabb to include the entity
    new_aabb.Extend(extend_include_aabb);
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    new_aabb *= growth_factor;

    return Rebuild(new_aabb);
}

void Octree::ForceVisibilityStates()
{
    m_visibility_state.ForceAllVisible();
    
    for (auto &node : m_nodes) {
        AssertThrow(node.entity != nullptr);
        node.entity->m_visibility_state.ForceAllVisible();
    }

    if (m_is_divided) {
        for (auto &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->ForceVisibilityStates();
        }
    }
}

void Octree::CopyVisibilityState(const VisibilityState &visibility_state)
{
    /* HACK to prevent objects having no visibility state and flicking when switching octants.
    * We set the visibility state to be the most up to date it can be, and in the next visibility
    * state check, the proper state will be applied
    */

    // tried having it just load from the current and next cursor. doesn't work. not sure why.
    // but it could be a slight performance boost.
    for (UInt i = 0; i < UInt(m_visibility_state.snapshots.Size()); i++) {
        // m_visibility_state.snapshots[i] = visibility_state.snapshots[i];

        m_visibility_state.snapshots[i].bits |= visibility_state.snapshots[i].bits;
        m_visibility_state.snapshots[i].nonce = visibility_state.snapshots[i].nonce;
    }
    
    // m_visibility_state.bits.fetch_or(visibility_state.bits.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void Octree::CollectEntities(Array<Entity *> &out) const
{
    out.Reserve(out.Size() + m_nodes.Size());

    for (auto &node : m_nodes) {
        out.PushBack(node.entity);
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntities(out);
        }
    }
}

void Octree::CollectEntitiesInRange(const Vector3 &position, Float radius, Array<Entity *> &out) const
{
    const BoundingBox inclusion_aabb(position - radius, position + radius);

    if (!inclusion_aabb.Intersects(m_aabb)) {
        return;
    }

    out.Reserve(out.Size() + m_nodes.Size());

    for (const Octree::Node &node : m_nodes) {
        if (inclusion_aabb.Intersects(node.aabb)) {
            out.PushBack(node.entity);
        }
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntitiesInRange(position, radius, out);
        }
    }
}

Bool Octree::GetNearestOctants(const Vector3 &position, FixedArray<Octree *, 8> &out) const
{
    if (!m_aabb.ContainsPoint(position)) {
        return false;
    }

    if (!m_is_divided) {
        return false;
    }

    for (const Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->GetNearestOctants(position, out)) {
            return true;
        }
    }

    for (SizeType i = 0; i < m_octants.Size(); i++) {
        out[i] = m_octants[i].octree.Get();
    }

    return true;
}

Bool Octree::GetNearestOctant(const Vector3 &position, Octree const *&out) const
{
    if (!m_aabb.ContainsPoint(position)) {
        return false;
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree->GetNearestOctant(position, out)) {
                return true;
            }
        }
    }

    out = this;

    return true;
}

Bool Octree::GetFittingOctant(const BoundingBox &aabb, Octree const *&out) const
{
    if (!m_aabb.Contains(aabb)) {
        return false;
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree->GetFittingOctant(aabb, out)) {
                return true;
            }
        }
    }

    out = this;

    return true;
}

void Octree::NextVisibilityState()
{
    AssertThrow(m_root != nullptr);

    UInt8 cursor = m_root->visibility_cursor.fetch_add(1u, std::memory_order_relaxed);
    cursor = (cursor + 1) % VisibilityState::cursor_size;

    // m_visibility_state.snapshots[cursor].bits.store(0u, std::memory_order_relaxed);
    m_visibility_state.snapshots[cursor].nonce++;
}

UInt8 Octree::LoadVisibilityCursor() const
{
    AssertThrow(m_root != nullptr);

    return m_root->visibility_cursor.load(std::memory_order_relaxed) % VisibilityState::cursor_size;
}

UInt8 Octree::LoadPreviousVisibilityCursor() const
{
    AssertThrow(m_root != nullptr);

    return (m_root->visibility_cursor.load(std::memory_order_relaxed) + VisibilityState::cursor_size - 1) % VisibilityState::cursor_size;
}

void Octree::CalculateVisibility(Camera *camera)
{
    AssertThrow(m_root != nullptr);

    if (camera == nullptr) {
        return;
    }

    if (camera->GetID().value - 1 >= VisibilityState::max_visibility_states) {
        DebugLog(
            LogType::Error,
            "Camera ID #%lu out of bounds of octree visibility bitset. Cannot update visibility state.\n",
            camera->GetID().value
        );

        return;
    }

    const Frustum &frustum = camera->GetFrustum();

    if (frustum.ContainsAABB(m_aabb)) {
        const auto cursor = LoadVisibilityCursor();

        UpdateVisibilityState(camera, cursor);
    }
}

void Octree::UpdateVisibilityState(Camera *camera, UInt8 cursor)
{
    /* assume we are already visible from CalculateVisibility() check */
    const Frustum &frustum = camera->GetFrustum();

    m_visibility_state.SetVisible(camera->GetID(), cursor);

    if (m_is_divided) {
        const auto nonce = m_visibility_state.snapshots[cursor].nonce;

        for (auto &octant : m_octants) {
            if (!frustum.ContainsAABB(octant.aabb)) {
                continue;
            }

            // if (nonce != octant.octree->m_visibility_state.snapshots[cursor].nonce.load(std::memory_order_relaxed)) {
            //     // clear it out after first set
            //     octant.octree->m_visibility_state.snapshots[cursor].bits.store(0u, std::memory_order_relaxed);
            // }
            
            octant.octree->m_visibility_state.snapshots[cursor].nonce = nonce;
            octant.octree->UpdateVisibilityState(camera, cursor);
        }
    }
}

void Octree::OnEntityRemoved(Entity *entity)
{
    if (entity == nullptr) {
        return;
    }
    
    if (!Remove(entity)) {
        DebugLog(LogType::Error, "Failed to find Entity #%lu in octree\n", entity->GetID().value);
    }
}

void Octree::ResetNodesHash()
{
    m_nodes_hash = { 0 };
}

void Octree::RebuildNodesHash(UInt level)
{
    ResetNodesHash();

    for (const Node &item : m_nodes) {
        m_nodes_hash.Add(item.GetHashCode());
    }
}

Bool Octree::TestRay(const Ray &ray, RayTestResults &out_results) const
{
    Bool has_hit = false;

    if (ray.TestAABB(m_aabb)) {
        for (const Octree::Node &node : m_nodes) {
            AssertThrow(node.entity != nullptr);

            if (!node.entity->HasFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED)) {
                continue;
            }

            if (!BucketRayTestsEnabled(node.entity->GetRenderableAttributes().GetMaterialAttributes().bucket)) {
                continue;
            }

            if (ray.TestAABB(
                node.aabb,
                node.entity->GetID().value,
                (void *)node.entity,
                out_results
            )) {
                has_hit = true;
            }
        }
        
        if (m_is_divided) {
            for (const Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->TestRay(ray, out_results)) {
                    has_hit = true;
                }
            }
        }
    }

    return has_hit;
}

} // namespace hyperion::v2