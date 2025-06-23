/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_GLOBAL_STATE_HPP
#define HYPERION_RENDER_GLOBAL_STATE_HPP

#include <core/memory/UniquePtr.hpp>

#include <core/Handle.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion {

class Engine;
class Entity;
class ShadowMapAllocator;
class GPUBufferHolderMap;
class PlaceholderData;
class RenderProxyList;
class RenderView;
class View;
class DrawCallCollection;
class IRenderer;
class EnvProbeRenderer;
class EnvProbe;
class ReflectionProbe;
class SkyProbe;
class RenderGlobalState;

HYP_API extern uint32 GetRenderThreadFrameIndex();
HYP_API extern uint32 GetGameThreadFrameIndex();

HYP_API extern void BeginFrame_GameThread();
HYP_API extern void EndFrame_GameThread();

HYP_API extern void BeginFrame_RenderThread();
HYP_API extern void EndFrame_RenderThread();

/*! \brief Get the RenderProxyList for the Game thread to write to for the current frame, for the given view.
 *  The game thread adds proxies of entities, lights, envprobes, etc. to this list, which the render thread will
 *  use when rendering the frame.
 *  \note This is only valid to call from the game thread, or from a task that is initiated by the game thread. */
HYP_API extern RenderProxyList& GetProducerRenderProxyList(View* view);

/*! \brief Get the RenderProxyList for the Render thread to read from for the current frame, for the given view.
 *  \note This is only valid to call from the render thread, or from a task that is initiated by the render thread. */
HYP_API extern RenderProxyList& GetConsumerRenderProxyList(View* view);

struct ObjectBindingAllocatorBase
{
    static constexpr uint32 invalid_binding = ~0u;

    ObjectBindingAllocatorBase(uint32 max_size)
        : max_size(max_size)
    {
        used_indices.Resize(max_size);
    }

    uint32 AllocateIndex()
    {
        for (uint32 i = 0; i < max_size; ++i)
        {
            if (!used_indices.Get(i))
            {
                used_indices.Set(i, true);

                return i;
            }
        }

        return invalid_binding; // No free index found
    }

    void FreeIndex(uint32 index)
    {
        if (index >= max_size || index == invalid_binding)
        {
            return;
        }

        used_indices.Set(index, false);
    }

    void Reset()
    {
        current_frame_count = 0;
    }

    // the maximum size of this allocator, i.e. the maximum number of bindings that can be allocated in a single frame
    const uint32 max_size;

    // number of claims for the current from
    // NOTE: not same as bindings (bindings are set after UpdateBoundResources() is called so we can calculate reuse)
    uint32 current_frame_count = 0;

    // Bits representing whether an index is allocated or not.
    // we find free indices by iterating over this bitset to find the first unset bit.
    Bitset used_indices;
};

template <uint32 MaxSize>
struct ObjectBindingAllocator : ObjectBindingAllocatorBase
{
    ObjectBindingAllocator()
        : ObjectBindingAllocatorBase(MaxSize)
    {
    }
};

class HYP_API ObjectBinderBase
{
public:
    virtual ~ObjectBinderBase() = default;

    virtual void UpdateBoundResources() = 0;

protected:
    ObjectBinderBase(RenderGlobalState* rgs);

    static int GetSubclassIndex(TypeID base_type_id, TypeID subclass_type_id);

    template <class T, class U>
    static int GetSubclassIndex()
    {
        static const int idx = GetSubclassIndex(TypeID::ForType<T>(), TypeID::ForType<U>());

        return idx;
    }

    static SizeType GetNumDescendants(TypeID type_id);
};

template <class T, auto OnBindingChanged>
class ObjectBinder final : public ObjectBinderBase
{
    struct Impl final
    {
        template <class Functions>
        Impl(TypeWrapper<Functions>)
        {
            Bind = &Functions::Bind;
            Unbind = &Functions::Unbind;
            UpdateBoundResources = &Functions::UpdateBoundResources;
            ReleaseBindings = &Functions::ReleaseBindings;
        }

