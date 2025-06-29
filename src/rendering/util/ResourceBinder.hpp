/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RESOURCE_BINDER_HPP
#define HYPERION_RESOURCE_BINDER_HPP

#include <core/memory/UniquePtr.hpp>

#include <core/Handle.hpp>

namespace hyperion {

HYP_API extern SizeType GetNumDescendants(TypeId type_id);
HYP_API extern int GetSubclassIndex(TypeId base_type_id, TypeId subclass_type_id);

class RenderGlobalState;

struct ResourceBindingAllocatorBase
{
    static constexpr uint32 invalid_binding = ~0u;

    ResourceBindingAllocatorBase(uint32 max_size)
        : max_size(max_size)
    {
        if (max_size != ~0u)
        {
            used_indices.Resize(max_size);
        }
    }

    uint32 AllocateIndex()
    {
        const uint32 free_index = used_indices.FirstZeroBitIndex();

        // if max_size is not ~0u we have to check up to max_size
        if (max_size != ~0u && free_index >= max_size)
        {
            return invalid_binding;
        }

        used_indices.Set(free_index, true);

        return free_index;
    }

    void FreeIndex(uint32 index)
    {
        if (!used_indices.Test(index))
        {
            // already not set
            return;
        }

        if ((max_size != ~0u && index >= max_size) || index == invalid_binding)
        {
            return;
        }

        used_indices.Set(index, false);
    }

    void ResetStat()
    {
        current_frame_count = 0;
    }

    // the maximum size of this allocator, i.e. the maximum number of bindings that can be allocated in a single frame
    // if set to ~0u, no limit is used when allocating indices
    const uint32 max_size;

    // number of claims for the current from
    // NOTE: not same as bindings (bindings are set after ApplyUpdates() is called so we can calculate reuse)
    uint32 current_frame_count = 0;

    // Bits representing whether an index is allocated or not.
    // we find free indices by iterating over this bitset to find the first unset bit.
    Bitset used_indices;
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
        return m_binding_allocator;
    }

    // Mark the object to be considered to be a bound resource for the current frame
    virtual void Consider(HypObjectBase* object) = 0;

    // Remove the object from being considered to be bound
    virtual void Deconsider(HypObjectBase* object) = 0;

    // Assign / remove bindings for resources (call after all Consider()/Deconsider() calls)
    virtual void ApplyUpdates() = 0;

    // Get a bitset containing all the bound resources of a given type.
    virtual const Bitset& GetBoundIndices(TypeId type_id) const = 0;

protected:
    ResourceBinderBase(ResourceBindingAllocatorBase* binding_allocator)
        : m_binding_allocator(binding_allocator)
    {
    }

    ResourceBindingAllocatorBase* m_binding_allocator;
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
        Impl(TypeId type_id)
            : type_id(type_id)
        {
        }

