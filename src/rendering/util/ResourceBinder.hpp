/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/logging/LoggerFwd.hpp>

namespace hyperion {

HYP_API extern SizeType GetNumDescendants(TypeId typeId);
HYP_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

class RenderGlobalState;

HYP_DECLARE_LOG_CHANNEL(Rendering);

struct ResourceBindingAllocatorBase
{
    static constexpr uint32 invalidBinding = ~0u;

    ResourceBindingAllocatorBase(uint32 maxSize)
        : maxSize(maxSize)
    {
    }

    uint32 AllocateIndex()
    {
        const uint32 freeIndex = usedIndices.FirstZeroBitIndex();

        // if maxSize is not ~0u we have to check up to maxSize
        if (maxSize != ~0u && freeIndex >= maxSize)
        {
            return invalidBinding;
        }

        usedIndices.Set(freeIndex, true);

        return freeIndex;
    }

    void FreeIndex(uint32 index)
    {
        if (!usedIndices.Test(index))
        {
            // already not set
            return;
        }

        if ((maxSize != ~0u && index >= maxSize) || index == invalidBinding)
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

class HYP_API ResourceBinderBase
{
public:
    virtual ~ResourceBinderBase() = default;

    HYP_FORCE_INLINE ResourceBindingAllocatorBase* GetBindingAllocator() const
    {
        return m_bindingAllocator;
    }

    // Mark the object to be considered to be a bound resource for the current frame
    virtual void Consider(HypObjectBase* object) = 0;

    // Remove the object from being considered to be bound
    virtual void Deconsider(HypObjectBase* object) = 0;

    // Assign / remove bindings for resources (call after all Consider()/Deconsider() calls)
    virtual void ApplyUpdates() = 0;

    // Get a bitset containing all the bound resources of a given type.
    virtual const Bitset& GetBoundIndices(TypeId typeId) const = 0;

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
 *  Only static subclasses are supported so using types extended only from managed code will not work. (See HypClass::GetStaticIndex)
 *  \note This system is not thread safe and should only be used from a single thread at any given time */
template <class T, auto OnBindingChanged = (void (*)(T*, uint32, uint32)) nullptr>
class ResourceBinder : public ResourceBinderBase
{
    struct Impl final
    {
        Impl(TypeId typeId)
            : typeId(typeId)
        {
        }

        void ReleaseBindings(ResourceBindingAllocatorBase* allocator)
        {
            // Unbind all objects that were bound in the last frame
            for (Bitset::BitIndex i : lastFrameIds)
            {
                const ObjId<T> id = ObjId<T>(ObjIdBase { typeId, uint32(i + 1) });

                const auto it = bindings.FindAs(id);
                AssertDebug(it != bindings.End());

                if (it != bindings.End())
                {
                    T* object = it->first.GetUnsafe();
                    const uint32 binding = it->second;

                    if (OnBindingChanged != nullptr)
                    {
                        OnBindingChanged(object, binding, ResourceBindingAllocatorBase::invalidBinding);
                    }

                    allocator->FreeIndex(binding);
                    bindings.Erase(it);
                }
            }

            AssertDebug(bindings.Empty());
        }

        void Consider(ResourceBindingAllocatorBase* allocator, HypObjectBase* object)
        {
            ObjIdBase id = object->Id();

            if (!id.IsValid())
            {
                return;
            }

            AssertDebug(id.GetTypeId() == typeId);

            currentFrameIds.Set(id.ToIndex(), true);
        }

        void Deconsider(ResourceBindingAllocatorBase* allocator, HypObjectBase* object)
        {
            ObjIdBase id = object->Id();

            if (!id.IsValid())
            {
                return;
            }
            
            AssertDebug(id.GetTypeId() == typeId);

            currentFrameIds.Set(id.ToIndex(), false);
        }

        void ApplyUpdates(ResourceBindingAllocatorBase* allocator)
        {
            const Bitset removed = GetRemoved();
            const Bitset newlyAdded = GetNewlyAdded();
            const Bitset after = (lastFrameIds & ~removed) | newlyAdded;

            AssertDebug(after.Count() <= allocator->maxSize);

            // NOTE: We do removed bits first, to free up slots for the newly added elements to claim a binding index.
            if (removed.AnyBitsSet())
            {
                Array<KeyValuePair<WeakHandle<T>, uint32>> removedElements;
                removedElements.Reserve(removed.Count());

                for (Bitset::BitIndex i : removed)
                {
                    const ObjId<T> id = ObjId<T>(ObjIdBase { typeId, uint32(i + 1) });

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
                        OnBindingChanged(object, binding, ResourceBindingAllocatorBase::invalidBinding);
                    }

                    allocator->FreeIndex(binding);
                }
            }

            for (Bitset::BitIndex i : newlyAdded)
            {
                const ObjId<T> id = ObjId<T>(ObjIdBase { typeId, uint32(i + 1) });

                if (bindings.FindAs(id) != bindings.End())
                {
                    // already bound
                    continue;
                }

                const uint32 index = allocator->AllocateIndex();
                if (index == ResourceBindingAllocatorBase::invalidBinding)
                {
                    HYP_LOG(Rendering, Warning, "ResourceBinder<{}>: Maximum size of {} reached, cannot bind more objects!",
                        TypeNameWithoutNamespace<T>().Data(),
                        allocator->maxSize);

                    continue; // no more space to bind
                }

                auto insertResult = bindings.Insert(WeakHandle<T> { id }, index);
                AssertDebug(insertResult.second, "Failed to insert binding for object with ID {}- it should not already exist!", id);

                if (OnBindingChanged != nullptr)
                {
                    OnBindingChanged(insertResult.first->first.GetUnsafe(), ResourceBindingAllocatorBase::invalidBinding, index);
                }
            }

            lastFrameIds = currentFrameIds;
        }

        HYP_FORCE_INLINE Bitset GetNewlyAdded() const
        {
            const SizeType count = MathUtil::Max(lastFrameIds.NumBits(), currentFrameIds.NumBits());

            return Bitset(currentFrameIds).SetNumBits(count) & ~Bitset(lastFrameIds).SetNumBits(count);
        }

        HYP_FORCE_INLINE Bitset GetRemoved() const
        {
            const SizeType count = MathUtil::Max(lastFrameIds.NumBits(), currentFrameIds.NumBits());

            return Bitset(lastFrameIds).SetNumBits(count) & ~Bitset(currentFrameIds).SetNumBits(count);
        }

        TypeId typeId;
        // these bitsets are used to track which objects were bound in the last frame with bitwise operations
        Bitset lastFrameIds;
        Bitset currentFrameIds;
        HashMap<WeakHandle<T>, uint32> bindings;
    };

public:
    ResourceBinder(ResourceBindingAllocatorBase* bindingAllocator)
        : ResourceBinderBase(bindingAllocator),
          m_impl(TypeId::ForType<T>())
    {
        AssertDebug(m_bindingAllocator != nullptr);

        const SizeType numDescendants = GetNumDescendants(TypeId::ForType<T>());

        // Create storage for subclass implementations
        // subclasses use a bitset (indexing by the subclass' StaticIndex) to determine which implementations are initialized
        m_subclassImpls.Resize(numDescendants);
    }

    ResourceBinder(const ResourceBinder&) = delete;
    ResourceBinder& operator=(const ResourceBinder&) = delete;
    ResourceBinder(ResourceBinder&&) noexcept = delete;
    ResourceBinder& operator=(ResourceBinder&&) noexcept = delete;

    virtual ~ResourceBinder() override
    {
        m_impl.ReleaseBindings(m_bindingAllocator);

        // Loop over the set bits and destruct subclass impls
        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            AssertDebug(i < m_subclassImpls.Size());

            Impl& impl = m_subclassImpls[i].Get();
            impl.ReleaseBindings(m_bindingAllocator);

            m_subclassImpls[i].Destruct();
        }
    }

    virtual void Consider(HypObjectBase* object) override
    {
        if (!object)
        {
            return;
        }

        constexpr TypeId baseTypeId = TypeId::ForType<T>();
        const TypeId objectTypeId = object->GetTypeId();

        if (objectTypeId == baseTypeId)
        {
            m_impl.Consider(m_bindingAllocator, object);

            return;
        }

        const int subclassIndex = GetSubclassIndex(baseTypeId, objectTypeId);
        AssertDebug(subclassIndex >= 0 && subclassIndex < int(m_subclassImpls.Size()),
            "ResourceBinder<{}>: Attempted to bind object with TypeId {} which is not a subclass of the expected TypeId ({}) or has no static index",
            TypeNameWithoutNamespace<T>().Data(), objectTypeId.Value(), baseTypeId.Value());

        if (!m_subclassImplsInitialized.Test(subclassIndex))
        {
            m_subclassImpls[subclassIndex].Construct(objectTypeId);
            m_subclassImplsInitialized.Set(subclassIndex, true);
        }

        Impl& impl = m_subclassImpls[subclassIndex].Get();

        impl.Consider(m_bindingAllocator, object);
    }

    virtual void Deconsider(HypObjectBase* object) override
    {
        AssertDebug(object != nullptr);

        constexpr TypeId typeId = TypeId::ForType<T>();

        if (object->GetTypeId() == typeId)
        {
            m_impl.Deconsider(m_bindingAllocator, object);
        }
        else
        {
            const int subclassIndex = GetSubclassIndex(typeId, object->GetTypeId());
            AssertDebug(subclassIndex >= 0 && subclassIndex < int(m_subclassImpls.Size()),
                "ResourceBinder<{}>: Attempted to Deconsider object with TypeId {} which is not a subclass of the expected TypeId ({}) or has no static index",
                TypeNameWithoutNamespace<T>().Data(), object->GetTypeId().Value(), typeId.Value());

            if (!m_subclassImplsInitialized.Test(subclassIndex))
            {
                // don't do anything if not set here since we're just Deconsidering
                return;
            }

            Impl& impl = m_subclassImpls[subclassIndex].Get();
            impl.Deconsider(m_bindingAllocator, object);
        }
    }

    virtual void ApplyUpdates() override
    {
        m_impl.ApplyUpdates(m_bindingAllocator);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            Impl& impl = m_subclassImpls[i].Get();
            impl.ApplyUpdates(m_bindingAllocator);
        }
    }

