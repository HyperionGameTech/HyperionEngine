#include "Octree.hpp"
#include "Entity.hpp"
#include "../Engine.hpp"
#include <Threads.hpp>

namespace hyperion::v2 {

const BoundingBox Octree::default_bounds = BoundingBox({ -250.0f }, { 250.0f });

bool Octree::IsVisible(
    const Octree *root,
    const Octree *child
)
{
    return child->m_visibility_state.ValidToParent(
        root->m_visibility_state,
        (root->GetRoot()->visibility_cursor.load(std::memory_order_relaxed) + VisibilityState::cursor_size - 1) % VisibilityState::cursor_size
    );
}

bool Octree::IsVisible(
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

    AssertThrowMsg(m_nodes.empty(), "Expected nodes to be emptied before octree destructor");
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

bool Octree::EmptyDeep(int depth, UInt8 octant_mask) const
{
    if (!Empty()) {
        return false;
    }

    if (!m_is_divided) {
        return true;
    }

    if (depth != 0) {
        return std::all_of(m_octants.begin(), m_octants.end(), [depth, octant_mask](const Octant &octant) {
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

    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            for (int z = 0; z < 2; z++) {
                int index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + divided_aabb_dimensions * Vector3(float(x), float(y), float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vector3(float(x), float(y), float(z)) + Vector3(1.0f))
                    )
                };
            }
        }
    }
}

void Octree::Divide()
{
    AssertThrow(!m_is_divided);

    for (SizeType i = 0; i < m_octants.size(); i++) {
        auto &octant = m_octants[i];

        AssertThrow(octant.octree == nullptr);

        octant.octree.reset(new Octree(this, octant.aabb, static_cast<UInt8>(i)));

        if (m_root != nullptr) {
            m_root->events.on_insert_octant(octant.octree.get(), nullptr);
        }
    }

    m_is_divided = true;
}

void Octree::Undivide()
{
    AssertThrow(m_is_divided);
    AssertThrowMsg(m_nodes.empty(), "Undivide() should be called on octrees with no remaining nodes");

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->m_is_divided) {
            octant.octree->Undivide();
        }
        
        if (m_root != nullptr) {
            m_root->events.on_remove_octant(octant.octree.get(), nullptr);
        }

        octant.octree.reset();
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

            if (octant.octree.get() == highest_empty) {
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
    std::vector<Node> nodes;

    Clear(nodes);
}

void Octree::Clear(std::vector<Node> &out_nodes)
{
    ClearInternal(out_nodes);

    if (m_is_divided) {
        Undivide();
    }
}

void Octree::ClearInternal(std::vector<Node> &out_nodes)
{
    for (auto &node : m_nodes) {
        node.entity->OnRemovedFromOctree(this);

        if (m_root != nullptr) {
            auto it = m_root->node_to_octree.find(node.entity);

            if (it != m_root->node_to_octree.end()) {
                m_root->node_to_octree.erase(it);
            }
        }

        out_nodes.push_back(std::move(node));
    }

    m_nodes.clear();

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

    const auto &entity_aabb = entity->GetWorldAABB();

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

                return octant.octree->Insert(entity);
            }
        }
    }

    return InsertInternal(entity);
}

