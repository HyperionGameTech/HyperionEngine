/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/ObjId.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>
#include <Core/Defines.hpp>

#include <Core/containers/SparsePagedArray.hpp>
#include <Core/containers/Map.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_API extern const Class* GetClass(const TypeId& typeId);
HYP_API extern size_t GetNumDescendants(TypeId typeId);
HYP_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

HYP_API extern const TypeInfo& Class_GetTypeInfo(const Class& cls);

class NullProxy;

namespace Resources {

struct ResourceTrackerDiff
{
    uint32 numAdded = 0;
    uint32 numRemoved = 0;
    uint32 numChanged = 0;

    HYP_FORCE_INLINE bool NeedsUpdate() const
    {
        return numAdded > 0 || numRemoved > 0 || numChanged > 0;
    }
};

template <class AllocatorType>
class ResourceTrackerBase
{
public:
    virtual ~ResourceTrackerBase() = default;
};

template <class AllocatorType, class IdType, class ElementType, class ProxyType = NullProxy>
class ResourceTracker final : public ResourceTrackerBase<AllocatorType>
{
public:
    struct Impl;

    using TIdType = IdType;
    using TElementType = ElementType;

    template <class Derived, bool IsConst>
    struct IteratorBase
    {
        using ResourceTrackerType = std::conditional_t<IsConst, const ResourceTracker, ResourceTracker>;
        using ImplType = typename ResourceTrackerType::Impl;

        ResourceTrackerType* tracker;

        // index into subclassImpls (and subclassIndices)
        size_t subclassImplIndex;
        size_t elementIndex;

        IteratorBase(ResourceTrackerType* tracker, size_t subclassIdx, size_t elemIdx)
            : tracker(tracker),
              subclassImplIndex(subclassIdx),
              elementIndex(elemIdx)
        {
            if (subclassImplIndex == Bitset::NotFound && elementIndex == Bitset::NotFound)
            {
                // both set to notFound : at end
                return;
            }

            // find valid element - first check baseImpl then try subclasses if not found
            if (subclassImplIndex == Bitset::NotFound)
            {
                elementIndex = FindNextSet(tracker->baseImpl.next, elementIndex);

                if (elementIndex != Bitset::NotFound)
                {
                    // We found an element at the given index in our main impl.
                    return;
                }

                // Need to search subclasses - set indices to zero so we land on the first subclass that is valid and has a valid element
                subclassImplIndex = 0;
                elementIndex = 0;
            }

            // Find first subclass that is set (if it exists),
            // next part will handle finding the element index.
            subclassImplIndex = FindNextSet(tracker->subclassIndices, subclassImplIndex);

            while (subclassImplIndex != Bitset::NotFound)
            {
                AssertDebug(subclassImplIndex < tracker->subclassImpls.Size());

                auto& impl = *tracker->subclassImpls[subclassImplIndex];
                elementIndex = FindNextSet(impl.next, elementIndex);

                // Found valid element in subclass impl.
                if (elementIndex != Bitset::NotFound)
                {
                    return;
                }

                // If we are at the end of the current subclass impl, move to the next one.
                subclassImplIndex = FindNextSet(tracker->subclassIndices, subclassImplIndex + 1);
                elementIndex = 0;
            }

            // exhausted all subclass elements
            subclassImplIndex = Bitset::NotFound;
            elementIndex = Bitset::NotFound;
        }