        bool (*Bind)(Impl* impl, ObjectBindingAllocatorBase* allocator, T* object);
        void (*Unbind)(Impl* impl, ObjectBindingAllocatorBase* allocator, T* object);
        void (*UpdateBoundResources)(Impl* impl, ObjectBindingAllocatorBase* allocator);
        void (*ReleaseBindings)(Impl* impl, ObjectBindingAllocatorBase* allocator);

        HYP_FORCE_INLINE Bitset GetNewlyAdded() const
        {
            const SizeType new_num_bits = MathUtil::Max(m_last_frame_ids.NumBits(), m_current_frame_ids.NumBits());

            return Bitset(m_current_frame_ids).Resize(new_num_bits) & ~Bitset(m_last_frame_ids).Resize(new_num_bits);
        }

        HYP_FORCE_INLINE Bitset GetRemoved() const
        {
            const SizeType new_num_bits = MathUtil::Max(m_last_frame_ids.NumBits(), m_current_frame_ids.NumBits());

            return Bitset(m_last_frame_ids).Resize(new_num_bits) & ~Bitset(m_current_frame_ids).Resize(new_num_bits);
        }

        HashMap<WeakHandle<T>, uint32> m_bindings;
        // these bitsets are used to track which objects were bound in the last frame with bitwise operations
        Bitset m_last_frame_ids;
        Bitset m_current_frame_ids;
    };

    template <class Subclass>
    struct ImplFunctions final
    {
        static void ReleaseBindings(Impl* impl, ObjectBindingAllocatorBase* allocator)
        {
            // Unbind all objects that were bound in the last frame
            for (Bitset::BitIndex bit_index : impl->m_last_frame_ids)
            {
                const ID<Subclass> id = ID<Subclass>::FromIndex(bit_index);
                const auto it = impl->m_bindings.FindAs(id);
                AssertDebug(it != impl->m_bindings.End());

                if (it != impl->m_bindings.End())
                {
                    Subclass* object = it->first.GetUnsafe();
                    const uint32 binding = it->second;

                    OnBindingChanged(object, binding, ObjectBindingAllocatorBase::invalid_binding);
                    allocator->FreeIndex(binding);
                    impl->m_bindings.Erase(it);
                }
            }
        }

        static bool Bind(Impl* impl, ObjectBindingAllocatorBase* allocator, T* object)
        {
            ID<Subclass> id = static_cast<Subclass*>(object)->GetID();

            if (!id.IsValid())
            {
                return false;
            }

            if (impl->m_allocator->current_frame_count >= impl->m_allocator->max_size)
            {
                DebugLog(LogType::Warn, "ObjectBinder<%s>::Impl<%s>: Maximum size of %u reached, cannot bind more objects!\n", TypeNameWithoutNamespace<T>().Data(), TypeNameWithoutNamespace<Subclass>().Data(), allocator->max_size);
                return false; // maximum size reached
            }

            ++allocator->current_frame_count;

            impl->m_current_frame_ids.Set(id.ToIndex(), true);

            return true;
        }

        static void Unbind(Impl* impl, ObjectBindingAllocatorBase* allocator, T* object)
        {
            ID<Subclass> id = static_cast<Subclass*>(object)->GetID();

            if (!id.IsValid())
            {
                return;
            }

            impl->m_current_frame_ids.Set(id.ToIndex(), false);
        }