Octree::Result Octree::InsertInternal(Entity *entity)
{
    m_nodes.push_back(Node {
        .entity = entity,
        .aabb = entity->GetWorldAABB()
    });

    if (m_root != nullptr) {
        AssertThrowMsg(m_root->node_to_octree.find(entity) == m_root->node_to_octree.end(), "Entity must not already be in octree hierarchy.");

        m_root->node_to_octree[entity] = this;
        m_root->events.on_insert_node(this, entity);
    }

    entity->OnAddedToOctree(this);

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
            m_root->node_to_octree.erase(it);

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

    if (it == m_nodes.end()) {
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
        m_root->events.on_remove_node(this, entity);
        m_root->node_to_octree.erase(entity);
    }

    entity->OnRemovedFromOctree(this);

    m_nodes.erase(it);

    if (!m_is_divided && m_nodes.empty()) {
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

Octree::Result Octree::Move(Entity *entity, const std::vector<Node>::iterator *it)
{
    const auto &new_aabb = entity->GetWorldAABB();

    const bool is_root = IsRoot();
    const bool contains = m_aabb.Contains(new_aabb);

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


        bool inserted = false;

        /* Contains is false at this point */
        Octree *parent = m_parent;
        Octree *last_parent = m_parent;

        while (parent != nullptr) {
            last_parent = parent;

            if (parent->m_aabb.Contains(new_aabb)) {
                
                if (it != nullptr) {
                    if (m_root != nullptr) {
                        m_root->events.on_remove_node(this, entity);
                        m_root->node_to_octree.erase(entity);
                    }

                    m_nodes.erase(*it);
                }


                inserted = bool(parent->Move(entity));

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
                        m_root->events.on_remove_node(this, entity);
                        m_root->node_to_octree.erase(entity);
                    }

                    m_nodes.erase(*it);
                }

                if (!m_is_divided) {
                    Divide();
                }
                
                AssertThrow(octant.octree != nullptr);

                const auto octant_move_result = octant.octree->Move(entity, nullptr);
                AssertThrow(octant_move_result);

                return octant_move_result;
            }
        }
    }

    if (it != nullptr) { /* Not moved */
        auto &entity_it = *it;

        entity_it->aabb = new_aabb;
    } else { /* Moved into new octant */
        // force this octant to be visible to prevent flickering
        CopyVisibilityState(Engine::Get()->GetWorld()->GetOctree().GetVisibilityState());

        m_nodes.push_back(Node {
            .entity = entity,
            .aabb = entity->GetWorldAABB()
        });

        if (m_root != nullptr) {
            AssertThrowMsg(m_root->node_to_octree.find(entity) == m_root->node_to_octree.end(), "Entity must not already be in octree hierarchy.");

            m_root->node_to_octree[entity] = this;
            m_root->events.on_insert_node(this, entity);
        }

        entity->OnMovedToOctant(this);
    }
    
    return {};
}

Octree::Result Octree::Update(Entity *entity)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(entity);

        if (it == m_root->node_to_octree.end()) {
            return {Result::OCTREE_ERR, "Object not found in node map!"};
        }

        if (auto *octree = it->second) {
            return octree->UpdateInternal(entity);
        }

        return {Result::OCTREE_ERR, "Object has no octree in node map!"};
    }

    return UpdateInternal(entity);
}