        void ReleaseBindings(ResourceBindingAllocatorBase* allocator)
        {
            DebugLog(LogType::Debug, "ResourceBinder<%s>::ReleaseBindings() called\n", TypeNameWithoutNamespace<T>().Data());
            // Unbind all objects that were bound in the last frame
            for (Bitset::BitIndex i : last_frame_ids)
            {
                const ObjId<T> id = ObjId<T>(ObjIdBase { type_id, uint32(i + 1) });

                const auto it = bindings.FindAs(id);
                AssertDebug(it != bindings.End());

                if (it != bindings.End())
                {
                    T* object = it->first.GetUnsafe();
                    const uint32 binding = it->second;

                    if (OnBindingChanged != nullptr)
                    {
                        OnBindingChanged(object, binding, ResourceBindingAllocatorBase::invalid_binding);
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

            ++allocator->current_frame_count;

            current_frame_ids.Set(id.ToIndex(), true);
        }

        void Deconsider(ResourceBindingAllocatorBase* allocator, HypObjectBase* object)
        {
            ObjIdBase id = object->Id();

            if (!id.IsValid())
            {
                return;
            }

            current_frame_ids.Set(id.ToIndex(), false);
        }

        void ApplyUpdates(ResourceBindingAllocatorBase* allocator)
        {
            const Bitset removed = GetRemoved();
            const Bitset newly_added = GetNewlyAdded();
            const Bitset after = (last_frame_ids & ~removed) | newly_added;

            AssertDebug(after.Count() <= allocator->max_size);

            if (removed.AnyBitsSet())
            {
                Array<KeyValuePair<WeakHandle<T>, uint32>> removed_elements;
                removed_elements.Reserve(removed.Count());

                for (Bitset::BitIndex i : removed)
                {
                    const ObjId<T> id = ObjId<T>(ObjIdBase { type_id, uint32(i + 1) });

                    auto it = bindings.FindAs(id);
                    AssertDebug(it != bindings.End());

                    if (it != bindings.End())
                    {
                        removed_elements.PushBack(std::move(*it));
                        bindings.Erase(it);
                    }
                }

                for (KeyValuePair<WeakHandle<T>, uint32>& it : removed_elements)
                {
                    T* object = it.first.GetUnsafe();
                    AssertDebug(object != nullptr);

                    const uint32 binding = it.second;

                    if (OnBindingChanged != nullptr)
                    {
                        OnBindingChanged(object, binding, ResourceBindingAllocatorBase::invalid_binding);
                    }

                    allocator->FreeIndex(binding);
                }
            }

            for (Bitset::BitIndex i : newly_added)
            {
                const ObjId<T> id = ObjId<T>(ObjIdBase { type_id, uint32(i + 1) });

                if (bindings.FindAs(id) != bindings.End())
                {
                    // already bound
                    continue;
                }

                const uint32 index = allocator->AllocateIndex();
                if (index == ResourceBindingAllocatorBase::invalid_binding)
                {
                    DebugLog(LogType::Warn, "ResourceBinder<%s>: Maximum size of %u reached, cannot bind more objects!\n",
                        TypeNameWithoutNamespace<T>().Data(),
                        allocator->max_size);

                    continue; // no more space to bind
                }

                auto insert_result = bindings.Insert(WeakHandle<T> { id }, index);
                AssertDebugMsg(insert_result.second, "Failed to insert binding for object with Id %u - it should not already exist!", id.Value());

                if (OnBindingChanged != nullptr)
                {
                    OnBindingChanged(insert_result.first->first.GetUnsafe(), ResourceBindingAllocatorBase::invalid_binding, index);
                }
            }

            if (newly_added.Count() != 0 || removed.Count() != 0)
            {
                DebugLog(LogType::Debug, "ResourceBinder<%s>: %u objects added, %u objects removed, %u total bindings\n",
                    TypeNameWithoutNamespace<T>().Data(),
                    newly_added.Count(),
                    removed.Count(),
                    after.Count());
            }

            last_frame_ids = std::move(current_frame_ids);
            current_frame_ids.Clear();
        }

        HYP_FORCE_INLINE Bitset GetNewlyAdded() const
        {
            const SizeType count = MathUtil::Max(last_frame_ids.NumBits(), current_frame_ids.NumBits());

            return Bitset(current_frame_ids).Resize(count) & ~Bitset(last_frame_ids).Resize(count);
        }

        HYP_FORCE_INLINE Bitset GetRemoved() const
        {
            const SizeType count = MathUtil::Max(last_frame_ids.NumBits(), current_frame_ids.NumBits());

            return Bitset(last_frame_ids).Resize(count) & ~Bitset(current_frame_ids).Resize(count);
        }

        TypeId type_id;
        // these bitsets are used to track which objects were bound in the last frame with bitwise operations
        Bitset last_frame_ids;
        Bitset current_frame_ids;
        HashMap<WeakHandle<T>, uint32> bindings;
    };

public:
    ResourceBinder(ResourceBindingAllocatorBase* binding_allocator)
        : ResourceBinderBase(binding_allocator),
          m_impl(TypeId::ForType<T>())
    {
        AssertDebug(m_binding_allocator != nullptr);

        const SizeType num_descendants = GetNumDescendants(TypeId::ForType<T>());

        // Create storage for subclass implementations
        // subclasses use a bitset (indexing by the subclass' StaticIndex) to determine which implementations are initialized
        m_subclass_impls.Resize(num_descendants);
        m_subclass_impls_initialized.Resize(num_descendants);
    }

    ResourceBinder(const ResourceBinder&) = delete;
    ResourceBinder& operator=(const ResourceBinder&) = delete;
    ResourceBinder(ResourceBinder&&) noexcept = delete;
    ResourceBinder& operator=(ResourceBinder&&) noexcept = delete;

    virtual ~ResourceBinder() override
    {
        m_impl.ReleaseBindings(m_binding_allocator);

        // Loop over the set bits and destruct subclass impls
        for (Bitset::BitIndex i : m_subclass_impls_initialized)
        {
            AssertDebug(i < m_subclass_impls.Size());

            Impl& impl = m_subclass_impls[i].Get();
            impl.ReleaseBindings(m_binding_allocator);

            m_subclass_impls[i].Destruct();
        }
    }

    virtual void Consider(HypObjectBase* object) override
    {
        if (!object)
        {
            return;
        }

        constexpr TypeId base_type_id = TypeId::ForType<T>();
        const TypeId object_type_id = object->GetTypeId();

        if (object_type_id == base_type_id)
        {
            m_impl.Consider(m_binding_allocator, object);

            return;
        }

        const int subclass_index = GetSubclassIndex(base_type_id, object_type_id);
        AssertDebugMsg(subclass_index >= 0 && subclass_index < int(m_subclass_impls.Size()),
            "ResourceBinder<%s>: Attempted to bind object with TypeId %u which is not a subclass of the expected TypeId (%u) or has no static index",
            TypeNameWithoutNamespace<T>().Data(), object_type_id.Value(), base_type_id.Value());

        if (!m_subclass_impls_initialized.Test(subclass_index))
        {
            m_subclass_impls[subclass_index].Construct(object_type_id);
            m_subclass_impls_initialized.Set(subclass_index, true);
        }

        Impl& impl = m_subclass_impls[subclass_index].Get();

        impl.Consider(m_binding_allocator, object);
    }

    virtual void Deconsider(HypObjectBase* object) override
    {
        AssertDebug(object != nullptr);

        constexpr TypeId type_id = TypeId::ForType<T>();

        if (object->GetTypeId() == type_id)
        {
            m_impl.Deconsider(m_binding_allocator, object);
        }
        else
        {
            const int subclass_index = GetSubclassIndex(type_id, object->GetTypeId());
            AssertDebugMsg(subclass_index >= 0 && subclass_index < int(m_subclass_impls.Size()),
                "ResourceBinder<%s>: Attempted to Deconsider object with TypeId %u which is not a subclass of the expected TypeId (%u) or has no static index",
                TypeNameWithoutNamespace<T>().Data(), object->GetTypeId().Value(), type_id.Value());

            if (!m_subclass_impls_initialized.Test(subclass_index))
            {
                // don't do anything if not set here since we're just Deconsidering
                return;
            }

            Impl& impl = m_subclass_impls[subclass_index].Get();
            impl.Deconsider(m_binding_allocator, object);
        }
    }

    virtual void ApplyUpdates() override
    {
        m_impl.ApplyUpdates(m_binding_allocator);

        for (Bitset::BitIndex i : m_subclass_impls_initialized)
        {
            Impl& impl = m_subclass_impls[i].Get();
            impl.ApplyUpdates(m_binding_allocator);
        }
    }

    virtual const Bitset& GetBoundIndices(TypeId type_id) const override
    {
        static const Bitset empty_bitset;

        constexpr TypeId base_type_id = TypeId::ForType<T>();

        if (type_id == TypeId::Void())
        {
            return empty_bitset;
        }

        if (type_id == base_type_id)
        {
            return m_impl.current_frame_ids;
        }
        else
        {
            const int subclass_index = GetSubclassIndex(base_type_id, type_id);
            AssertDebugMsg(subclass_index >= 0 && subclass_index < int(m_subclass_impls.Size()),
                "ResourceBinder<%s>: Attempted to get bound indices for TypeId %u which is not a subclass of the expected TypeId (%u) or has no static index",
                TypeNameWithoutNamespace<T>().Data(), type_id.Value(), base_type_id.Value());

            if (!m_subclass_impls_initialized.Test(subclass_index))
            {
                // not initialized, return empty bitset
                return empty_bitset;
            }

            return m_subclass_impls[subclass_index].Get().current_frame_ids;
        }
    }

protected:
    // base class impl
    Impl m_impl;

    // per-subtype implementations (only constructed and setup on first Consider() call with that type)
    Array<ValueStorage<Impl>> m_subclass_impls;
    Bitset m_subclass_impls_initialized;
};

} // namespace hyperion

#endif