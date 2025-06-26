/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RESOURCE_TRACKER_HPP
#define HYPERION_RESOURCE_TRACKER_HPP

#include <core/ID.hpp>
#include <core/Defines.hpp>

#include <core/containers/SparsePagedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/TypeID.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_API extern SizeType GetNumDescendants(TypeID type_id);
HYP_API extern int GetSubclassIndex(TypeID base_type_id, TypeID subclass_type_id);

/*! \brief The action to take on call to Advance for a ResourceTracker. */
enum class AdvanceAction : uint32
{
    CLEAR,  //! Clear the 'next' elements, so on next iteration, any elements that have not been re-added are marked for removal.
    PERSIST //! Copy the previous elements over to next. To remove elements, `RemoveProxy` needs to be manually called.
};

template <class IDType, class ElementType>
class ResourceTracker
{
    template <class Derived, bool IsConst>
    struct IteratorBase
    {
        using Element = std::conditional_t<IsConst, const ElementType, ElementType>;
        using ResourceTrackerType = std::conditional_t<IsConst, const ResourceTracker, ResourceTracker>;

        ResourceTrackerType* tracker;

        // index into m_subclass_impls (and m_subclass_impls_initialized)
        SizeType subclass_impl_index;
        SizeType element_index;

        IteratorBase(ResourceTrackerType* tracker, SizeType subclass_idx, SizeType elem_idx)
            : tracker(tracker),
              subclass_impl_index(subclass_idx),
              element_index(elem_idx)
        {
            if (subclass_impl_index == Bitset::not_found && element_index == Bitset::not_found)
            {
                // both set to not_found : at end
                return;
            }

            // find valid element - first check m_impl then try subclasses if not found
            if (subclass_impl_index == Bitset::not_found)
            {
                element_index = FindNextSet(tracker->m_impl.GetCurrentBits(), element_index);

                if (element_index != Bitset::not_found)
                {
                    // We found an element at the given index in our main impl.
                    return;
                }

                // Need to search subclasses - set indices to zero so we land on the first subclass that is valid and has a valid element
                subclass_impl_index = 0;
                element_index = 0;
            }

            // Find first subclass that is set (if it exists),
            // next part will handle finding the element index.
            subclass_impl_index = FindNextSet(tracker->m_subclass_impls_initialized, subclass_impl_index);

            while (subclass_impl_index != Bitset::not_found)
            {
                AssertDebug(subclass_impl_index < tracker->m_subclass_impls.Size());

                auto& impl = tracker->m_subclass_impls[subclass_impl_index].Get();
                element_index = FindNextSet(impl.GetCurrentBits(), element_index);

                // Found valid element in subclass impl.
                if (element_index != Bitset::not_found)
                {
                    return;
                }

                // If we are at the end of the current subclass impl, move to the next one.
                subclass_impl_index = FindNextSet(tracker->m_subclass_impls_initialized, subclass_impl_index + 1);
                element_index = 0;
            }

            // exhausted all subclass elements
            subclass_impl_index = Bitset::not_found;
            element_index = Bitset::not_found;
        }

        Derived& operator++()
        {
            // at the end check
            if (subclass_impl_index == Bitset::not_found && element_index == Bitset::not_found)
            {
                HYP_FAIL("At end! cannot increment");
            }

            if (subclass_impl_index == Bitset::not_found)
            {
                element_index = FindNextSet(tracker->m_impl.GetCurrentBits(), element_index + 1);

                if (element_index != Bitset::not_found)
                {
                    // We found an element at the given index in our main impl.
                    return static_cast<Derived&>(*this);
                }

                // Find first subclass that is set (if it exists),
                // next part will handle finding the element index.
                subclass_impl_index = FindNextSet(tracker->m_subclass_impls_initialized, 0);
                element_index = SizeType(-1); // wrap around for next
            }

            while (subclass_impl_index != Bitset::not_found)
            {
                AssertDebug(subclass_impl_index < tracker->m_subclass_impls.Size());

                auto& impl = tracker->m_subclass_impls[subclass_impl_index].Get();

                element_index = FindNextSet(impl.GetCurrentBits(), element_index + 1);

                if (element_index != Bitset::not_found)
                {
                    // Found a valid subclass element with valid element
                    return static_cast<Derived&>(*this);
                }

                // If we are at the end of the current subclass impl, move to the next one.
                subclass_impl_index = FindNextSet(tracker->m_subclass_impls_initialized, subclass_impl_index + 1);
                element_index = SizeType(-1); // wrap around for next
            }

            // exhausted all subclass elements
            subclass_impl_index = Bitset::not_found;
            element_index = Bitset::not_found;

            return static_cast<Derived&>(*this);
        }