Octree::Result Octree::UpdateInternal(Entity *entity)
{
    const auto it = FindNode(entity);

    if (it == m_nodes.end()) {
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->UpdateInternal(entity)) {
                    return {};
                }
            }
        }

        return {Result::OCTREE_ERR, "Could not update in any sub octants"};
    }

    const auto &new_aabb = entity->GetWorldAABB();
    const auto &old_aabb = it->aabb;

    if (new_aabb == old_aabb) {
        /* AABB has not changed - no need to update */
        return {};
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

    std::vector<Node> new_nodes;
    Clear(new_nodes);

    m_aabb = new_aabb;

    for (auto &node : new_nodes) {
        if (auto *entity = node.entity) {
            if (entity->GetScene() == nullptr) {
                DebugLog(
                    LogType::Error,
                    "Entity #%u was not attached to a Scene!\n",
                    entity->GetID().value
                );

                continue;
            }

            auto insert_result = Insert(node.entity);

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

        m_visibility_state.snapshots[i].bits.fetch_or(visibility_state.snapshots[i].bits.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_visibility_state.snapshots[i].nonce.store(visibility_state.snapshots[i].nonce.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    
    // m_visibility_state.bits.fetch_or(visibility_state.bits.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void Octree::CollectEntities(std::vector<Entity *> &out) const
{
    out.reserve(out.size() + m_nodes.size());

    for (auto &node : m_nodes) {
        out.push_back(node.entity);
    }

    if (m_is_divided) {
        for (auto &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntities(out);
        }
    }
}

void Octree::CollectEntitiesInRange(const Vector3 &position, float radius, std::vector<Entity *> &out) const
{
    const BoundingBox inclusion_aabb(position - radius, position + radius);

    if (!inclusion_aabb.Intersects(m_aabb)) {
        return;
    }

    out.reserve(out.size() + m_nodes.size());

    for (auto &node : m_nodes) {
        if (inclusion_aabb.Intersects(node.aabb)) {
            out.push_back(node.entity);
        }
    }

    if (m_is_divided) {
        for (auto &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntitiesInRange(position, radius, out);
        }
    }
}

bool Octree::GetNearestOctants(const Vector3 &position, std::array<Octree *, 8> &out) const
{
    if (!m_aabb.ContainsPoint(position)) {
        return false;
    }

    if (!m_is_divided) {
        return false;
    }

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->GetNearestOctants(position, out)) {
            return true;
        }
    }

    for (SizeType i = 0; i < m_octants.size(); i++) {
        out[i] = m_octants[i].octree.get();
    }

    return true;
}

void Octree::NextVisibilityState()
{
    AssertThrow(m_root != nullptr);

    UInt8 cursor = m_root->visibility_cursor.fetch_add(1u, std::memory_order_relaxed);
    cursor = (cursor + 1) % VisibilityState::cursor_size;

    // m_visibility_state.snapshots[cursor].bits.store(0u, std::memory_order_relaxed);
    m_visibility_state.snapshots[cursor].nonce.fetch_add(1u, std::memory_order_relaxed);
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

void Octree::CalculateVisibility(Scene *scene)
{
    AssertThrow(m_root != nullptr);

    if (scene == nullptr) {
        return;
    }

    if (scene->GetID().value - 1 >= VisibilityState::max_scenes) {
        DebugLog(
            LogType::Error,
            "Scene #%lu out of bounds of octree scene visibility bitset. Cannot update visibility state.\n",
            scene->GetID().value
        );

        return;
    }

    const auto &frustum = scene->GetCamera()->GetFrustum();

    if (frustum.ContainsAABB(m_aabb)) {
        const auto cursor = LoadVisibilityCursor();

        UpdateVisibilityState(scene, cursor);
    }
}

void Octree::UpdateVisibilityState(Scene *scene, UInt8 cursor)
{
    /* assume we are already visible from CalculateVisibility() check */
    const auto &frustum = scene->GetCamera()->GetFrustum();

    m_visibility_state.SetVisible(scene->GetID(), cursor);

    if (m_is_divided) {
        const auto nonce = m_visibility_state.snapshots[cursor].nonce.load(std::memory_order_relaxed);

        for (auto &octant : m_octants) {
            if (!frustum.ContainsAABB(octant.aabb)) {
                continue;
            }

            // if (nonce != octant.octree->m_visibility_state.snapshots[cursor].nonce.load(std::memory_order_relaxed)) {
            //     // clear it out after first set
            //     octant.octree->m_visibility_state.snapshots[cursor].bits.store(0u, std::memory_order_relaxed);
            // }
            
            octant.octree->m_visibility_state.snapshots[cursor].nonce.store(nonce, std::memory_order_relaxed);
            octant.octree->UpdateVisibilityState(scene, cursor);
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

bool Octree::TestRay(const Ray &ray, RayTestResults &out_results) const
{
    bool has_hit = false;

    if (ray.TestAABB(m_aabb)) {
        for (auto &node : m_nodes) {
            AssertThrow(node.entity != nullptr);

            if (!node.entity->HasFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED)) {
                continue;
            }

            if (!BucketRayTestsEnabled(node.entity->GetRenderableAttributes().material_attributes.bucket)) {
                continue;
            }

            if (ray.TestAABB(
                node.aabb,
                node.entity->GetID().value,
                static_cast<void *>(node.entity),
                out_results
            )) {
                has_hit = true;
            }
        }
        
        if (m_is_divided) {
            for (auto &octant : m_octants) {
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