        Derived& operator++()
        {
            // at the end check
            if (subclassImplIndex == Bitset::NotFound && elementIndex == Bitset::NotFound)
            {
                HYP_FAIL("At end! cannot increment");
            }

            if (subclassImplIndex == Bitset::NotFound)
            {
                elementIndex = FindNextSet(tracker->baseImpl.next, elementIndex + 1);

                if (elementIndex != Bitset::NotFound)
                {
                    // We found an element at the given index in our main impl.
                    return static_cast<Derived&>(*this);
                }

                // Find first subclass that is set (if it exists),
                // next part will handle finding the element index.
                subclassImplIndex = FindNextSet(tracker->subclassIndices, 0);
                elementIndex = Bitset::BitIndex(-1); // wrap around for next
            }

            while (subclassImplIndex != Bitset::NotFound)
            {
                AssertDebug(subclassImplIndex < tracker->subclassImpls.Size());

                auto& impl = *tracker->subclassImpls[subclassImplIndex];

                elementIndex = FindNextSet(impl.next, elementIndex + 1);

                if (elementIndex != Bitset::NotFound)
                {
                    // Found a valid subclass element with valid element
                    return static_cast<Derived&>(*this);
                }

                // If we are at the end of the current subclass impl, move to the next one.
                subclassImplIndex = FindNextSet(tracker->subclassIndices, subclassImplIndex + 1);
                elementIndex = Bitset::BitIndex(-1); // wrap around for next
            }

            // exhausted all subclass elements
            subclassImplIndex = Bitset::NotFound;
            elementIndex = Bitset::NotFound;

            return static_cast<Derived&>(*this);
        }

        Derived operator++(int)
        {
            Derived tmp = static_cast<Derived>(*this);
            ++(*this);
            return tmp;
        }

        auto operator*() const -> std::conditional_t<IsConst, const ElementType, ElementType>&
        {
            if (subclassImplIndex == Bitset::NotFound && elementIndex == Bitset::NotFound)
            {
                HYP_FAIL("At end! cannot dereference!");
            }

            if (subclassImplIndex == Bitset::NotFound) // primary search phase
            {
                return tracker->baseImpl.elements.Get(elementIndex);
            }
            else
            {
                AssertDebug(subclassImplIndex < tracker->subclassImpls.Size());

                return tracker->subclassImpls[subclassImplIndex]->elements.Get(elementIndex);
            }

            HYP_FAIL("Invalid iterator phase");
        }

        auto operator->() const -> std::conditional_t<IsConst, const ElementType, ElementType>*
        {
            return &(**this);
        }

        bool operator==(const Derived& other) const
        {
            return tracker == other.tracker
                && subclassImplIndex == other.subclassImplIndex
                && elementIndex == other.elementIndex;
        }

        bool operator!=(const Derived& other) const
        {
            return tracker != other.tracker
                || subclassImplIndex != other.subclassImplIndex
                || elementIndex != other.elementIndex;
        }

    private:
        HYP_FORCE_INLINE bool IsAtEnd() const
        {
            return subclassImplIndex == Bitset::NotFound
                && elementIndex == Bitset::NotFound;
        }

        static HYP_FORCE_INLINE Bitset::BitIndex FindNextSet(const Bitset& bs, Bitset::BitIndex idx)
        {
            return bs.NextSetBitIndex(idx);
        }
    };

public:
    struct ConstIterator;

    struct Iterator : IteratorBase<Iterator, false>
    {
        using ImplType = typename ResourceTracker::Impl;

        Iterator() = delete;

        Iterator(ResourceTracker* tracker, size_t subclassImplIndex, size_t elementIndex)
            : IteratorBase<Iterator, false>(tracker, subclassImplIndex, elementIndex)
        {
        }

        // Allow implicit conversion to ConstIterator
        operator ConstIterator() const
        {
            return ConstIterator(
                IteratorBase<Iterator, false>::tracker,
                IteratorBase<Iterator, false>::subclassImplIndex,
                IteratorBase<Iterator, false>::elementIndex);
        }
    };

    struct ConstIterator : IteratorBase<ConstIterator, true>
    {
        using ImplType = typename ResourceTracker::Impl;

        ConstIterator() = delete;

        ConstIterator(const ResourceTracker* tracker, size_t subclassImplIndex, size_t elementIndex)
            : IteratorBase<ConstIterator, true>(tracker, subclassImplIndex, elementIndex)
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

    // use a sparse array so we can use IDs as indices
    // without worring about hashing for lookups and allowing us to
    // still iterate over the elements (mostly) linearly.
    using ElementArrayType = SparsePagedArray<ElementType, 256, AllocatorType>;
    using VersionArrayType = SparsePagedArray<int, 256, AllocatorType>; // mirrors elements array

