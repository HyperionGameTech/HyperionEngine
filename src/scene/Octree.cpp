#include "Octree.hpp"
#include "Entity.hpp"
#include <Engine.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/camera/Camera.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

// @TODO EntityManager remove event listener

const BoundingBox Octree::default_bounds = BoundingBox({ -250.0f }, { 250.0f });

// 0x80 For index bit because we reserve the highest bit for invalid octants
// 0xff for depth because +1 (used for child octant id) will cause it to overflow to 0
const OctantID OctantID::invalid = OctantID(OctantID::invalid_bits, 0xff);

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

Octree::Octree(RC<EntityManager> entity_manager, const BoundingBox &aabb)
    : Octree(std::move(entity_manager), aabb, nullptr, 0)
{
    m_root = new Root;
}

Octree::Octree(RC<EntityManager> entity_manager, const BoundingBox &aabb, Octree *parent, UInt8 index)
    : m_entity_manager(std::move(entity_manager)),
      m_aabb(aabb),
      m_parent(nullptr),
      m_is_divided(false),
      m_root(nullptr),
      m_visibility_state { },
      m_octant_id(index, OctantID::invalid)
{
    if (parent != nullptr) {
        SetParent(parent); // call explicitly to set root ptr
    }

    AssertThrow(m_octant_id.GetIndex() == index);

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

    m_octant_id = OctantID(m_octant_id.GetIndex(), parent != nullptr ? parent->m_octant_id : OctantID::invalid);

    if (IsDivided()) {
        for (Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->SetParent(this);
        }
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
        return m_octants.Every([depth, octant_mask](const Octant &octant)
        {
            if (octant_mask & (1u << octant.octree->m_octant_id.GetIndex())) {
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
                        m_aabb.GetMin() + divided_aabb_dimensions * Vec3f(Float(x), Float(y), Float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vec3f(Float(x), Float(y), Float(z)) + Vec3f(1.0f))
                    )
                };
            }
        }
    }
}

Octree *Octree::GetChildOctant(OctantID octant_id)
{
    if (octant_id == OctantID::invalid) {
#if HYP_OCTREE_DEBUG
        DebugLog(
            LogType::Warn,
            "Invalid octant id %u:%u: Octant is invalid\n",
            octant_id.GetDepth(),
            octant_id.GetIndex()
        );
#endif

        return nullptr;
    }

    if (octant_id == m_octant_id) {
        return this;
    }

    if (octant_id.depth <= m_octant_id.depth) {
#if HYP_OCTREE_DEBUG
        DebugLog(
            LogType::Warn,
            "Octant id %u:%u is not a child of %u:%u: Octant is not deep enough\n",
            octant_id.GetDepth(),
            octant_id.GetIndex(),
            m_octant_id.GetDepth(),
            m_octant_id.GetIndex()
        );
#endif

        return nullptr;
    }

    Octree *current = this;

    for (UInt depth = m_octant_id.depth + 1; depth <= octant_id.depth; depth++) {
        const UInt8 index = octant_id.GetIndex(depth);

        if (!current || !current->IsDivided()) {
#if HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Warn,
                "Octant id %u:%u is not a child of %u:%u: Octant %u:%u is not divided",
                octant_id.GetDepth(),
                octant_id.GetIndex(),
                m_octant_id.GetDepth(),
                m_octant_id.GetIndex(),
                current ? current->m_octant_id.GetDepth() : ~0u,
                current ? current->m_octant_id.GetIndex() : ~0u
            );
#endif

            return nullptr;
        }

        current = current->m_octants[index].octree.Get();
    }

    return current;
}

void Octree::Divide()
{
    AssertThrow(!IsDivided());

    for (UInt i = 0; i < 8; ++i) {
        Octant &octant = m_octants[i];
        AssertThrow(octant.octree == nullptr);

        octant.octree.Reset(new Octree(m_entity_manager, octant.aabb, this, UInt8(i)));
    }

    m_is_divided = true;
    RebuildNodesHash();
}

void Octree::Undivide()
{
    AssertThrow(IsDivided());
    AssertThrowMsg(m_nodes.Empty(), "Undivide() should be called on octrees with no remaining nodes");

    for (Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->IsDivided()) {
            octant.octree->Undivide();
        }

        octant.octree.Reset();
    }

    m_is_divided = false;
}