        static void UpdateBoundResources(Impl* impl, ObjectBindingAllocatorBase* allocator)
        {
            const Bitset removed = impl->GetRemoved();
            const Bitset newly_added = impl->GetNewlyAdded();
            const Bitset after = (impl->m_last_frame_ids & ~removed) | newly_added;

            AssertDebug(after.Count() <= allocator->max_size);

            for (Bitset::BitIndex bit_index : removed)
            {
                const ID<Subclass> id = ID<Subclass>::FromIndex(bit_index);
                const auto it = impl->m_bindings.FindAs(id);
                AssertDebug(it != impl->m_bindings.End());

                if (it != impl->m_bindings.End())
                {
                    Subclass* object = it->first.Get();
                    const uint32 binding = it->second;

                    OnBindingChanged(object, binding, ObjectBindingAllocatorBase::invalid_binding);
                    allocator->FreeIndex(binding);
                    impl->m_bindings.Erase(it);
                }
            }

            for (Bitset::BitIndex bit_index : newly_added)
            {
                const ID<Subclass> id = ID<Subclass>::FromIndex(bit_index);
                const auto it = impl->m_bindings.FindAs(id);
                if (it != impl->m_bindings.End())
                {
                    // already bound
                    continue;
                }

                const uint32 index = allocator->AllocateIndex();
                if (index == ObjectBindingAllocatorBase::invalid_binding)
                {
                    DebugLog(LogType::Warn, "ObjectBinder<%s>::Impl<%s>: Maximum size of %u reached, cannot bind more objects!\n",
                        TypeNameWithoutNamespace<T>().Data(),
                        TypeNameWithoutNamespace<T>().Data(),
                        allocator->max_size);

                    continue; // no more space to bind
                }

                auto insert_result = impl->m_bindings.Insert(WeakHandle<Subclass> { id }, index);
                AssertDebugMsg(insert_result.second, "Failed to insert binding for object with ID %u - it should not already exist!", id.Value());

                OnBindingChanged(insert_result.first->first.GetUnsafe(), ObjectBindingAllocatorBase::invalid_binding, index);
            }

            if (newly_added.Count() != 0 || removed.Count() != 0)
            {
                DebugLog(LogType::Debug, "ObjectBinder<%s>::Impl<%s>: %u objects added, %u objects removed, %u total bindings\n",
                    TypeNameWithoutNamespace<T>().Data(),
                    TypeNameWithoutNamespace<Subclass>().Data(),
                    newly_added.Count(),
                    removed.Count(),
                    after.Count());
            }

            impl->m_last_frame_ids = impl->m_current_frame_ids;
            impl->m_current_frame_ids.Clear();
        }
    };

public:
    ObjectBinder(RenderGlobalState* rgs, ObjectBindingAllocatorBase* allocator)
        : ObjectBinderBase(rgs),
          m_allocator(allocator),
          m_impl(ImplFunctions<T>())
    {
        AssertDebug(m_allocator != nullptr);

        const SizeType num_descendants = GetNumDescendants(TypeID::ForType<T>());

        // Create storage for subclass implementations
        // subclasses use a bitset (indexing by the subclass' StaticIndex) to determine which implementations are initialized
        m_subclass_impls.Resize(num_descendants);
        m_subclass_impls_initialized.Resize(num_descendants);
    }

    ObjectBinder(const ObjectBinder&) = delete;
    ObjectBinder& operator=(const ObjectBinder&) = delete;
    ObjectBinder(ObjectBinder&&) = delete;
    ObjectBinder& operator=(ObjectBinder&&) = delete;