    using ProxyArrayType = SparsePagedArray<ProxyType, 256, AllocatorType>;

    static_assert(std::is_base_of_v<ObjIdBase, IdType>, "IdType must be derived from ObjIdBase (must use numeric id)");

    ResourceTracker()
        : baseImpl(&TypeOf<typename IdType::ObjectType>()), // default impl for base class
          subclassImpls(),
          emptyArray(),
          cachedDiffNeedsUpdate(false)
    {
        // Setup the subclass implementations array, we initialize them as they get used
        const size_t numDescendants = GetNumDescendants(TypeIdOf<typename IdType::ObjectType>());
        subclassImpls.Resize(numDescendants);
    }

    ResourceTracker(const ResourceTracker& other) = delete;
    ResourceTracker& operator=(const ResourceTracker& other) = delete;

    ResourceTracker(ResourceTracker&& other) noexcept = delete;
    ResourceTracker& operator=(ResourceTracker&& other) noexcept = delete;

    virtual ~ResourceTracker() override = default;

    uint32 NumCurrent() const
    {
        HYP_SCOPE;

        uint32 count = baseImpl.next.Count();

        for (Bitset::BitIndex i : subclassIndices)
        {
            AssertDebug(i < subclassImpls.Size());

            count += subclassImpls[i]->next.Count();
        }

        return count;
    }