void Octree::CollapseParents()
{
    if (IsDivided() || !Empty()) {
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

    if (IsDivided()) {
        Undivide();
    }
}

void Octree::ClearInternal(Array<Node> &out_nodes)
{
    for (auto &node : m_nodes) {
        // node.entity->OnRemovedFromOctree(this);

        if (m_entity_manager) {
            m_entity_manager->PushCommand([id = node.id, octant_id = m_octant_id](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                if (mgr.HasEntity(id)) {
                    mgr.GetComponent<VisibilityStateComponent>(id).octant_id = OctantID::invalid;
                }
            });
        }

        if (m_root != nullptr) {
            auto it = m_root->node_to_octree.Find(node.id);

            if (it != m_root->node_to_octree.End()) {
                m_root->node_to_octree.Erase(it);
            }
        }

        out_nodes.PushBack(std::move(node));
    }

    m_nodes.Clear();

    if (!m_is_divided) {
        return;
    }

    for (Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        octant.octree->ClearInternal(out_nodes);
    }
}

Octree::InsertResult Octree::Insert(ID<Entity> id, const BoundingBox &aabb)
{
    if (!m_aabb.Contains(aabb)) {
        auto rebuild_result = RebuildExtendInternal(aabb);

        if (!rebuild_result.first) {
            DebugLog(
                LogType::Warn,
                "Failed to rebuild octree when inserting entity #%lu\n",
                id.Value()
            );

            return rebuild_result;
        }
    }

    // stop recursing if we are at max depth
    if (m_octant_id.depth < OctantID::max_depth) {
        for (Octant &octant : m_octants) {
            if (octant.aabb.Contains(aabb)) {
                if (!IsDivided()) {
                    Divide();
                }

                AssertThrow(octant.octree != nullptr);

                const Octree::InsertResult insert_result = octant.octree->Insert(id, aabb);

                RebuildNodesHash();

                return insert_result;
            }
        }
    }

    return InsertInternal(id, aabb);
}

Octree::InsertResult Octree::InsertInternal(ID<Entity> id, const BoundingBox &aabb)
{
    m_nodes.PushBack(Node { id, aabb });

    if (m_root != nullptr) {
        AssertThrowMsg(m_root->node_to_octree.Find(id) == m_root->node_to_octree.End(), "Entity must not already be in octree hierarchy.");

        m_root->node_to_octree[id] = this;
    }

    // entity->OnAddedToOctree(this);

    if (m_entity_manager) {
        m_entity_manager->PushCommand([id, octant_id = m_octant_id](EntityManager &mgr, GameCounter::TickUnit delta)
        {
            if (mgr.HasEntity(id)) {
                if (!mgr.HasComponent<VisibilityStateComponent>(id)) {
                    mgr.AddComponent<VisibilityStateComponent>(id, {
                        .octant_id = octant_id
                    });

                    DebugLog(
                        LogType::Warn,
                        "Entity #%lu octant_id was not set, so it was set to %u:%u\n",
                        id.Value(),
                        octant_id.GetDepth(),
                        octant_id.GetIndex()
                    );
                } else {
                    mgr.GetComponent<VisibilityStateComponent>(id).octant_id = octant_id;

                    DebugLog(
                        LogType::Debug,
                        "Entity #%lu octant_id was set to %u:%u\n",
                        id.Value(),
                        octant_id.GetDepth(),
                        octant_id.GetIndex()
                    );
                }
            }
        });
        
    }

    RebuildNodesHash();

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::Result Octree::Remove(ID<Entity> id)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.Find(id);

        if (it == m_root->node_to_octree.End()) {
            return { Result::OCTREE_ERR, "Not found in node map" };
        }

        if (auto *octree = it->second) {
            return octree->RemoveInternal(id);
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants" };
    }

    return RemoveInternal(id);
}

Octree::Result Octree::RemoveInternal(ID<Entity> id)
{
    const auto it = FindNode(id);

    if (it == m_nodes.End()) {
        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->RemoveInternal(id)) {
                    return { };
                }
            }
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants" };
    }

    if (m_root != nullptr) {
        m_root->node_to_octree.Erase(id);
    }

    // entity->OnRemovedFromOctree(this);

    // @TODO Update once everything is moved to ECS
    if (m_entity_manager) {
        m_entity_manager->PushCommand([id](EntityManager &mgr, GameCounter::TickUnit delta)
        {
            mgr.GetComponent<VisibilityStateComponent>(id).octant_id = OctantID::invalid;
        });
    }

    m_nodes.Erase(it);
    RebuildNodesHash();

    if (!m_is_divided && m_nodes.Empty()) {
        Octree *last_empty_parent = nullptr;

        if (Octree *parent = m_parent) {
            const Octree *child = this;

            while (parent->EmptyDeep(DEPTH_SEARCH_INF, 0xff & ~(1 << child->m_octant_id.GetIndex()))) { // do not search this branch of the tree again
                last_empty_parent = parent;

                if (parent->m_parent == nullptr) {
                    break;
                }

                child = parent;
                parent = child->m_parent;
            }
        }

        if (last_empty_parent != nullptr) {
            AssertThrow(last_empty_parent->EmptyDeep(DEPTH_SEARCH_INF));

            /* At highest empty parent octant, call Undivide() to collapse nodes */
            last_empty_parent->Undivide();
        }
    }

    return { };
}

