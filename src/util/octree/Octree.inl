template <class Derived, class TEntry>
const BoundingBox OctreeBase<Derived, TEntry>::default_bounds = BoundingBox({ -250.0f }, { 250.0f });

template <class Derived, class TEntry>
void OctreeState<Derived, TEntry>::MarkOctantDirty(OctantId octant_id)
{
    const OctantId prev_state = rebuild_state;

    if (octant_id.IsInvalid())
    {
        return;
    }

    if (rebuild_state.IsInvalid())
    {
        rebuild_state = octant_id;

        return;
    }

    while (octant_id != rebuild_state && !octant_id.IsChildOf(rebuild_state) && !rebuild_state.IsInvalid())
    {
        rebuild_state = rebuild_state.GetParent();
    }

    // should always end up at root if it doesnt match any
    AssertThrow(rebuild_state != OctantId::Invalid());
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::OctreeBase()
    : OctreeBase(default_bounds)
{
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::OctreeBase(const BoundingBox& aabb)
    : OctreeBase(aabb, nullptr, 0)
{
    m_state = new OctreeState<Derived, TEntry>();
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::OctreeBase(const BoundingBox& aabb, OctreeBase* parent, uint8 index)
    : m_aabb(aabb),
      m_parent(nullptr),
      m_is_divided(false),
      m_state(nullptr),
      m_octant_id(index, OctantId::Invalid()),
      m_invalidation_marker(0)
{
    if (parent != nullptr)
    {
        SetParent(parent); // call explicitly to set root ptr
    }

    AssertThrow(m_octant_id.GetIndex() == index);

    InitOctants();
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::~OctreeBase()
{
    Clear();

    if (IsRoot())
    {
        delete m_state;
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::SetParent(OctreeBase* parent)
{
    m_parent = parent;

    if (m_parent != nullptr)
    {
        m_state = m_parent->m_state;
    }
    else
    {
        m_state = nullptr;
    }

    m_octant_id = OctantId(m_octant_id.GetIndex(), parent != nullptr ? parent->m_octant_id : OctantId::Invalid());

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->SetParent(this);
        }
    }
}

template <class Derived, class TEntry>
bool OctreeBase<Derived, TEntry>::EmptyDeep(int depth, uint8 octant_mask) const
{
    if (!Empty())
    {
        return false;
    }

    if (!m_is_divided)
    {
        return true;
    }

    if (depth != 0)
    {
        return Every(m_octants, [depth, octant_mask](const Octant& octant)
            {
                if (octant_mask & (1u << octant.octree->m_octant_id.GetIndex()))
                {
                    return octant.octree->EmptyDeep(depth - 1);
                }

                return true;
            });
    }

    return true;
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::InitOctants()
{
    const Vec3f divided_aabb_dimensions = m_aabb.GetExtent() / 2.0f;

    for (uint32 x = 0; x < 2; x++)
    {
        for (uint32 y = 0; y < 2; y++)
        {
            for (uint32 z = 0; z < 2; z++)
            {
                const uint32 index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + divided_aabb_dimensions * Vec3f(float(x), float(y), float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vec3f(float(x), float(y), float(z)) + Vec3f(1.0f)))
                };
            }
        }
    }
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>* OctreeBase<Derived, TEntry>::GetChildOctant(OctantId octant_id)
{
    if (octant_id == OctantId::Invalid())
    {
        return nullptr;
    }

    if (octant_id == m_octant_id)
    {
        return this;
    }

    if (octant_id.depth <= m_octant_id.depth)
    {
        return nullptr;
    }

    OctreeBase* current = this;

    for (uint32 depth = m_octant_id.depth + 1; depth <= octant_id.depth; depth++)
    {
        const uint8 index = octant_id.GetIndex(depth);

        if (!current || !current->IsDivided())
        {
#ifdef HYP_OCTREE_DEBUG
            HYP_DEBUG(OctreeBase, Warning,
                "Octant id {}:{} is not a child of {}:{}: Octant {}:{} is not divided",
                octant_id.GetDepth(), octant_id.GetIndex(),
                m_octant_id.GetDepth(), m_octant_id.GetIndex(),
                current ? current->m_octant_id.GetDepth() : ~0u,
                current ? current->m_octant_id.GetIndex() : ~0u);
#endif

            return nullptr;
        }

        current = current->m_octants[index].octree.Get();
    }

    return current;
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Divide()
{
    AssertThrow(!IsDivided());

    for (int i = 0; i < 8; i++)
    {
        Octant& octant = m_octants[i];
        AssertThrow(octant.octree == nullptr);

        octant.octree = CreateChildOctant(octant.aabb, static_cast<Derived*>(this), uint8(i));
    }

    m_is_divided = true;
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Undivide()
{
    AssertThrow(IsDivided());
    AssertThrowMsg(m_entries.Empty(), "Undivide() should be called on octrees with no remaining entries");

    for (Octant& octant : m_octants)
    {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->IsDivided())
        {
            octant.octree->Undivide();
        }

        octant.octree.Reset();
    }

    m_is_divided = false;
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Invalidate()
{
    m_invalidation_marker++;

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->Invalidate();
        }
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::CollapseParents(bool allow_rebuild)
{
    m_state->MarkOctantDirty(m_octant_id);

    if (IsDivided() || !Empty())
    {
        return;
    }

    OctreeBase *iteration = m_parent,
               *highest_empty = nullptr;

    while (iteration != nullptr && iteration->Empty())
    {
        for (const Octant& octant : iteration->m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree.Get() == highest_empty)
            {
                /* Do not perform a check on our entry, as we've already
                 * checked it by going up the chain.
                 * As `iter` becomes the parent of this entry we're currently working with,
                 * we will not have to perform duplicate EmptyDeep() checks on any octants because of this check. */
                continue;
            }

            if (!octant.octree->EmptyDeep())
            {
                goto undivide;
            }
        }

        highest_empty = iteration;
        iteration = iteration->m_parent;
    }

undivide:
    if (highest_empty != nullptr)
    {
        if (allow_rebuild)
        {
            highest_empty->Undivide();
        }
        else
        {
            m_state->MarkOctantDirty(highest_empty->GetOctantID());
        }
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Clear()
{
    Array<Entry> entries;
    Clear(entries, /* undivide */ true);
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Clear(Array<Entry>& out_entries, bool undivide)
{
    out_entries.Reserve(out_entries.Size() + m_entries.Size());

    for (Entry& entry : m_entries)
    {
        if (m_state != nullptr)
        {
            auto entry_to_octree_it = m_state->entry_to_octree.Find(entry.value);

            AssertThrow(entry_to_octree_it != m_state->entry_to_octree.End());
            AssertThrow(entry_to_octree_it->second == this);

            m_state->entry_to_octree.Erase(entry_to_octree_it);
        }

        out_entries.PushBack(std::move(entry));
    }

    m_entries.Clear();

    if (!m_is_divided)
    {
        return;
    }

    for (Octant& octant : m_octants)
    {
        AssertThrow(octant.octree != nullptr);

        octant.octree->Clear(out_entries, /* undivide */ false);
    }

    if (undivide && IsDivided())
    {
        Undivide();
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Clear(Array<TEntry>& out_entries, bool undivide)
{
    out_entries.Reserve(out_entries.Size() + m_entries.Size());

    for (Entry& entry : m_entries)
    {
        if (m_state != nullptr)
        {
            auto entry_to_octree_it = m_state->entry_to_octree.Find(entry.value);

            AssertThrow(entry_to_octree_it != m_state->entry_to_octree.End());
            AssertThrow(entry_to_octree_it->second == this);

            m_state->entry_to_octree.Erase(entry_to_octree_it);
        }

        out_entries.PushBack(std::move(entry.value));
    }

    m_entries.Clear();

    if (!m_is_divided)
    {
        return;
    }

    for (Octant& octant : m_octants)
    {
        AssertThrow(octant.octree != nullptr);

        octant.octree->Clear(out_entries, /* undivide */ false);
    }

    if (undivide && IsDivided())
    {
        Undivide();
    }
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Insert(const TEntry& value, const BoundingBox& aabb, bool allow_rebuild)
{
    if (aabb.IsValid() && aabb.IsFinite())
    {
        if (allow_rebuild)
        {
            if (!m_aabb.Contains(aabb))
            {
                auto rebuild_result = RebuildExtend_Internal(aabb);

                if (!rebuild_result.first)
                {
                    return rebuild_result;
                }
            }
        }

        // stop recursing if we are at max depth
        if (m_octant_id.GetDepth() < OctantId::max_depth - 1)
        {
            for (Octant& octant : m_octants)
            {
                if (octant.aabb.Contains(aabb))
                {
                    if (!IsDivided())
                    {
                        if (allow_rebuild)
                        {
                            Divide();
                        }
                        else
                        {
                            // do not use this octant if it has not been divided yet.
                            // instead, we'll insert into the current octant and mark it dirty
                            // so it will get added after the fact.
                            continue;
                        }
                    }

                    AssertThrow(octant.octree != nullptr);

                    return octant.octree->Insert(value, aabb, allow_rebuild);
                }
            }
        }
    }

    m_state->MarkOctantDirty(m_octant_id);

    return Insert_Internal(value, aabb);
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Insert_Internal(const TEntry& value, const BoundingBox& aabb)
{
    m_entries.Set(Entry { value, aabb });

    if (m_state != nullptr)
    {
        AssertThrowMsg(m_state->entry_to_octree.Find(value) == m_state->entry_to_octree.End(), "Entry must not already be in octree hierarchy.");

        m_state->entry_to_octree.Insert(value, this);
    }

    return {
        { OctreeBase<Derived, TEntry>::Result::OCTREE_OK },
        m_octant_id
    };
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::Result OctreeBase<Derived, TEntry>::Remove(const TEntry& value, bool allow_rebuild)
{
    if (m_state != nullptr)
    {
        const auto it = m_state->entry_to_octree.Find(value);

        if (it == m_state->entry_to_octree.End())
        {
            HYP_BREAKPOINT;
            return { Result::OCTREE_ERR, "Not found in entry map" };
        }

        if (OctreeBase* octant = it->second)
        {
            return octant->Remove_Internal(value, allow_rebuild);
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants" };
    }

    return Remove_Internal(value, allow_rebuild);
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::Result OctreeBase<Derived, TEntry>::Remove_Internal(const TEntry& value, bool allow_rebuild)
{
    const auto it = m_entries.Find(value);

    if (it == m_entries.End())
    {
        if (m_is_divided)
        {
            for (Octant& octant : m_octants)
            {
                AssertThrow(octant.octree != nullptr);

                if (Result octant_result = octant.octree->Remove_Internal(value, allow_rebuild))
                {
                    return octant_result;
                }
            }
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants and not found in this octant" };
    }

    if (m_state != nullptr)
    {
        auto entry_to_octree_it = m_state->entry_to_octree.Find(value);

        AssertThrow(entry_to_octree_it != m_state->entry_to_octree.End());
        AssertThrow(entry_to_octree_it->second == this);

        m_state->entry_to_octree.Erase(entry_to_octree_it);
    }

    m_entries.Erase(it);

    m_state->MarkOctantDirty(m_octant_id);

    if (!m_is_divided && m_entries.Empty())
    {
        OctreeBase* last_empty_parent = nullptr;

        if (OctreeBase* parent = m_parent)
        {
            const OctreeBase* child = this;

            while (parent->EmptyDeep(DEPTH_SEARCH_INF, 0xff & ~(1 << child->m_octant_id.GetIndex())))
            { // do not search this branch of the tree again
                last_empty_parent = parent;

                if (parent->m_parent == nullptr)
                {
                    break;
                }

                child = parent;
                parent = child->m_parent;
            }
        }

        if (last_empty_parent != nullptr)
        {
            AssertThrow(last_empty_parent->EmptyDeep(DEPTH_SEARCH_INF));

            /* At highest empty parent octant, call Undivide() to collapse entries */
            if (allow_rebuild)
            {
                last_empty_parent->Undivide();
            }
            else
            {
                m_state->MarkOctantDirty(last_empty_parent->GetOctantID());
            }
        }
    }

    return {};
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Move(const TEntry& value, const BoundingBox& aabb, bool allow_rebuild, EntrySet::Iterator it)
{
    const BoundingBox& new_aabb = aabb;

    const bool is_root = IsRoot();
    const bool contains = m_aabb.Contains(new_aabb);

    if (!contains)
    {
        // NO LONGER CONTAINS AABB

        if (is_root)
        {
            // have to rebuild, invalidating child octants.
            // which we have a Contains() check for child entries walking upwards

            if (allow_rebuild)
            {
                return RebuildExtend_Internal(new_aabb);
            }
            else
            {
                m_state->MarkOctantDirty(m_octant_id);

                // Moved outside of the root octree, but we keep it here for now.
                // Next call of PerformUpdates(), we will extend the octree.
                return {
                    { OctreeBase<Derived, TEntry>::Result::OCTREE_OK },
                    m_octant_id
                };
            }
        }

        // not root

        Optional<InsertResult> parent_insert_result;

        /* Contains is false at this point */
        OctreeBase* parent = m_parent;
        OctreeBase* last_parent = m_parent;

        while (parent != nullptr)
        {
            last_parent = parent;

            if (parent->m_aabb.Contains(new_aabb))
            {
                if (it != m_entries.End())
                {
                    if (m_state != nullptr)
                    {
                        auto entry_to_octree_it = m_state->entry_to_octree.Find(value);

                        AssertThrow(entry_to_octree_it != m_state->entry_to_octree.End());
                        AssertThrow(entry_to_octree_it->second == this);

                        m_state->entry_to_octree.Erase(entry_to_octree_it);
                    }

                    m_entries.Erase(it);
                }

                parent_insert_result = parent->Move(value, aabb, allow_rebuild, parent->m_entries.End());

                break;
            }

            parent = parent->m_parent;
        }

        if (parent_insert_result.HasValue())
        { // succesfully inserted, safe to call CollapseParents()
            // Entry has now been added to it's appropriate octant which is a parent of this -
            // collapse this up to the top
            CollapseParents(allow_rebuild);

            return parent_insert_result.Get();
        }

        // not inserted because no Move() was called on parents (because they don't contain AABB),
        // have to _manually_ call Move() which will go to the above branch for the parent octant,
        // this invalidating `this`

        AssertThrow(last_parent != nullptr);

        return last_parent->Move(value, aabb, allow_rebuild, last_parent->m_entries.End());
    }

    // CONTAINS AABB HERE

    if (allow_rebuild)
    {
        // Check if we can go deeper.
        for (Octant& octant : m_octants)
        {
            if (octant.aabb.Contains(new_aabb))
            {
                if (it != m_entries.End())
                {
                    if (m_state != nullptr)
                    {
                        auto entry_to_octree_it = m_state->entry_to_octree.Find(value);

                        AssertThrow(entry_to_octree_it != m_state->entry_to_octree.End());
                        AssertThrow(entry_to_octree_it->second == this);

                        m_state->entry_to_octree.Erase(entry_to_octree_it);
                    }

                    m_entries.Erase(it);
                }

                if (!IsDivided())
                {
                    if (allow_rebuild && m_octant_id.GetDepth() < OctantId::max_depth - 1)
                    {
                        Divide();
                    }
                    else
                    {
                        continue;
                    }
                }

                AssertThrow(octant.octree != nullptr);

                const auto octant_move_result = octant.octree->Move(value, aabb, allow_rebuild, octant.octree->m_entries.End());
                AssertThrow(octant_move_result.first);

                return octant_move_result;
            }
        }
    }
    else
    {
        m_state->MarkOctantDirty(m_octant_id);
    }

    if (it != m_entries.End())
    { /* Not moved out of this octant (for now) */
        it->aabb = new_aabb;
    }
    else
    { /* Moved into new octant */
        m_entries.Insert(Entry { value, new_aabb });

        if (m_state != nullptr)
        {
            AssertThrowMsg(m_state->entry_to_octree.Find(value) == m_state->entry_to_octree.End(), "Entry must not already be in octree hierarchy.");

            m_state->entry_to_octree.Insert(value, this);
        }
    }

    return {
        { OctreeBase<Derived, TEntry>::Result::OCTREE_OK },
        m_octant_id
    };
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Update(const TEntry& value, const BoundingBox& aabb, bool force_invalidation, bool allow_rebuild)
{
    if (m_state != nullptr)
    {
        const auto it = m_state->entry_to_octree.Find(value);

        if (it == m_state->entry_to_octree.End())
        {
            return {
                { Result::OCTREE_ERR, "Object not found in entry map!" },
                OctantId::Invalid()
            };
        }

        if (OctreeBase* octree = it->second)
        {
            return octree->Update_Internal(value, aabb, force_invalidation, allow_rebuild);
        }

        return {
            { Result::OCTREE_ERR, "Object has no octree in entry map!" },
            OctantId::Invalid()
        };
    }

    return Update_Internal(value, aabb, force_invalidation, allow_rebuild);
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Update_Internal(const TEntry& value, const BoundingBox& aabb, bool force_invalidation, bool allow_rebuild)
{
    const auto it = m_entries.Find(value);

    if (it == m_entries.End())
    {
        if (m_is_divided)
        {
            for (Octant& octant : m_octants)
            {
                AssertThrow(octant.octree != nullptr);

                auto update_internal_result = octant.octree->Update_Internal(value, aabb, force_invalidation, allow_rebuild);

                if (update_internal_result.first)
                {
                    return update_internal_result;
                }
            }
        }

        return {
            { Result::OCTREE_ERR, "Could not update in any sub octants" },
            OctantId::Invalid()
        };
    }

    if (force_invalidation)
    {
        // force invalidation of this entry so the octant's hash will be updated
        Invalidate();
    }

    const BoundingBox& new_aabb = aabb;
    const BoundingBox& old_aabb = it->aabb;

    if (new_aabb == old_aabb)
    {
        if (force_invalidation)
        {
            // force invalidation of this entry so the octant's hash will be updated
            m_state->MarkOctantDirty(m_octant_id);
        }

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

    return Move(value, new_aabb, allow_rebuild, it);
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Rebuild()
{
    if (IsRoot())
    {
        return Rebuild(BoundingBox::Empty(), /* allow_grow */ true);
    }
    else
    {
        // if we are not root, we can't grow this octant as it would invalidate the rules of an octree!
        return Rebuild(m_aabb, /* allow_grow */ false);
    }
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Rebuild(const BoundingBox& new_aabb, bool allow_grow)
{
    Array<Entry> new_entries;
    Clear(new_entries, /* undivide */ true);

    m_aabb = new_aabb;

    for (auto& it : new_entries)
    {
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

        auto insert_result = Insert(it.value, it.aabb, true /* allow rebuild */);

        if (!insert_result.first)
        {
            return insert_result;
        }
    }

    return {
        { OctreeBase<Derived, TEntry>::Result::OCTREE_OK },
        m_octant_id
    };
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::RebuildExtend_Internal(const BoundingBox& extend_include_aabb)
{
    if (!extend_include_aabb.IsValid())
    {
        return {
            { OctreeBase<Derived, TEntry>::Result::OCTREE_ERR, "AABB is in invalid state" },
            OctantId::Invalid()
        };
    }

    if (!extend_include_aabb.IsFinite())
    {
        return {
            { OctreeBase<Derived, TEntry>::Result::OCTREE_ERR, "AABB is not finite" },
            OctantId::Invalid()
        };
    }

    // have to grow the aabb by rebuilding the octree
    BoundingBox new_aabb(m_aabb.Union(extend_include_aabb));
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    new_aabb *= growth_factor;

    return Rebuild(new_aabb, /* allow_grow */ false);
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::PerformUpdates()
{
    AssertThrow(m_state != nullptr);

    if (!m_state->NeedsRebuild())
    {
        // No octant to rebuild, skipping
        return;
    }

    OctreeBase* octant = GetChildOctant(m_state->rebuild_state);
    AssertThrow(octant != nullptr);

    const auto rebuild_result = octant->Rebuild();

    if (rebuild_result.first)
    {
        // set rebuild state back to invalid if rebuild was successful
        m_state->rebuild_state = OctantId::Invalid();
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::CollectEntries(Array<const TEntry*>& out_entries) const
{
    out_entries.Reserve(out_entries.Size() + m_entries.Size());

    for (const OctreeBase<Derived, TEntry>::Entry& entry : m_entries)
    {
        out_entries.PushBack(&entry.value);
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntries(out_entries);
        }
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::CollectEntries(const BoundingSphere& bounds, Array<const TEntry*>& out_entries) const
{
    if (!bounds.Overlaps(m_aabb))
    {
        return;
    }

    out_entries.Reserve(out_entries.Size() + m_entries.Size());

    for (const OctreeBase<Derived, TEntry>::Entry& entry : m_entries)
    {
        if (bounds.Overlaps(entry.aabb))
        {
            out_entries.PushBack(&entry.value);
        }
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntries(bounds, out_entries);
        }
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::CollectEntries(const BoundingBox& bounds, Array<const TEntry*>& out_entries) const
{
    if (!m_aabb.Overlaps(bounds))
    {
        return;
    }

    out_entries.Reserve(out_entries.Size() + m_entries.Size());

    for (const OctreeBase<Derived, TEntry>::Entry& entry : m_entries)
    {
        if (bounds.Overlaps(entry.aabb))
        {
            out_entries.PushBack(&entry.value);
        }
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntries(bounds, out_entries);
        }
    }
}

template <class Derived, class TEntry>
bool OctreeBase<Derived, TEntry>::GetNearestOctants(const Vec3f& position, FixedArray<Derived*, 8>& out) const
{
    if (!m_aabb.ContainsPoint(position))
    {
        return false;
    }

    if (!m_is_divided)
    {
        return false;
    }

    for (const Octant& octant : m_octants)
    {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->GetNearestOctants(position, out))
        {
            return true;
        }
    }

    for (SizeType i = 0; i < m_octants.Size(); i++)
    {
        out[i] = static_cast<Derived*>(m_octants[i].octree.Get());
    }

    return true;
}

template <class Derived, class TEntry>
bool OctreeBase<Derived, TEntry>::GetNearestOctant(const Vec3f& position, Derived const*& out) const
{
    if (!m_aabb.ContainsPoint(position))
    {
        return false;
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree->GetNearestOctant(position, out))
            {
                return true;
            }
        }
    }

    out = static_cast<const Derived*>(this);

    return true;
}

template <class Derived, class TEntry>
bool OctreeBase<Derived, TEntry>::GetFittingOctant(const BoundingBox& aabb, Derived const*& out) const
{
    if (!m_aabb.Contains(aabb))
    {
        out = nullptr;

        return false;
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree->GetFittingOctant(aabb, out))
            {
                return true;
            }
        }
    }

    out = static_cast<const Derived*>(this);

    return true;
}