    ~ObjectBinder() override
    {
        // Loop over the set bits and destruct subclass impls
        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            AssertDebug(bit_index < m_subclass_impls.Size());

            Impl& impl = m_subclass_impls[bit_index].Get();
            impl.ReleaseBindings(&impl, m_allocator);
            m_subclass_impls[bit_index].Destruct();
        }
    }

    bool Bind(T* object)
    {
        if (!object)
        {
            return false;
        }

        constexpr TypeID type_id = TypeID::ForType<T>();
        if (object->GetTypeID() == type_id)
        {
            return m_impl.Bind(&m_impl, m_allocator, object);
        }
        else
        {
            const int subclass_index = GetSubclassIndex(type_id, object->GetTypeID());
            AssertDebugMsg(subclass_index >= 0 && subclass_index < int(m_subclass_impls.Size()),
                "ObjectBinder<%s>: Attempted to bind object with TypeID %u which is not a subclass of the expected TypeID (%u) or has no static index",
                TypeNameWithoutNamespace<T>().Data(), object->GetTypeID().Value(), type_id.Value());

            if (!m_subclass_impls_initialized.Test(subclass_index))
            {
                m_subclass_impls[subclass_index].Construct(this);
                m_subclass_impls_initialized.Set(subclass_index, true);
            }

            Impl& impl = m_subclass_impls[subclass_index].Get();

            return impl.Bind(&impl, m_allocator, object);
        }
    }

    void Unbind(T* object)
    {
        AssertDebug(object != nullptr);

        constexpr TypeID type_id = TypeID::ForType<T>();
        if (object->GetTypeID() == type_id)
        {
            m_impl.Unbind(&m_impl, m_allocator, object);
        }
        else
        {
            const int subclass_index = GetSubclassIndex(type_id, object->GetTypeID());
            AssertDebugMsg(subclass_index >= 0 && subclass_index < int(m_subclass_impls.Size()),
                "ObjectBinder<%s>: Attempted to unbind object with TypeID %u which is not a subclass of the expected TypeID (%u) or has no static index",
                TypeNameWithoutNamespace<T>().Data(), object->GetTypeID().Value(), type_id.Value());

            if (!m_subclass_impls_initialized.Test(subclass_index))
            {
                // don't do anything if not set here since we're just unbinding
                return;
            }

            Impl& impl = m_subclass_impls[type_id].Get();
            impl.Unbind(&impl, m_allocator, object);
        }
    }

    virtual void UpdateBoundResources() override
    {
        m_impl.UpdateBoundResources();

        for (Bitset::BitIndex bit_index : m_subclass_impls_initialized)
        {
            Impl& impl = m_subclass_impls[bit_index].Get();
            impl.UpdateBoundResources(&m_impl, m_allocator);
        }
    }

protected:
    ObjectBindingAllocatorBase* m_allocator;

    // base class impl
    Impl m_impl;
    // per-subtype implementations (only constructed and setup on first Bind() call with that type)
    Array<ValueStorage<Impl>> m_subclass_impls;
    Bitset m_subclass_impls_initialized;
};

class RenderGlobalState
{
    friend class ObjectBinderBase;

    static void OnEnvProbeBindingChanged(EnvProbe* env_probe, uint32 prev, uint32 next);

public:
    static constexpr uint32 max_binders = 16;

    RenderGlobalState();
    RenderGlobalState(const RenderGlobalState& other) = delete;
    RenderGlobalState& operator=(const RenderGlobalState& other) = delete;
    ~RenderGlobalState();

    void Create();
    void Destroy();
    void UpdateBuffers(FrameBase* frame);

    GPUBufferHolderBase* Worlds;
    GPUBufferHolderBase* Cameras;
    GPUBufferHolderBase* Lights;
    GPUBufferHolderBase* Entities;
    GPUBufferHolderBase* Materials;
    GPUBufferHolderBase* Skeletons;
    GPUBufferHolderBase* ShadowMaps;
    GPUBufferHolderBase* EnvProbes;
    GPUBufferHolderBase* EnvGrids;
    GPUBufferHolderBase* LightmapVolumes;

    BindlessStorage BindlessTextures;

    UniquePtr<class ShadowMapAllocator> ShadowMapAllocator;
    UniquePtr<class GPUBufferHolderMap> GPUBufferHolderMap;
    UniquePtr<PlaceholderData> PlaceholderData;

    DescriptorTableRef GlobalDescriptorTable;

    IRenderer* Renderer;
    EnvProbeRenderer** EnvProbeRenderers;

    ObjectBinderBase* ObjectBinders[max_binders] { nullptr };
    ObjectBinder<EnvProbe, &OnEnvProbeBindingChanged> EnvProbeBinder;

private:
    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();

    void SetDefaultDescriptorSetElements(uint32 frame_index);
};

} // namespace hyperion

#endif