Octree::InsertResult Octree::Move(ID<Entity> id, const BoundingBox &aabb, const Array<Node>::Iterator *it)
{
    const BoundingBox &new_aabb = aabb;

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
                id.Value()
            );
#endif

            return RebuildExtendInternal(new_aabb);
        }

        // not root
#if HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Debug,
                "Moving entity #%lu into the closest fitting (or root) parent\n",
                id.Value()
            );
#endif


        Optional<InsertResult> parent_insert_result;

        /* Contains is false at this point */
        Octree *parent = m_parent;
        Octree *last_parent = m_parent;

        while (parent != nullptr) {
            last_parent = parent;

            if (parent->m_aabb.Contains(new_aabb)) {
                
                if (it != nullptr) {
                    if (m_root != nullptr) {
                        m_root->node_to_octree.Erase(id);
                    }

                    m_nodes.Erase(*it);
                    RebuildNodesHash();
                }

                parent_insert_result = parent->Move(id, aabb);

                break;
            }

            parent = parent->m_parent;
        }

        if (parent_insert_result.HasValue()) { // succesfully inserted, safe to call CollapseParents()
            /* Node has now been added to it's appropriate octant -- remove any potential empty octants */
            CollapseParents();

            return parent_insert_result.Get();
        }

        // not inserted because no Move() was called on parents (because they don't contain AABB),
        // have to _manually_ call Move() which will go to the above branch for the parent octant,
        // this invalidating `this`

#if HYP_OCTREE_DEBUG
        DebugLog(
            LogType::Debug,
            "In child, no parents contain AABB so calling Move() on last valid octant (root). This will invalidate `this`.. %lu\n",
            id.Value()
        );
#endif

        AssertThrow(last_parent != nullptr);

        return last_parent->Move(id, aabb);
    }

    // CONTAINS AABB HERE

    if (m_octant_id.GetDepth() < OctantID::max_depth) {
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
                        m_root->node_to_octree.Erase(id);
                    }

                    m_nodes.Erase(*it);
                }
                
                if (!IsDivided()) {
                    Divide();
                }
                
                AssertThrow(octant.octree != nullptr);

                const auto octant_move_result = octant.octree->Move(id, aabb, nullptr);
                AssertThrow(octant_move_result.first);

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
        ForceVisibilityStates();

        m_nodes.PushBack(Node { id, aabb });

        if (m_root != nullptr) {
            AssertThrowMsg(m_root->node_to_octree.Find(id) == m_root->node_to_octree.End(), "Entity must not already be in octree hierarchy.");

            m_root->node_to_octree[id] = this;
        }

        // entity->OnMovedToOctant(this);

        if (m_entity_manager) {
            m_entity_manager->PushCommand([id, octant_id = m_octant_id](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                if (mgr.HasEntity(id)) {
                    mgr.GetComponent<VisibilityStateComponent>(id).octant_id = octant_id;
                }
            });

#ifdef HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Debug,
                "Entity #%lu octant_id was moved to %u:%u\n",
                id.Value(),
                m_octant_id.GetDepth(),
                m_octant_id.GetIndex()
            );
#endif
        }
    }

    RebuildNodesHash();
    
    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::InsertResult Octree::Update(ID<Entity> id, const BoundingBox &aabb)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.Find(id);

        if (it == m_root->node_to_octree.End()) {
            return {
                { Result::OCTREE_ERR, "Object not found in node map!" },
                OctantID::invalid
            };
        }

        if (Octree *octree = it->second) {
            return octree->UpdateInternal(id, aabb);
        }

        return {
            { Result::OCTREE_ERR, "Object has no octree in node map!" },
            OctantID::invalid
        };
    }

    return UpdateInternal(id, aabb);
}