    uint32 NumCurrent(TypeId typeId) const
    {
        if (typeId == TypeIdOf<typename IdType::ObjectType>())
        {
            return baseImpl.next.Count();
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (subclassIndices.Test(subclassIndex))
        {
            return subclassImpls[subclassIndex]->next.Count();
        }

        return 0;
    }

    const ElementArrayType& GetElements(TypeId typeId) const
    {
        if (typeId == TypeIdOf<typename IdType::ObjectType>())
        {
            return baseImpl.elements;
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (subclassIndices.Test(subclassIndex))
        {
            return subclassImpls[subclassIndex]->elements;
        }

        return emptyArray;
    }

    template <class T>
    HYP_FORCE_INLINE const ElementArrayType& GetElements() const
    {
        static_assert(std::is_base_of_v<typename IdType::ObjectType, T>, "T must be a subclass of the ID's inner type!");

        return GetElements(TypeId::ForType<T>());
    }

    HYP_FORCE_INLINE const Bitset& GetSubclassIndices() const
    {
        return subclassIndices;
    }

    HYP_FORCE_INLINE const Impl& GetSubclassImpl(int subclassIndex) const
    {
        if (subclassIndex == -1)
        {
            return baseImpl;
        }

        AssertDebug(subclassIndex < int(subclassImpls.Size()));
        AssertDebug(subclassIndices.Test(subclassIndex));

        return *subclassImpls[subclassIndex];
    }

    const ResourceTrackerDiff& GetDiff() const
    {
        HYP_SCOPE;

        if (cachedDiffNeedsUpdate)
        {
            cachedDiff = {};
            cachedDiff.numAdded = baseImpl.GetAdded().Count();
            cachedDiff.numRemoved = baseImpl.GetRemoved().Count();
            cachedDiff.numChanged = baseImpl.GetChanged().Count();

            for (Bitset::BitIndex i : subclassIndices)
            {
                AssertDebug(i < subclassImpls.Size());

                auto& impl = *subclassImpls[i];

                cachedDiff.numAdded += impl.GetAdded().Count();
                cachedDiff.numRemoved += impl.GetRemoved().Count();
                cachedDiff.numChanged += impl.GetChanged().Count();
            }

            cachedDiffNeedsUpdate = false;
        }

        return cachedDiff;
    }

    /*! \brief Marks an element to be considered valid for this frame
     *  \param id The ID of the element to track, used as a key / index
     *  \param element The element to track
     *  \param versionPtr Optional pointer to an integer that holds the version of the element. If the version does not match what is stored,
     *  the element will be considered changed and the data marked for update.
     *  \param allowDuplicatesInSameFrame If true, allows the same ID to be tracked multiple times in the same frame. (Can have different values or versions)
     */
    void Track(IdType id, const ElementType& element, const int* versionPtr = nullptr, bool allowDuplicatesInSameFrame = true)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();
        AssertDebug(typeId != TypeId::Void());

        cachedDiffNeedsUpdate = true;

        if (typeId == TypeInfo_GetId(*baseImpl.typeInfo))
        {
            baseImpl.Track(id, element, versionPtr, allowDuplicatesInSameFrame);
            return;
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (!subclassIndices.Test(subclassIndex))
        {
            const Class* cls = GetClass(typeId);
            AssertDebug(cls != nullptr);

            subclassImpls[subclassIndex] = MakePimplWithAllocator<Impl, AllocatorType>(&Class_GetTypeInfo(*cls));
            subclassIndices.Set(subclassIndex, true);
        }

        subclassImpls[subclassIndex]->Track(id, element, versionPtr, allowDuplicatesInSameFrame);
    }

    bool MarkToKeep(IdType id)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();
        AssertDebug(typeId != TypeId::Void());

        cachedDiffNeedsUpdate = true;

        if (typeId == TypeInfo_GetId(*baseImpl.typeInfo))
        {
            return baseImpl.MarkToKeep(id);
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (!subclassIndices.Test(subclassIndex))
        {
            return false;
        }

        return subclassImpls[subclassIndex]->MarkToKeep(id);
    }

    void MarkToRemove(IdType id)
    {
        TypeId typeId = id.GetTypeId();
        AssertDebug(typeId != TypeId::Void());

        cachedDiffNeedsUpdate = true;

        if (typeId == TypeInfo_GetId(*baseImpl.typeInfo))
        {
            baseImpl.MarkToRemove(id);

            return;
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (!subclassIndices.Test(subclassIndex))
        {
            return;
        }

        subclassImpls[subclassIndex]->MarkToRemove(id);
    }

    template <class ArrayAllocatorType>
    void GetRemoved(Array<IdType, ArrayAllocatorType>& outIds, bool includeChanged) const
    {
        HYP_SCOPE;

        baseImpl.GetRemoved(outIds, includeChanged);

        for (Bitset::BitIndex bitIndex : subclassIndices)
        {
            subclassImpls[bitIndex]->GetRemoved(outIds, includeChanged);
        }
    }

    template <class ArrayAllocatorType>
    void GetRemoved(Array<ElementType, ArrayAllocatorType>& out, bool includeChanged) const
    {
        HYP_SCOPE;

        baseImpl.GetRemoved(out, includeChanged);

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->GetRemoved(out, includeChanged);
        }
    }

    template <class ArrayAllocatorType>
    void GetRemoved(Array<ElementType*, ArrayAllocatorType>& out, bool includeChanged) const
    {
        HYP_SCOPE;

        baseImpl.GetRemoved(out, includeChanged);

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->GetRemoved(out, includeChanged);
        }
    }

    template <class ArrayAllocatorType>
    void GetAdded(Array<IdType, ArrayAllocatorType>& outIds, bool includeChanged) const
    {
        HYP_SCOPE;

        baseImpl.GetAdded(outIds, includeChanged);

        for (Bitset::BitIndex bitIndex : subclassIndices)
        {
            subclassImpls[bitIndex]->GetAdded(outIds, includeChanged);
        }
    }

    template <class ArrayAllocatorType>
    void GetAdded(Array<ElementType, ArrayAllocatorType>& out, bool includeChanged) const
    {
        HYP_SCOPE;

        baseImpl.GetAdded(out, includeChanged);

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->GetAdded(out, includeChanged);
        }
    }

    template <class ArrayAllocatorType>
    void GetAdded(Array<ElementType*, ArrayAllocatorType>& out, bool includeChanged) const
    {
        HYP_SCOPE;

        baseImpl.GetAdded(out, includeChanged);

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->GetAdded(out, includeChanged);
        }
    }

    template <class ArrayAllocatorType>
    void GetChanged(Array<IdType, ArrayAllocatorType>& outIds) const
    {
        HYP_SCOPE;

        baseImpl.GetChanged(outIds);

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->GetChanged(outIds);
        }
    }