        Derived operator++(int)
        {
            Derived tmp = static_cast<Derived>(*this);
            ++(*this);
            return tmp;
        }

        Element& operator*() const
        {
            if (subclass_impl_index == Bitset::not_found && element_index == Bitset::not_found)
            {
                HYP_FAIL("At end! cannot dereference!");
            }

            if (subclass_impl_index == Bitset::not_found) // primary search phase
            {
                return tracker->m_impl.elements[element_index];
            }
            else
            {
                AssertDebug(subclass_impl_index < tracker->m_subclass_impls.Size());

                return tracker->m_subclass_impls[subclass_impl_index].Get().elements[element_index];
            }

            HYP_FAIL("Invalid iterator phase");
        }

        Element* operator->() const
        {
            return &(**this);
        }

        bool operator==(const Derived& other) const
        {
            return tracker == other.tracker
                && subclass_impl_index == other.subclass_impl_index
                && element_index == other.element_index;
        }

        bool operator!=(const Derived& other) const
        {
            return tracker != other.tracker
                || subclass_impl_index != other.subclass_impl_index
                || element_index != other.element_index;
        }

    private:
        HYP_FORCE_INLINE bool IsAtEnd() const
        {
            return subclass_impl_index == Bitset::not_found
                && element_index == Bitset::not_found;
        }

        HYP_FORCE_INLINE static Bitset::BitIndex FindNextSet(const Bitset& bs, Bitset::BitIndex idx)
        {
            return bs.NextSetBitIndex(idx);
        }
    };

public:
    struct ConstIterator;

    struct Iterator : IteratorBase<Iterator, false>
    {
        Iterator() = delete;

        Iterator(ResourceTracker* tracker, SizeType subclass_impl_index, SizeType element_index)
            : IteratorBase<Iterator, false>(tracker, subclass_impl_index, element_index)
        {
        }

        // Allow implicit conversion to ConstIterator
        operator ConstIterator() const
        {
            return ConstIterator(
                IteratorBase<Iterator, false>::tracker,
                IteratorBase<Iterator, false>::subclass_impl_index,
                IteratorBase<Iterator, false>::element_index);
        }
    };

    struct ConstIterator : IteratorBase<ConstIterator, true>
    {
        ConstIterator() = delete;

        ConstIterator(const ResourceTracker* tracker, SizeType subclass_impl_index, SizeType element_index)
            : IteratorBase<ConstIterator, true>(tracker, subclass_impl_index, element_index)
        {
        }
    };

    enum ResourceTrackState : uint8
    {
        UNCHANGED = 0x0,
        CHANGED_ADDED = 0x1,
        CHANGED_MODIFIED = 0x2,

        CHANGED = CHANGED_ADDED | CHANGED_MODIFIED
    };

    struct Diff
    {
        uint32 num_added = 0;
        uint32 num_removed = 0;
        uint32 num_changed = 0;

        HYP_FORCE_INLINE bool NeedsUpdate() const
        {
            return num_added > 0 || num_removed > 0 || num_changed > 0;
        }
    };

    // use a sparse array so we can use IDs as indices
    // without worring about hashing for lookups and allowing us to
    // still iterate over the elements (mostly) linearly.
    using ElementArrayType = SparsePagedArray<ElementType, 256>;

    static_assert(std::is_base_of_v<IDBase, IDType>, "IDType must be derived from IDBase (must use numeric ID)");