Octree::InsertResult Octree::UpdateInternal(ID<Entity> id, const BoundingBox &aabb)
{
    const auto it = FindNode(id);

    if (it == m_nodes.End()) {
        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                auto update_internal_result = octant.octree->UpdateInternal(id, aabb);

                if (update_internal_result.first) {
                    return update_internal_result;
                }
            }
        }

        return {
            { Result::OCTREE_ERR, "Could not update in any sub octants" },
            OctantID::invalid
        };
    }

    const BoundingBox &new_aabb = aabb;
    const BoundingBox &old_aabb = it->aabb;

    if (new_aabb == old_aabb) {
        /* AABB has not changed - no need to update */
        return {
            { Result::OCTREE_OK },
            m_octant_id
        };
    }

    /* AABB has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    return Move(id, new_aabb, &it);
}

Octree::InsertResult Octree::Rebuild(const BoundingBox &new_aabb)
{
    DebugLog(LogType::Debug, "Rebuild octree\n");

    Array<Node> new_nodes;
    Clear(new_nodes);

    m_aabb = new_aabb;

    for (auto &node : new_nodes) {
        auto insert_result = Insert(node.id, node.aabb);

        if (!insert_result.first) {
            return insert_result;
        }
    }
    
    // force all entities visible to prevent flickering
    ForceVisibilityStates();

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::InsertResult Octree::RebuildExtendInternal(const BoundingBox &extend_include_aabb)
{
#if HYP_OCTREE_DEBUG
    DebugLog(
        LogType::Debug,
        "RebuildExtendInternal: %f,%f,%f\t%f,%f,%f\n",
        extend_include_aabb.min.x, extend_include_aabb.min.y, extend_include_aabb.min.z,
        extend_include_aabb.max.x, extend_include_aabb.max.y, extend_include_aabb.max.z
    );
#endif

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
        if (m_entity_manager) {
            m_entity_manager->PushCommand([id = node.id, octant_id = m_octant_id](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                if (mgr.HasEntity(id)) {
                    mgr.GetComponent<VisibilityStateComponent>(id).octant_id = octant_id;
                }
            });
        }
    }

    if (m_is_divided) {
        for (Octant &octant : m_octants) {
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

void Octree::CollectEntities(Array<ID<Entity>> &out) const
{
    out.Reserve(out.Size() + m_nodes.Size());

    for (auto &node : m_nodes) {
        out.PushBack(node.id);
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntities(out);
        }
    }
}

void Octree::CollectEntitiesInRange(const Vector3 &position, Float radius, Array<ID<Entity>> &out) const
{
    const BoundingBox inclusion_aabb(position - radius, position + radius);

    if (!inclusion_aabb.Intersects(m_aabb)) {
        return;
    }

    out.Reserve(out.Size() + m_nodes.Size());

    for (const Octree::Node &node : m_nodes) {
        if (inclusion_aabb.Intersects(node.aabb)) {
            out.PushBack(node.id);
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
    if (camera == nullptr) {
        return;
    }

    if (camera->GetID().ToIndex() >= VisibilityState::max_visibility_states) {
        DebugLog(
            LogType::Error,
            "Camera ID #%lu out of bounds of octree visibility bitset. Cannot update visibility state.\n",
            camera->GetID().Value()
        );

        return;
    }

    const Frustum &frustum = camera->GetFrustum();

    if (frustum.ContainsAABB(m_aabb)) {
        const UInt8 cursor = LoadVisibilityCursor();

        UpdateVisibilityState(camera, cursor);
    }
}

void Octree::UpdateVisibilityState(Camera *camera, UInt8 cursor)
{
    /* assume we are already visible from CalculateVisibility() check */
    const Frustum &frustum = camera->GetFrustum();

    m_visibility_state.SetVisible(camera->GetID(), cursor);

    if (m_is_divided) {
        const VisibilityStateSnapshot::Nonce nonce = m_visibility_state.snapshots[cursor].nonce;

        for (Octant &octant : m_octants) {
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

// void Octree::OnEntityRemoved(Entity *entity)
// {
//     if (entity == nullptr) {
//         return;
//     }
    
//     if (!Remove(entity)) {
//         DebugLog(LogType::Error, "Failed to find Entity #%lu in octree\n", entity->GetID().value);
//     }
// }

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
            // if (!node.entity->HasFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED)) {
            //     continue;
            // }

            // if (!BucketRayTestsEnabled(node.entity->GetRenderableAttributes().GetMaterialAttributes().bucket)) {
            //     continue;
            // }

            if (ray.TestAABB(
                node.aabb,
                node.id.Value(),
                nullptr,
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