    virtual const Bitset& GetBoundIndices(TypeId typeId) const override
    {
        static const Bitset emptyBitset;

        constexpr TypeId baseTypeId = TypeId::ForType<T>();

        if (typeId == TypeId::Void())
        {
            return emptyBitset;
        }

        if (typeId == baseTypeId)
        {
            return m_impl.lastFrameIds;
        }
        else
        {
            const int subclassIndex = GetSubclassIndex(baseTypeId, typeId);
            AssertDebug(subclassIndex >= 0 && subclassIndex < int(m_subclassImpls.Size()),
                "ResourceBinder<{}>: Attempted to get bound indices for TypeId {} which is not a subclass of the expected TypeId ({}) or has no static index",
                TypeNameWithoutNamespace<T>().Data(), typeId.Value(), baseTypeId.Value());

            if (!m_subclassImplsInitialized.Test(subclassIndex))
            {
                // not initialized, return empty bitset
                return emptyBitset;
            }

            return m_subclassImpls[subclassIndex].Get().lastFrameIds;
        }
    }

    virtual uint32 TotalBoundResources() const override
    {
        uint32 total = m_impl.currentFrameIds.Count();

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            total += m_subclassImpls[i].Get().currentFrameIds.Count();
        }

        return total;
    }

protected:
    // base class impl
    Impl m_impl;

    // per-subtype implementations (only constructed and setup on first Consider() call with that type)
    Array<ValueStorage<Impl>> m_subclassImpls;
    Bitset m_subclassImplsInitialized;
};

} // namespace hyperion