    ResourceTracker()
        : m_impl(IDType::type_id_static) // default impl for base class
    {
        // Setup the subclass implementations array, we initialize them as they get used
        const SizeType num_descendants = GetNumDescendants(IDType::type_id_static);
        m_subclass_impls.Resize(num_descendants);
        m_subclass_impls_initialized.Resize(num_descendants);
    }

    ResourceTracker(const ResourceTracker& other) = delete;
    ResourceTracker& operator=(const ResourceTracker& other) = delete;

    ResourceTracker(ResourceTracker&& other) noexcept = delete;
    ResourceTracker& operator=(ResourceTracker&& other) noexcept = delete;

    ~ResourceTracker()
    {
        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            AssertDebug(bit_index < m_subclass_impls.Size());

            m_subclass_impls[bit_index].Destruct();
        }
    }

    uint32 NumCurrent() const
    {
        HYP_SCOPE;

        uint32 count = m_impl.GetCurrentBits().Count();

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            AssertDebug(bit_index < m_subclass_impls.Size());

            count += m_subclass_impls[bit_index].Get().GetCurrentBits().Count();
        }

        return count;
    }

    uint32 NumCurrent(TypeID type_id) const
    {
        if (type_id == IDType::type_id_static)
        {
            return m_impl.GetCurrentBits().Count();
        }

        const int subclass_index = GetSubclassIndex(m_impl.type_id, type_id);
        AssertDebugMsg(subclass_index != -1, "Invalid subclass index");
        AssertDebugMsg(subclass_index < m_subclass_impls.Size(), "Invalid subclass index");

        if (m_subclass_impls_initialized.Test(subclass_index))
        {
            return m_subclass_impls[subclass_index].Get().GetCurrent().Count();
        }

        return 0;
    }

    const ElementArrayType& GetElements(TypeID type_id) const
    {
        static const ElementArrayType empty_array {};

        if (type_id == IDType::type_id_static)
        {
            return m_impl.elements;
        }

        const int subclass_index = GetSubclassIndex(m_impl.type_id, type_id);
        AssertDebugMsg(subclass_index != -1, "Invalid subclass index");
        AssertDebugMsg(subclass_index < m_subclass_impls.Size(), "Invalid subclass index");

        if (m_subclass_impls_initialized.Test(subclass_index))
        {
            return m_subclass_impls[subclass_index].Get().elements;
        }

        return empty_array;
    }

    template <class T>
    const ElementArrayType& GetElements() const
    {
        static constexpr TypeID type_id = TypeID::ForType<T>();

        return GetElements(type_id);
    }

    HYP_FORCE_INLINE const Bitset& GetSubclassBits() const
    {
        return m_subclass_impls_initialized;
    }

    Diff GetDiff() const
    {
        HYP_SCOPE;

        Diff diff;

        diff.num_added = m_impl.GetAdded().Count();
        diff.num_removed = m_impl.GetRemoved().Count();
        diff.num_changed = m_impl.GetChanged().Count();

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            AssertDebug(bit_index < m_subclass_impls.Size());

            diff.num_added += m_subclass_impls[bit_index].Get().GetAdded().Count();
            diff.num_removed += m_subclass_impls[bit_index].Get().GetRemoved().Count();
            diff.num_changed += m_subclass_impls[bit_index].Get().GetChanged().Count();
        }

        return diff;
    }

    void Track(IDType id, const ElementType& element, ResourceTrackState* out_track_state = nullptr)
    {
        HYP_SCOPE;

        TypeID type_id = id.GetTypeID();

        if (type_id == m_impl.type_id)
        {
            m_impl.Track(id, element, out_track_state);
            return;
        }

        const int subclass_index = GetSubclassIndex(m_impl.type_id, type_id);
        AssertDebugMsg(subclass_index != -1, "Invalid subclass index");
        AssertDebugMsg(subclass_index < m_subclass_impls.Size(), "Invalid subclass index");

        if (!m_subclass_impls_initialized.Test(subclass_index))
        {
            m_subclass_impls[subclass_index].Construct(type_id);
            m_subclass_impls_initialized.Set(subclass_index, true);
        }

        m_subclass_impls[subclass_index].Get().Track(id, element, out_track_state);
    }

    void Track(IDType id, ElementType&& element, ResourceTrackState* out_track_state = nullptr)
    {
        HYP_SCOPE;

        TypeID type_id = id.GetTypeID();

        if (type_id == m_impl.type_id)
        {
            m_impl.Track(id, std::move(element), out_track_state);
            return;
        }

        const int subclass_index = GetSubclassIndex(m_impl.type_id, type_id);
        AssertDebugMsg(subclass_index != -1, "Invalid subclass index");
        AssertDebugMsg(subclass_index < m_subclass_impls.Size(), "Invalid subclass index");

        if (!m_subclass_impls_initialized.Test(subclass_index))
        {
            m_subclass_impls[subclass_index].Construct(type_id);
            m_subclass_impls_initialized.Set(subclass_index, true);
        }

        m_subclass_impls[subclass_index].Get().Track(id, std::move(element), out_track_state);
    }

    bool MarkToKeep(IDType id)
    {
        HYP_SCOPE;

        TypeID type_id = id.GetTypeID();

        if (type_id == m_impl.type_id)
        {
            return m_impl.MarkToKeep(id);
        }

        const int subclass_index = GetSubclassIndex(m_impl.type_id, type_id);
        AssertDebugMsg(subclass_index != -1, "Invalid subclass index");
        AssertDebugMsg(subclass_index < m_subclass_impls.Size(), "Invalid subclass index");

        if (!m_subclass_impls_initialized.Test(subclass_index))
        {
            return false;
        }

        return m_subclass_impls[subclass_index].Get().MarkToKeep(id);
    }

    void MarkToRemove(IDType id)
    {
        TypeID type_id = id.GetTypeID();

        if (type_id == m_impl.type_id)
        {
            m_impl.MarkToRemove(id);

            return;
        }

        const int subclass_index = GetSubclassIndex(m_impl.type_id, type_id);
        AssertDebugMsg(subclass_index != -1, "Invalid subclass index");
        AssertDebugMsg(subclass_index < m_subclass_impls.Size(), "Invalid subclass index");

        if (!m_subclass_impls_initialized.Test(subclass_index))
        {
            return;
        }

        m_subclass_impls[subclass_index].Get().MarkToRemove(id);
    }

    template <class AllocatorType>
    void GetRemoved(Array<IDType, AllocatorType>& out_ids, bool include_changed) const
    {
        HYP_SCOPE;

        m_impl.GetRemoved(out_ids, include_changed);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetRemoved(out_ids, include_changed);
        }
    }

    template <class AllocatorType>
    void GetRemoved(Array<ElementType, AllocatorType>& out, bool include_changed) const
    {
        HYP_SCOPE;

        m_impl.GetRemoved(out, include_changed);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetRemoved(out, include_changed);
        }
    }

    template <class AllocatorType>
    void GetRemoved(Array<ElementType*, AllocatorType>& out, bool include_changed) const
    {
        HYP_SCOPE;

        m_impl.GetRemoved(out, include_changed);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetRemoved(out, include_changed);
        }
    }

    template <class AllocatorType>
    void GetAdded(Array<ElementType, AllocatorType>& out, bool include_changed) const
    {
        HYP_SCOPE;

        m_impl.GetAdded(out, include_changed);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetAdded(out, include_changed);
        }
    }

    template <class AllocatorType>
    void GetAdded(Array<ElementType*, AllocatorType>& out, bool include_changed) const
    {
        HYP_SCOPE;

        m_impl.GetAdded(out, include_changed);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetAdded(out, include_changed);
        }
    }

    template <class AllocatorType>
    void GetChanged(Array<IDType, AllocatorType>& out_ids) const
    {
        HYP_SCOPE;

        m_impl.GetChanged(out_ids);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetChanged(out_ids);
        }
    }

    template <class AllocatorType>
    void GetChanged(Array<ElementType, AllocatorType>& out) const
    {
        HYP_SCOPE;

        m_impl.GetChanged(out);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetChanged(out);
        }
    }

    template <class AllocatorType>
    void GetChanged(Array<ElementType*, AllocatorType>& out) const
    {
        HYP_SCOPE;

        m_impl.GetChanged(out);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetChanged(out);
        }
    }

    template <class AllocatorType>
    void GetCurrent(Array<ElementType, AllocatorType>& out) const
    {
        HYP_SCOPE;

        m_impl.GetCurrent(out);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetCurrent(out);
        }
    }

    template <class AllocatorType>
    void GetCurrent(Array<ElementType*, AllocatorType>& out) const
    {
        HYP_SCOPE;

        m_impl.GetCurrent(out);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().GetCurrent(out);
        }
    }

    ElementType* GetElement(IDType id)
    {
        HYP_SCOPE;

        TypeID type_id = id.GetTypeID();

        if (type_id == m_impl.type_id)
        {
            return m_impl.GetElement(id);
        }

        const int subclass_index = GetSubclassIndex(m_impl.type_id, type_id);
        AssertDebugMsg(subclass_index != -1, "Invalid subclass index");
        AssertDebugMsg(subclass_index < m_subclass_impls.Size(), "Invalid subclass index");

        if (!m_subclass_impls_initialized.Test(subclass_index))
        {
            return nullptr;
        }

        return m_subclass_impls[subclass_index].Get().GetElement(id);
    }

    void Advance(AdvanceAction action)
    {
        HYP_SCOPE;

        m_impl.Advance(action);

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().Advance(action);
        }
    }

    void Reset()
    {
        m_impl.Reset();

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            m_subclass_impls[bit_index].Get().Reset();
        }
    }

    HYP_DEF_STL_BEGIN_END(Iterator(const_cast<ResourceTracker*>(this), Bitset::not_found, 0), Iterator(const_cast<ResourceTracker*>(this), Bitset::not_found, Bitset::not_found))

    // protected:
    struct Impl final
    {
        /*! \brief Checks if the ResourceTracker<ID<Entity>, RenderProxy> already has a proxy for the given ID from the previous frame */
        HYP_FORCE_INLINE bool HasElement(IDType id) const
        {
            AssertDebugMsg(id.GetTypeID() == type_id, "ResourceTracker typeid mismatch");

            return previous.Test(id.ToIndex());
        }

        HYP_FORCE_INLINE const Bitset& GetCurrentBits() const
        {
            return previous;
        }

        HYP_FORCE_INLINE Bitset GetAdded() const
        {
            const SizeType new_nubits = MathUtil::Max(previous.NumBits(), next.NumBits());

            return Bitset(next).Resize(new_nubits) & ~Bitset(previous).Resize(new_nubits);
        }

        HYP_FORCE_INLINE Bitset GetRemoved() const
        {
            const SizeType new_nubits = MathUtil::Max(previous.NumBits(), next.NumBits());

            return Bitset(previous).Resize(new_nubits) & ~Bitset(next).Resize(new_nubits);
        }

        HYP_FORCE_INLINE const Bitset& GetChanged() const
        {
            return changed;
        }

        void Track(IDType id, const ElementType& element, ResourceTrackState* out_track_state = nullptr)
        {
            ElementType* ptr = nullptr;

            AssertThrowMsg(!next.Test(id.ToIndex()), "ID #%u already marked to be added for this iteration!", id.Value());

            if (HasElement(id))
            {
                AssertDebug(elements.HasIndex(id.ToIndex()));

                ptr = &elements[id.ToIndex()];
            }

            if (ptr)
            {
                // Advance if version has changed
                if (element != *ptr)
                { // element.version != iter->second.version) {
                    // Sanity check - must not contain duplicates or it will mess up safe releasing the previous RenderProxy objects
                    AssertDebug(!changed.Test(id.ToIndex()));

                    // Mark as changed if it is found in the previous iteration
                    changed.Set(id.ToIndex(), true);

                    *ptr = element;

                    if (out_track_state != nullptr)
                    {
                        *out_track_state = ResourceTrackState::CHANGED_MODIFIED;
                    }
                }
                else
                {
                    if (out_track_state != nullptr)
                    {
                        *out_track_state = ResourceTrackState::UNCHANGED;
                    }
                }
            }
            else
            {
                // sanity check - if not in previous iteration, it must not be in the changed list
                AssertDebug(!changed.Test(id.ToIndex()));

                elements.Set(id.ToIndex(), element);

                if (out_track_state != nullptr)
                {
                    *out_track_state = ResourceTrackState::CHANGED_ADDED;
                }
            }

            next.Set(id.ToIndex(), true);
        }

        void Track(IDType id, ElementType&& element, ResourceTrackState* out_track_state = nullptr)
        {
            ElementType* ptr = nullptr;

            AssertThrowMsg(!next.Test(id.ToIndex()), "ID #%u already marked to be added for this iteration!", id.Value());

            if (HasElement(id))
            {
                AssertDebug(elements.HasIndex(id.ToIndex()));

                ptr = &elements[id.ToIndex()];
            }

            if (ptr)
            {
                // Advance if version has changed
                if (element != *ptr)
                { // element.version != iter->second.version) {
                    // Sanity check - must not contain duplicates or it will mess up safe releasing the previous RenderProxy objects
                    AssertDebug(!changed.Test(id.ToIndex()));

                    // Mark as changed if it is found in the previous iteration
                    changed.Set(id.ToIndex(), true);

                    *ptr = std::move(element);

                    if (out_track_state != nullptr)
                    {
                        *out_track_state = ResourceTrackState::CHANGED_MODIFIED;
                    }
                }
                else
                {
                    if (out_track_state != nullptr)
                    {
                        *out_track_state = ResourceTrackState::UNCHANGED;
                    }
                }
            }
            else
            {
                // sanity check - if not in previous iteration, it must not be in the changed list
                AssertDebug(!changed.Test(id.ToIndex()));

                elements.Set(id.ToIndex(), std::move(element));

                if (out_track_state != nullptr)
                {
                    *out_track_state = ResourceTrackState::CHANGED_ADDED;
                }
            }

            next.Set(id.ToIndex(), true);
        }

        HYP_FORCE_INLINE bool MarkToKeep(IDType id)
        {
            if (!previous.Test(id.ToIndex()))
            {
                return false;
            }

            next.Set(id.ToIndex(), true);

            return true;
        }

        HYP_FORCE_INLINE void MarkToRemove(IDType id)
        {
            next.Set(id.ToIndex(), false);
        }

        template <class AllocatorType>
        void GetRemoved(Array<IDType, AllocatorType>& out_ids, bool include_changed) const
        {
            HYP_SCOPE;

            Bitset removed_bits = GetRemoved();

            if (include_changed)
            {
                removed_bits |= GetChanged();
            }

            out_ids.Reserve(removed_bits.Count());

            SizeType first_set_bit_index;

            while ((first_set_bit_index = removed_bits.FirstSetBitIndex()) != -1)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                out_ids.PushBack(id);

                removed_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetRemoved(Array<ElementType, AllocatorType>& out, bool include_changed) const
        {
            HYP_SCOPE;

            Bitset removed_bits = GetRemoved();

            if (include_changed)
            {
                removed_bits |= GetChanged();
            }

            out.Reserve(out.Size() + removed_bits.Count());

            Bitset::BitIndex first_set_bit_index;

            while ((first_set_bit_index = removed_bits.FirstSetBitIndex()) != Bitset::not_found)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);

                removed_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetRemoved(Array<ElementType*, AllocatorType>& out, bool include_changed) const
        {
            HYP_SCOPE;

            Bitset removed_bits = GetRemoved();

            if (include_changed)
            {
                removed_bits |= GetChanged();
            }

            out.Reserve(out.Size() + removed_bits.Count());

            Bitset::BitIndex first_set_bit_index;

            while ((first_set_bit_index = removed_bits.FirstSetBitIndex()) != Bitset::not_found)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));

                removed_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetAdded(Array<ElementType, AllocatorType>& out, bool include_changed) const
        {
            HYP_SCOPE;

            Bitset newly_added_bits = GetAdded();

            if (include_changed)
            {
                newly_added_bits |= GetChanged();
            }

            out.Reserve(out.Size() + newly_added_bits.Count());

            Bitset::BitIndex first_set_bit_index;

            while ((first_set_bit_index = newly_added_bits.FirstSetBitIndex()) != Bitset::not_found)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);

                newly_added_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetAdded(Array<ElementType*, AllocatorType>& out, bool include_changed) const
        {
            HYP_SCOPE;

            Bitset newly_added_bits = GetAdded();

            if (include_changed)
            {
                newly_added_bits |= GetChanged();
            }

            out.Reserve(out.Size() + newly_added_bits.Count());

            Bitset::BitIndex first_set_bit_index;

            while ((first_set_bit_index = newly_added_bits.FirstSetBitIndex()) != Bitset::not_found)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));

                newly_added_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetChanged(Array<IDType, AllocatorType>& out_ids) const
        {
            HYP_SCOPE;

            Bitset changed_bits = GetChanged();

            out_ids.Reserve(changed_bits.Count());

            SizeType first_set_bit_index;

            while ((first_set_bit_index = changed_bits.FirstSetBitIndex()) != -1)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                out_ids.PushBack(id);

                changed_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetChanged(Array<ElementType, AllocatorType>& out) const
        {
            HYP_SCOPE;

            Bitset changed_bits = GetChanged();

            out.Reserve(out.Size() + changed_bits.Count());

            Bitset::BitIndex first_set_bit_index;

            while ((first_set_bit_index = changed_bits.FirstSetBitIndex()) != Bitset::not_found)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);

                changed_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetChanged(Array<ElementType*, AllocatorType>& out) const
        {
            HYP_SCOPE;

            Bitset changed_bits = GetChanged();

            out.Reserve(out.Size() + changed_bits.Count());

            Bitset::BitIndex first_set_bit_index;

            while ((first_set_bit_index = changed_bits.FirstSetBitIndex()) != Bitset::not_found)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));

                changed_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetCurrent(Array<ElementType, AllocatorType>& out) const
        {
            HYP_SCOPE;

            Bitset current_bits = previous;

            out.Reserve(out.Size() + current_bits.Count());

            Bitset::BitIndex first_set_bit_index;

            while ((first_set_bit_index = current_bits.FirstSetBitIndex()) != Bitset::not_found)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);

                current_bits.Set(first_set_bit_index, false);
            }
        }

        template <class AllocatorType>
        void GetCurrent(Array<ElementType*, AllocatorType>& out) const
        {
            HYP_SCOPE;

            Bitset current_bits = previous;

            out.Reserve(out.Size() + current_bits.Count());

            Bitset::BitIndex first_set_bit_index;

            while ((first_set_bit_index = current_bits.FirstSetBitIndex()) != Bitset::not_found)
            {
                const IDType id = IDType(IDBase { type_id, uint32(first_set_bit_index + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));

                current_bits.Set(first_set_bit_index, false);
            }
        }

        ElementType* GetElement(IDType id)
        {
            AssertDebug(id.GetTypeID() == type_id);

            if (id.GetTypeID() != type_id)
            {
                return nullptr;
            }

            return elements.TryGet(id.ToIndex());
        }

        void Advance(AdvanceAction action)
        {
            HYP_SCOPE;

            { // Remove proxies for removed bits
                for (Bitset::BitIndex index : GetRemoved())
                {
                    AssertDebug(elements.HasIndex(index));

                    elements.EraseAt(index);
                }
            }

            switch (action)
            {
            case AdvanceAction::CLEAR:
                // Next state starts out zeroed out -- and next call to Advance will remove proxies for these objs
                previous = std::move(next);

                break;
            case AdvanceAction::PERSIST:
                previous = next;

                break;
            default:
                HYP_UNREACHABLE();

                break;
            }

            changed.Clear();
        }

        /*! \brief Total reset of the list, including clearing the previous state. */
        void Reset()
        {
            elements.Clear();
            previous.Clear();
            next.Clear();
            changed.Clear();
        }

        TypeID type_id;

        ElementArrayType elements;

        Bitset previous;
        Bitset next;
        Bitset changed;
    };

    // base class impl
    Impl m_impl;

    // per-subtype implementations (only constructed and setup on first Bind() call with that type)
    Array<ValueStorage<Impl>> m_subclass_impls;
    Bitset m_subclass_impls_initialized;
};

} // namespace hyperion

#endif