    template <class ArrayAllocatorType>
    void GetChanged(Array<ElementType, ArrayAllocatorType>& out) const
    {
        HYP_SCOPE;

        baseImpl.GetChanged(out);

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->GetChanged(out);
        }
    }

    template <class ArrayAllocatorType>
    void GetChanged(Array<ElementType*, ArrayAllocatorType>& out) const
    {
        HYP_SCOPE;

        baseImpl.GetChanged(out);

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->GetChanged(out);
        }
    }

    ElementType* GetElement(IdType id)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();

        if (typeId == TypeInfo_GetId(*baseImpl.typeInfo))
        {
            return baseImpl.GetElement(id);
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (!subclassIndices.Test(subclassIndex))
        {
            return nullptr;
        }

        return subclassImpls[subclassIndex]->GetElement(id);
    }

    const ElementType* GetElement(IdType id) const
    {
        return const_cast<ResourceTracker*>(this)->GetElement(id);
    }

    ProxyType* GetProxy(IdType id)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();

        if (typeId == TypeInfo_GetId(*baseImpl.typeInfo))
        {
            return baseImpl.GetProxy(id);
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (!subclassIndices.Test(subclassIndex))
        {
            return nullptr;
        }

        return subclassImpls[subclassIndex]->GetProxy(id);
    }

    const ProxyType* GetProxy(IdType id) const
    {
        return const_cast<ResourceTracker*>(this)->GetProxy(id);
    }

    ProxyType* SetProxy(IdType id, const ProxyType& proxy)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();

        if (typeId == TypeInfo_GetId(*baseImpl.typeInfo))
        {
            return baseImpl.SetProxy(id, proxy);
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (!subclassIndices.Test(subclassIndex))
        {
            const Class* cls = GetClass(typeId);
            AssertDebug(cls != nullptr, "Class for TypeId {} not found", typeId.Value());

            subclassImpls[subclassIndex] = MakePimplWithAllocator<Impl, AllocatorType>(&Class_GetTypeInfo(*cls));
            subclassIndices.Set(subclassIndex, true);
        }

        AssertDebug(TypeInfo_GetId(*subclassImpls[subclassIndex]->typeInfo) == typeId,
            "TypeId mismatch: expected {}, got {}", typeId.Value(), TypeInfo_GetId(*subclassImpls[subclassIndex]->typeInfo).Value());

        return subclassImpls[subclassIndex]->SetProxy(id, proxy);
    }

    ProxyType* SetProxy(IdType id, ProxyType&& proxy)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();

        if (typeId == TypeInfo_GetId(*baseImpl.typeInfo))
        {
            return baseImpl.SetProxy(id, std::move(proxy));
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (!subclassIndices.Test(subclassIndex))
        {
            const Class* cls = GetClass(typeId);
            AssertDebug(cls != nullptr, "Class for TypeId {} not found", typeId.Value());

            subclassImpls[subclassIndex] = MakePimplWithAllocator<Impl, AllocatorType>(&Class_GetTypeInfo(*cls));
            subclassIndices.Set(subclassIndex, true);
        }

        AssertDebug(TypeInfo_GetId(*subclassImpls[subclassIndex]->typeInfo) == typeId,
            "TypeId mismatch: expected {}, got {}", typeId.Value(), TypeInfo_GetId(*subclassImpls[subclassIndex]->typeInfo).Value());

        return subclassImpls[subclassIndex]->SetProxy(id, std::move(proxy));
    }

    void RemoveProxy(IdType id)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();

        if (typeId == TypeInfo_GetId(*baseImpl.typeInfo))
        {
            baseImpl.RemoveProxy(id);
            return;
        }

        const int subclassIndex = GetSubclassIndex(TypeInfo_GetId(*baseImpl.typeInfo), typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < subclassImpls.Size(), "Invalid subclass index");

        if (!subclassIndices.Test(subclassIndex))
        {
            return;
        }

        subclassImpls[subclassIndex]->RemoveProxy(id);
    }

    void Advance(bool clearNextState = true)
    {
        HYP_SCOPE;

        cachedDiffNeedsUpdate = true;

        baseImpl.Advance(clearNextState);

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->Advance(clearNextState);
        }
    }

    void Reset()
    {
        cachedDiffNeedsUpdate = true;

        baseImpl.Reset();

        for (Bitset::BitIndex i : subclassIndices)
        {
            subclassImpls[i]->Reset();
        }
    }

    HYP_DEF_STL_BEGIN_END(Iterator(const_cast<ResourceTracker*>(this), Bitset::NotFound, 0), Iterator(const_cast<ResourceTracker*>(this), Bitset::NotFound, Bitset::NotFound))

    struct Impl final
    {
        Impl(const TypeInfo* typeInfo)
            : typeInfo(typeInfo),
              elements(),
              versions(),
              proxies()
        {
        }

        Impl(const Impl& other) = delete;
        Impl& operator=(const Impl& other) = delete;

        Impl(Impl&& other) noexcept = delete;
        Impl& operator=(Impl&& other) noexcept = delete;

        ~Impl() = default;

        /*! \brief Checks if it already has a proxy for the given Id from the previous frame */
        HYP_FORCE_INLINE bool HasElement(IdType id) const
        {
            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo), "ResourceTracker typeid mismatch");

            return previous.Test(id.ToIndex());
        }

        HYP_FORCE_INLINE Bitset GetAdded() const
        {
            const size_t newNumBits = MathUtil::Max(previous.NumBits(), next.NumBits());

            return Bitset(next).SetNumBits(newNumBits) & ~Bitset(previous).SetNumBits(newNumBits);
        }

        HYP_FORCE_INLINE Bitset GetRemoved() const
        {
            const size_t newNumBits = MathUtil::Max(previous.NumBits(), next.NumBits());

            return Bitset(previous).SetNumBits(newNumBits) & ~Bitset(next).SetNumBits(newNumBits);
        }

        HYP_FORCE_INLINE Bitset GetElementIndices() const
        {
            Bitset bits = previous | next;
            Bitset removed = GetRemoved();

            const size_t newNumBits = MathUtil::Max(bits.NumBits(), removed.NumBits());

            return bits.SetNumBits(newNumBits) & ~removed.SetNumBits(newNumBits);
        }

        HYP_FORCE_INLINE const Bitset& GetChanged() const
        {
            return changed;
        }

        void Track(IdType id, const ElementType& value, const int* pVersion = nullptr, bool allowDuplicatesInSameFrame = true)
        {
            ElementType* pCurrentValue = nullptr;
            int* pCurrentVersion = nullptr;

            ResourceTrackState trackState = ResourceTrackState::UNCHANGED;

            if (!allowDuplicatesInSameFrame)
            {
                AssertDebug(!next.Test(id.ToIndex()), "Element with ID: {} already marked to be added for this iteration!", id);
            }

            if (previous.Test(id.ToIndex()) || next.Test(id.ToIndex()))
            {
                AssertDebug(elements.HasIndex(id.ToIndex()));

                pCurrentValue = &elements.Get(id.ToIndex());
                pCurrentVersion = &versions.Get(id.ToIndex());

                AssertDebug(pCurrentValue && pCurrentVersion);
            }

            bool isDoublyAddedThisFrame = false;

            if (allowDuplicatesInSameFrame)
            {
                if (next.Test(id.ToIndex()))
                {
                    isDoublyAddedThisFrame = true;

                    // already added for this iteration. check if we can just return or if we need to update

                    // if (value == *currentValuePtr && (versionPtr == nullptr || *versionPtr == *currentVersionPtr))
                    // {
                    //     // same values, early out
                    //     return;
                    // }
                }
            }

            if (pCurrentValue != nullptr)
            {
                // elements and versions must be kept in sync
                AssertDebug(pCurrentVersion != nullptr);

                // Advance if version has changed or if elements are not equal
                if (value != *pCurrentValue || (pVersion && *pVersion != *pCurrentVersion))
                {
                    if (!next.Test(id.ToIndex()) && previous.Test(id.ToIndex()))
                    {
                        // Mark as changed if it is found in the previous iteration
                        changed.Set(id.ToIndex(), true);
                    }

                    // update value and version
                    *pCurrentValue = value;
                    *pCurrentVersion = pVersion ? *pVersion : 0;

                    trackState = ResourceTrackState::CHANGED_MODIFIED;
                }
                else
                {
                    trackState = ResourceTrackState::UNCHANGED;
                }
            }
            else
            {
                if (!isDoublyAddedThisFrame)
                {
                    // sanity check - if not in previous iteration, it must not be in the changed list
                    AssertDebug(!changed.Test(id.ToIndex()));
                }

                // use emplace to destruct / reconstruct current element
                elements.Set(id.ToIndex(), value);
                versions.Set(id.ToIndex(), pVersion ? *pVersion : 0);

                trackState = ResourceTrackState::CHANGED_ADDED;
            }

            next.Set(id.ToIndex(), true);
        }

        HYP_FORCE_INLINE bool MarkToKeep(IdType id)
        {
            if (!previous.Test(id.ToIndex()))
            {
                return false;
            }

            next.Set(id.ToIndex(), true);

            return true;
        }

        HYP_FORCE_INLINE void MarkToRemove(IdType id)
        {
            next.Set(id.ToIndex(), false);
        }

        template <class ArrayAllocatorType>
        void GetRemoved(Array<IdType, ArrayAllocatorType>& outIds, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset removedBits = GetRemoved();

            if (includeChanged)
            {
                removedBits |= GetChanged();
            }

            outIds.Reserve(removedBits.Count());

            for (Bitset::BitIndex i : removedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                outIds.PushBack(id);
            }
        }

        template <class ArrayAllocatorType>
        void GetRemoved(Array<ElementType, ArrayAllocatorType>& out, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset removedBits = GetRemoved();

            if (includeChanged)
            {
                removedBits |= GetChanged();
            }

            out.Reserve(out.Size() + removedBits.Count());

            for (Bitset::BitIndex i : removedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);
            }
        }

        template <class ArrayAllocatorType>
        void GetRemoved(Array<ElementType*, ArrayAllocatorType>& out, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset removedBits = GetRemoved();

            if (includeChanged)
            {
                removedBits |= GetChanged();
            }

            out.Reserve(out.Size() + removedBits.Count());

            for (Bitset::BitIndex i : removedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));
            }
        }

        template <class ArrayAllocatorType>
        void GetAdded(Array<IdType, ArrayAllocatorType>& outIds, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset newlyAddedBits = GetAdded();

            if (includeChanged)
            {
                newlyAddedBits |= GetChanged();
            }

            outIds.Reserve(outIds.Size() + newlyAddedBits.Count());

            for (Bitset::BitIndex i : newlyAddedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                outIds.PushBack(id);
            }
        }

        template <class ArrayAllocatorType>
        void GetAdded(Array<ElementType, ArrayAllocatorType>& out, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset newlyAddedBits = GetAdded();

            if (includeChanged)
            {
                newlyAddedBits |= GetChanged();
            }

            out.Reserve(out.Size() + newlyAddedBits.Count());

            for (Bitset::BitIndex i : newlyAddedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);
            }
        }

        template <class ArrayAllocatorType>
        void GetAdded(Array<ElementType*, ArrayAllocatorType>& out, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset newlyAddedBits = GetAdded();

            if (includeChanged)
            {
                newlyAddedBits |= GetChanged();
            }

            out.Reserve(out.Size() + newlyAddedBits.Count());

            for (Bitset::BitIndex i : newlyAddedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));
            }
        }

        template <class ArrayAllocatorType>
        void GetChanged(Array<IdType, ArrayAllocatorType>& outIds) const
        {
            HYP_SCOPE;

            Bitset changedBits = GetChanged();

            outIds.Reserve(changedBits.Count());

            for (Bitset::BitIndex i : changedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                outIds.PushBack(id);
            }
        }

        template <class ArrayAllocatorType>
        void GetChanged(Array<ElementType, ArrayAllocatorType>& out) const
        {
            HYP_SCOPE;

            const Bitset& changedBits = GetChanged();

            out.Reserve(out.Size() + changedBits.Count());

            for (Bitset::BitIndex i : changedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);
            }
        }

        template <class ArrayAllocatorType>
        void GetChanged(Array<ElementType*, ArrayAllocatorType>& out) const
        {
            HYP_SCOPE;

            const Bitset& changedBits = GetChanged();

            out.Reserve(out.Size() + changedBits.Count());

            for (Bitset::BitIndex i : changedBits)
            {
                const IdType id = IdType(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));
            }
        }

        ElementType* GetElement(IdType id)
        {
            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo));

            if (id.GetTypeId() != TypeInfo_GetId(*typeInfo))
            {
                return nullptr;
            }

            return elements.TryGet(id.ToIndex());
        }

        ProxyType* GetProxy(IdType id)
        {
            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo));

            if (id.GetTypeId() != TypeInfo_GetId(*typeInfo))
            {
                return nullptr;
            }

            uint32 idx = id.ToIndex();

            return proxies.TryGet(idx);
        }

        const ProxyType* GetProxy(IdType id) const
        {
            return const_cast<Impl*>(this)->GetProxy(id);
        }

        ProxyType* SetProxy(IdType id, const ProxyType& proxy)
        {
            HYP_SCOPE;

            const uint32 idx = id.ToIndex();

            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo));
            AssertDebug(elements.HasIndex(idx));

            ProxyType* pExistingProxy = proxies.TryGet(idx);
            AssertDebug(pExistingProxy != &proxy);

            if (pExistingProxy)
            {
                (*pExistingProxy) = proxy;

                return pExistingProxy;
            }
            else
            {
                return &*proxies.Set(idx, proxy);
            }
        }

        ProxyType* SetProxy(IdType id, ProxyType&& proxy)
        {
            HYP_SCOPE;

            const uint32 idx = id.ToIndex();

            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo));
            AssertDebug(elements.HasIndex(idx));

            ProxyType* pExistingProxy = proxies.TryGet(idx);
            AssertDebug(pExistingProxy != &proxy);

            if (pExistingProxy)
            {
                (*pExistingProxy) = std::move(proxy);

                return pExistingProxy;
            }
            else
            {
                return &*proxies.Set(idx, std::move(proxy));
            }
        }

        void RemoveProxy(IdType id)
        {
            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo));

            if (id.GetTypeId() != TypeInfo_GetId(*typeInfo))
            {
                return;
            }

            ProxyType* pProxy = proxies.TryGet(id.ToIndex());
            if (!pProxy)
            {
                return;
            }

            proxies.EraseAt(id.ToIndex());
        }

        void Advance(bool clearNextState)
        {
            HYP_SCOPE;

            // Remove element data for removed bits
            for (Bitset::BitIndex index : GetRemoved())
            {
                AssertDebug(elements.HasIndex(index));

                elements.EraseAt(index);
                versions.EraseAt(index);

                if (!clearNextState)
                {
                    next.Set(index, false);
                }
            }

            if (clearNextState)
            {
                previous = std::move(next);
                next.Clear();
            }
            else
            {
                previous = next;
            }

            changed.Clear();
        }

        /*! \brief Total reset of the list, including clearing the previous state. */
        void Reset()
        {
            elements.Clear();
            versions.Clear();

            proxies.Clear();

            previous.Clear();
            next.Clear();
            changed.Clear();
        }

        const TypeInfo* typeInfo;

        ElementArrayType elements;
        // per-element version identifier array - mirrors elements array
        VersionArrayType versions;

        ProxyArrayType proxies;

        Bitset previous;
        Bitset next;
        Bitset changed;
    };

    // base class impl
    Impl baseImpl;

    // per-subtype implementations (only constructed and setup on first Bind() call with that type)
    Array<Pimpl<Impl>, AllocatorType> subclassImpls;
    Bitset subclassIndices;

    ElementArrayType emptyArray; // fallback array when subclass impl not instantiated

    mutable ResourceTrackerDiff cachedDiff;
    mutable bool cachedDiffNeedsUpdate : 1 = true;
};

} // namespace Resources

} // namespace Hyperion
