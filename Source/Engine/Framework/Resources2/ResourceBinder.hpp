/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>

#include <Core/logging/LoggerFwd.hpp>

#include <Rendering/Shared.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

CORE_API extern size_t GetNumDescendants(TypeId typeId);
CORE_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

namespace Resources {

template <class T>
void OnBindingChanged_Default(T* resource, uint32 prev, uint32 next)
{
    SetBinding(resource, next);
}

struct ResourceBindingAllocatorBase
{
    explicit ResourceBindingAllocatorBase(uint32 maxSize)
        : maxSize(maxSize)
    {
    }

    HYP_FORCE_INLINE uint32 AllocateIndex()
    {
        const uint32 freeIndex = usedIndices.FirstZeroBitIndex();

        // if maxSize is not ~0u we have to check up to maxSize
        if (maxSize != ~0u && freeIndex >= maxSize)
        {
            return InvalidBinding;
        }

        usedIndices.Set(freeIndex, true);

        return freeIndex;
    }

    HYP_FORCE_INLINE void FreeIndex(uint32 index)
    {
        if (!usedIndices.Test(index))
        {
            // already not set
            return;
        }

        if ((maxSize != ~0u && index >= maxSize) || index == InvalidBinding)
        {
            return;
        }

        usedIndices.Set(index, false);
    }

    // the maximum size of this allocator, i.e. the maximum number of bindings that can be allocated in a single frame
    // if set to ~0u, no limit is used when allocating indices
    const uint32 maxSize;

    // Bits representing whether an index is allocated or not.
    // we find free indices by iterating over this bitset to find the first unset bit.
    Bitset usedIndices;
};

template <uint32 MaxSize = ~0u>
struct ResourceBindingAllocator : ResourceBindingAllocatorBase
{
    ResourceBindingAllocator()
        : ResourceBindingAllocatorBase(MaxSize)
    {
    }
};

class ENGINE_API ResourceBinderBase
{
public:
    virtual ~ResourceBinderBase() = default;

    HYP_FORCE_INLINE ResourceBindingAllocatorBase* GetBindingAllocator() const
    {
        return m_bindingAllocator;
    }

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    /*! \brief Mark the object to be considered to be a bound resource for the current frame
     *   \param pResource The resource to consider
     *   \param forceRebind Should we force rebind the object even if it was already bound? (Useful in the case of updating a texture descriptor, for example)
     */
    virtual void Consider(ObjectBase* pResource, bool forceRebind = false) = 0;

    /*! \brief Unmark the object from being considered to be a bound resource for the current frame
     *  \param pResource The resource to deconsider
     */
    virtual void Deconsider(ObjectBase* pResource) = 0;

    /*! \brief Set whether the given resource should be forced to rebind on the next ApplyUpdates() call
     *  \param pResource The resource to set force rebind for
     *  \param forceRebind Whether to force rebind or not
     */
    virtual void SetForceRebind(ObjectBase* pResource, bool forceRebind = true) = 0;

    virtual uint32 GetBindingForObject(ObjectBase* pResource) const = 0;

    // Assign / remove bindings for resources (call after all Consider()/Deconsider() calls)
    virtual void ApplyUpdates() = 0;

    // Get a bitset containing all the bound resources of a given type.
    virtual const TBitset<RenderAllocator>& GetBoundIndices(TypeId typeId) const = 0;

    virtual uint32 TotalBoundResources() const = 0;

protected:
    ResourceBinderBase(ResourceBindingAllocatorBase* bindingAllocator)
        : m_bindingAllocator(bindingAllocator)
    {
    }

    ResourceBindingAllocatorBase* m_bindingAllocator;
};

/*! \brief This class manages bindings slots for objects of a given resource type. Subclasses of T are also able to be managed,
 *  So binding an instance of e.g ReflectionProbe can be put into the same group of slots as SkyProbe if given the same allocator instance.
 *  Only static subclasses are supported so using types extended only from managed code will not work. (See Class::GetStaticIndex)
 *  \note This system is not thread safe and should only be used from a single thread at any given time */
template <class T, auto OnBindingChanged = &OnBindingChanged_Default<T> >
class ResourceBinder : public ResourceBinderBase
{
    using BitsetType = TBitset<RenderAllocator>;

    struct Impl final
    {
        explicit Impl(const TypeInfo* typeInfo)
            : typeInfo(typeInfo)
        {
        }

        void ReleaseBindings(ResourceBindingAllocatorBase* allocator)
        {
            // Unbind all objects that were bound in the last frame
            for (typename BitsetType::BitIndex i : lastFrameIds)
            {
                const ObjId<T> id = ObjId<T>(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(i + 1) });

                const auto it = bindings.FindAs(id);
                AssertDebug(it != bindings.End());

                if (it != bindings.End())
                {
                    T* object = it->first.GetUnsafe();
                    const uint32 binding = it->second;

                    if (OnBindingChanged != nullptr)
                    {
                        OnBindingChanged(object, binding, InvalidBinding);
                    }

                    allocator->FreeIndex(binding);
                    bindings.Erase(it);
                }
            }

            AssertDebug(bindings.Empty());
        }

        void Consider(ResourceBindingAllocatorBase* allocator, ObjectBase* pResource, bool forceRebind = false)
        {
            ObjIdBase id = pResource->Id();

            if (!id.IsValid())
            {
                return;
            }

            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo));

            currentFrameIds.Set(id.ToIndex(), true);

            if (forceRebind)
            {
                forceRebindBits.Set(id.ToIndex(), true);
            }
        }

        void Deconsider(ResourceBindingAllocatorBase* allocator, ObjectBase* pResource)
        {
            ObjIdBase id = pResource->Id();

            if (!id.IsValid())
            {
                return;
            }

            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo));

            currentFrameIds.Set(id.ToIndex(), false);
        }

        void SetForceRebind(ObjectBase* pResource, bool forceRebind = true)
        {
            ObjIdBase id = pResource->Id();
            AssertDebug(id.IsValid());
            AssertDebug(id.GetTypeId() == TypeInfo_GetId(*typeInfo));

            forceRebindBits.Set(id.ToIndex(), forceRebind);
        }

        void ApplyUpdates(ResourceBindingAllocatorBase* allocator)
        {
            const BitsetType removed = GetRemoved();
            const BitsetType newlyAdded = GetNewlyAdded();
            const BitsetType after = (lastFrameIds & ~removed) | newlyAdded;
            const BitsetType unchanged = currentFrameIds & lastFrameIds;

            AssertDebug(after.Count() <= allocator->maxSize);

            // HYP_LOG_TEMP("Num {} resources: {} (added {}, removed {}, unchanged {})", typeInfo->name, after.Count(), newlyAdded.Count(), removed.Count(), unchanged.Count());

            // NOTE: We do removed bits first, to free up slots for the newly added elements to claim a binding index.
            if (removed.AnyBitsSet())
            {
                Array<KeyValuePair<WeakHandle<T>, uint32>, RenderTempAllocator> removedElements;
                removedElements.Reserve(removed.Count());

                for (typename BitsetType::BitIndex bit : removed)
                {
                    if (forceRebindBits.Test(bit))
                    {
                        forceRebindBits.Set(bit, false);
                    }

                    const ObjId<T> id = ObjId<T>(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(bit + 1) });

                    auto it = bindings.FindAs(id);
                    AssertDebug(it != bindings.End());

                    if (it != bindings.End())
                    {
                        removedElements.PushBack(std::move(*it));
                        bindings.Erase(it);
                    }
                }

                for (KeyValuePair<WeakHandle<T>, uint32>& it : removedElements)
                {
                    T* object = it.first.GetUnsafe();
                    AssertDebug(object != nullptr);

                    const uint32 binding = it.second;

                    if (OnBindingChanged != nullptr)
                    {
                        OnBindingChanged(object, binding, InvalidBinding);
                    }

                    allocator->FreeIndex(binding);
                }
            }

            for (typename BitsetType::BitIndex bit : newlyAdded)
            {
                if (forceRebindBits.Test(bit))
                {
                    // Don't need to force rebind
                    forceRebindBits.Set(bit, false);
                }

                const ObjId<T> id = ObjId<T>(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(bit + 1) });

                if (bindings.FindAs(id) != bindings.End())
                {
                    // already bound
                    continue;
                }

                const uint32 binding = allocator->AllocateIndex();
                if (binding == InvalidBinding)
                {
                    HYP_LOG(Rendering, Warning, "ResourceBinder<{}>: Maximum size of {} reached, cannot bind more objects!",
                        TypeName<T>().Data(),
                        allocator->maxSize);

                    continue; // no more space to bind
                }

                auto insertResult = bindings.Insert(WeakHandle<T> { id }, binding);
                AssertDebug(insertResult.second, "Failed to insert binding for object with ID {}- it should not already exist!", id);

                if (OnBindingChanged != nullptr)
                {
                    OnBindingChanged(insertResult.first->first.GetUnsafe(), InvalidBinding, binding);
                }
            }

            if (forceRebindBits.AnyBitsSet())
            {
                // go through unchanged and compare versions to see if we need to unbind+rebind to same slot
                for (typename BitsetType::BitIndex bit : forceRebindBits)
                {
                    const ObjId<T> id = ObjId<T>(ObjIdBase { TypeInfo_GetId(*typeInfo), uint32(bit + 1) });

                    auto it = bindings.FindAs(id);
                    AssertDebug(it != bindings.End());

                    if (it != bindings.End())
                    {
                        T* object = it->first.GetUnsafe();
                        const uint32 binding = it->second;

                        if (OnBindingChanged != nullptr)
                        {
                            OnBindingChanged(object, binding, InvalidBinding);
                            OnBindingChanged(object, InvalidBinding, binding);
                        }
                    }
                }

                forceRebindBits.Clear();
            }

            lastFrameIds = currentFrameIds;
        }

        HYP_FORCE_INLINE BitsetType GetNewlyAdded() const
        {
            const size_t count = MathUtil::Max(lastFrameIds.NumBits(), currentFrameIds.NumBits());

            return BitsetType(currentFrameIds).SetNumBits(count) & ~BitsetType(lastFrameIds).SetNumBits(count);
        }

        HYP_FORCE_INLINE BitsetType GetRemoved() const
        {
            const size_t count = MathUtil::Max(lastFrameIds.NumBits(), currentFrameIds.NumBits());

            return BitsetType(lastFrameIds).SetNumBits(count) & ~BitsetType(currentFrameIds).SetNumBits(count);
        }

        const TypeInfo* typeInfo;
        // these bitsets are used to track which objects were bound in the last frame with bitwise operations
        BitsetType lastFrameIds;
        BitsetType currentFrameIds;
        BitsetType forceRebindBits;
        TMap<WeakHandle<T>, uint32> bindings;
    };

public:
    explicit ResourceBinder(ResourceBindingAllocatorBase* bindingAllocator)
        : ResourceBinderBase(bindingAllocator),
          m_baseImpl(nullptr)
    {
    }

    ResourceBinder(const ResourceBinder&) = delete;
    ResourceBinder& operator=(const ResourceBinder&) = delete;

    ResourceBinder(ResourceBinder&&) noexcept = delete;
    ResourceBinder& operator=(ResourceBinder&&) noexcept = delete;

    virtual ~ResourceBinder() override
    {
        Assert(!IsInitialized(), "Shutdown() not called");
    }

    virtual void Initialize() override
    {
        if (IsInitialized())
        {
            return;
        }

        AssertDebug(m_bindingAllocator != nullptr);

        AssertDebug(m_emptyBitset == nullptr);
        m_emptyBitset = new BitsetType;

        AssertDebug(m_baseImpl == nullptr);
        m_baseImpl = new Impl(&TypeOf<T>());

        const size_t numDescendants = GetNumDescendants(TypeId::ForType<T>());

        // Create storage for subclass implementations
        // subclasses use a bitset (indexing by the subclass' StaticIndex) to determine which implementations are initialized
        m_subclassImpls.Resize(numDescendants);
    }

    virtual void Shutdown() override
    {
        if (!IsInitialized())
        {
            return;
        }

        if (m_emptyBitset)
        {
            delete m_emptyBitset;
            m_emptyBitset = nullptr;
        }

        if (m_baseImpl)
        {
            m_baseImpl->ReleaseBindings(m_bindingAllocator);

            delete m_baseImpl;
            m_baseImpl = nullptr;
        }

        // Loop over the set bits and destruct subclass impls
        for (typename BitsetType::BitIndex i : m_subclassImplsInitialized)
        {
            AssertDebug(i < m_subclassImpls.Size());

            Impl& impl = m_subclassImpls[i].Get();
            impl.ReleaseBindings(m_bindingAllocator);

            m_subclassImpls[i].Destruct();
        }
    }

    virtual void Consider(ObjectBase* pResource, bool forceRebind = false) override
    {
        if (!pResource)
        {
            return;
        }

        const TypeId baseTypeId = TypeId::ForType<T>();
        const TypeId objectTypeId = GetTypeIdForClass(pResource->InstanceClass());

        if (objectTypeId == baseTypeId)
        {
            m_baseImpl->Consider(m_bindingAllocator, pResource, forceRebind);

            return;
        }

        const int subclassIndex = GetSubclassIndex(baseTypeId, objectTypeId);
        AssertDebug(subclassIndex >= 0 && subclassIndex < int(m_subclassImpls.Size()),
            "ResourceBinder<{}>: Attempted to bind object with TypeId {} which is not a subclass of the expected TypeId ({}) or has no static index",
            TypeName<T>().Data(), objectTypeId.Value(), baseTypeId.Value());

        if (!m_subclassImplsInitialized.Test(subclassIndex))
        {
            m_subclassImpls[subclassIndex].Construct(pResource->InstanceClass()->GetTypeInfo());
            m_subclassImplsInitialized.Set(subclassIndex, true);
        }

        Impl& impl = m_subclassImpls[subclassIndex].Get();
        impl.Consider(m_bindingAllocator, pResource, forceRebind);
    }

    virtual void Deconsider(ObjectBase* pResource) override
    {
        AssertDebug(pResource != nullptr);

        const TypeId typeId = TypeId::ForType<T>();
        const TypeId objectTypeId = GetTypeIdForClass(pResource->InstanceClass());

        if (objectTypeId == typeId)
        {
            m_baseImpl->Deconsider(m_bindingAllocator, pResource);
        }
        else
        {
            const int subclassIndex = GetSubclassIndex(typeId, objectTypeId);
            AssertDebug(subclassIndex >= 0 && subclassIndex < int(m_subclassImpls.Size()),
                "ResourceBinder<{}>: Attempted to Deconsider object with TypeId {} which is not a subclass of the expected TypeId ({}) or has no static index",
                TypeName<T>().Data(), objectTypeId.Value(), typeId.Value());

            if (!m_subclassImplsInitialized.Test(subclassIndex))
            {
                // don't do anything if not set here since we're just Deconsidering
                return;
            }

            Impl& impl = m_subclassImpls[subclassIndex].Get();
            impl.Deconsider(m_bindingAllocator, pResource);
        }
    }

    virtual void SetForceRebind(ObjectBase* pResource, bool forceRebind = true) override
    {
        AssertDebug(pResource != nullptr);

        const TypeId typeId = TypeId::ForType<T>();
        const TypeId objectTypeId = GetTypeIdForClass(pResource->InstanceClass());

        if (objectTypeId == typeId)
        {
            m_baseImpl->SetForceRebind(pResource, forceRebind);
        }
        else
        {
            const int subclassIndex = GetSubclassIndex(typeId, objectTypeId);
            AssertDebug(subclassIndex >= 0 && subclassIndex < int(m_subclassImpls.Size()),
                "ResourceBinder<{}>: Attempted to set force rebind for object with TypeId {} which is not a subclass of the expected TypeId ({}) or has no static index",
                TypeName<T>().Data(), objectTypeId.Value(), typeId.Value());

            AssertDebug(m_subclassImplsInitialized.Test(subclassIndex));

            Impl& impl = m_subclassImpls[subclassIndex].Get();
            impl.SetForceRebind(pResource, forceRebind);
        }
    }

    virtual uint32 GetBindingForObject(ObjectBase* pResource) const override
    {
        AssertDebug(pResource != nullptr);

        const TypeId typeId = TypeId::ForType<T>();
        const TypeId objectTypeId = GetTypeIdForClass(pResource->InstanceClass());

        if (objectTypeId == typeId)
        {
            const ObjIdBase id = pResource->Id();
            const auto it = m_baseImpl->bindings.FindAs(ObjId<T>(id));

            if (it != m_baseImpl->bindings.End())
            {
                return it->second;
            }
        }
        else
        {
            const int subclassIndex = GetSubclassIndex(typeId, objectTypeId);

            AssertDebug(subclassIndex >= 0 && subclassIndex < int(m_subclassImpls.Size()),
                "ResourceBinder<{}>: Attempted to get binding for object with TypeId {} which is not a subclass of the expected TypeId ({}) or has no static index",
                TypeName<T>().Data(), objectTypeId.Value(), typeId.Value());

            if (!m_subclassImplsInitialized.Test(subclassIndex))
            {
                return InvalidBinding;
            }

            const Impl& impl = m_subclassImpls[subclassIndex].Get();
            const ObjIdBase id = pResource->Id();
            const auto it = impl.bindings.FindAs(ObjId<T>(id));

            if (it != impl.bindings.End())
            {
                return it->second;
            }
        }

        return InvalidBinding;
    }

    virtual void ApplyUpdates() override
    {
        m_baseImpl->ApplyUpdates(m_bindingAllocator);

        for (typename BitsetType::BitIndex i : m_subclassImplsInitialized)
        {
            Impl& impl = m_subclassImpls[i].Get();
            impl.ApplyUpdates(m_bindingAllocator);
        }
    }

    virtual const BitsetType& GetBoundIndices(TypeId typeId) const override
    {
        const TypeId baseTypeId = TypeId::ForType<T>();

        if (typeId == TypeId::Void())
        {
            return *m_emptyBitset;
        }

        if (typeId == baseTypeId)
        {
            return m_baseImpl->lastFrameIds;
        }
        else
        {
            const int subclassIndex = GetSubclassIndex(baseTypeId, typeId);
            AssertDebug(subclassIndex >= 0 && subclassIndex < int(m_subclassImpls.Size()),
                "ResourceBinder<{}>: Attempted to get bound indices for TypeId {} which is not a subclass of the expected TypeId ({}) or has no static index",
                TypeName<T>().Data(), typeId.Value(), baseTypeId.Value());

            if (!m_subclassImplsInitialized.Test(subclassIndex))
            {
                // not initialized, return empty bitset
                return *m_emptyBitset;
            }

            return m_subclassImpls[subclassIndex].Get().lastFrameIds;
        }
    }

    virtual uint32 TotalBoundResources() const override
    {
        uint32 total = m_baseImpl->currentFrameIds.Count();

        for (typename BitsetType::BitIndex i : m_subclassImplsInitialized)
        {
            total += m_subclassImpls[i].Get().currentFrameIds.Count();
        }

        return total;
    }

protected:
    bool IsInitialized() const
    {
        return m_baseImpl != nullptr;
    }

    // base class impl
    Impl* m_baseImpl;

    // per-subtype implementations (only constructed and setup on first Consider() call with that type)
    Array<ValueStorage<Impl>, FixedAllocator<32>> m_subclassImpls;
    TBitset<FixedAllocator<32>> m_subclassImplsInitialized;

    BitsetType* m_emptyBitset;
};

} // namespace Resources

} // namespace Hyperion
