/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENVIRONMENT_HPP
#define HYPERION_ENVIRONMENT_HPP

#include <rendering/ParticleSystem.hpp>
#include <rendering/GaussianSplatting.hpp>
#include <rendering/RenderSubsystem.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/rt/DDGI.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/Base.hpp>
#include <core/Name.hpp>

#include <core/utilities/Pair.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/math/MathUtil.hpp>
#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {

class Engine;
class Scene;

using renderer::RTUpdateStateFlags;

using RenderEnvironmentUpdates = uint64;

enum RenderEnvironmentUpdateBits : RenderEnvironmentUpdates
{
    RENDER_ENVIRONMENT_UPDATES_NONE                 = 0x0,
    RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS    = 0x1,
    RENDER_ENVIRONMENT_UPDATES_PLACEHOLDER          = 0x2,

    RENDER_ENVIRONMENT_UPDATES_CONTAINERS           = RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS,

    RENDER_ENVIRONMENT_UPDATES_TLAS                 = 0x4,

    RENDER_ENVIRONMENT_UPDATES_THREAD_MASK          = 0x10 // use mask shifted by ThreadType value to issue unique updates for a specific thread
};

HYP_CLASS()
class HYP_API RenderEnvironment : public HypObject<RenderEnvironment>
{
    HYP_OBJECT_BODY(RenderEnvironment);

public:
    RenderEnvironment();
    RenderEnvironment(const RenderEnvironment &other)               = delete;
    RenderEnvironment &operator=(const RenderEnvironment &other)    = delete;
    ~RenderEnvironment();

    const TLASRef &GetTLAS() const
        { return m_tlas; }

    const Handle<ParticleSystem> &GetParticleSystem() const
        { return m_particle_system; }

    const Handle<GaussianSplatting> &GetGaussianSplatting() const
        { return m_gaussian_splatting; }

    template <class T>
    RC<T> AddRenderSubsystem(const RC<T> &component)
    {
        static_assert(std::is_base_of_v<RenderSubsystem, T>,
            "Component should be a derived class of RenderSubsystem");

        AssertThrow(component != nullptr);

        AddRenderSubsystem(GetRenderSubsystemTypeID<T>(), component);

        return component;
    }

    template <class T, class... Args>
    RC<T> AddRenderSubsystem(Name name, Args &&... args)
    {
        return AddRenderSubsystem(MakeRefCountedPtr<T>(name, std::forward<Args>(args)...));
    }

    template <class T>
    T *GetRenderSubsystem(Name name = Name::Invalid())
    {
        Mutex::Guard guard(m_render_subsystems_mutex);

        const TypeID type_id = GetRenderSubsystemTypeID<T>();

        if (!m_render_subsystems.Contains(type_id)) {
            return nullptr;
        }

        auto &items = m_render_subsystems.At(type_id);

        if (items.Empty()) {
            return nullptr;
        }

        RenderSubsystem *render_subsystem = nullptr;

        if (name) {
            const auto it = items.Find(name);

            if (it == items.End()) {
                return nullptr;
            }

            render_subsystem = it->second.Get();
        } else {
            render_subsystem = items.Front().second.Get();
        }

        if (type_id != TypeID::ForType<T>()) {
            if (!render_subsystem->IsInstanceOf(T::Class())) {
                return nullptr;
            }
        }

        return static_cast<T *>(render_subsystem);
    }

    template <class T>
    bool HasRenderSubsystem() const
    {
        const TypeID type_id = GetRenderSubsystemTypeID<T>();

        static_assert(std::is_base_of_v<RenderSubsystem, T>,
            "Component should be a derived class of RenderSubsystem");

        Mutex::Guard guard(m_render_subsystems_mutex);

        if (!m_render_subsystems.Contains(type_id)) {
            return false;
        }

        const FlatMap<Name, RC<RenderSubsystem>> &items = m_render_subsystems.At(type_id);

        if (type_id == TypeID::ForType<T>()) {
            return items.Any();
        }

        for (const auto &it : items) {
            if (it.second->IsInstanceOf(T::Class())) {
                return true;
            }
        }

        return false;
    }

    template <class T>
    bool HasRenderSubsystem(Name name) const
    {
        static_assert(std::is_base_of_v<RenderSubsystem, T>,
            "Component should be a derived class of RenderSubsystem");

        const TypeID type_id = GetRenderSubsystemTypeID<T>();

        Mutex::Guard guard(m_render_subsystems_mutex);

        if (!m_render_subsystems.Contains(type_id)) {
            return false;
        }

        const FlatMap<Name, RC<RenderSubsystem>> &items = m_render_subsystems.At(type_id);

        if (type_id == TypeID::ForType<T>()) {
            return items.Contains(name);
        }

        for (const auto &it : items) {
            if (it.first == name && it.second->IsInstanceOf(T::Class())) {
                return true;
            }
        }

        return false;
    }

    /*! \brief Remove a RenderSubsystem of the given type T and the given name value.
     *  If the name value is Name::Invalid(), then all items of the type T are removed.
     */
    template <class T>
    void RemoveRenderSubsystem(Name name = Name::Invalid())
    {
        static_assert(std::is_base_of_v<RenderSubsystem, T>,
            "Component should be a derived class of RenderSubsystem");

        RemoveRenderSubsystem(GetRenderSubsystemTypeID<T>(), T::Class(), name);
    }

    void RemoveRenderSubsystem(const RenderSubsystem *render_subsystem);

    // only touch from render thread!
    uint32 GetEnabledRenderSubsystemsMask() const
        { return m_current_enabled_render_subsystems_mask; }

    uint32 GetFrameCounter() const
        { return m_frame_counter; }

    void Init();
    void Update(GameCounter::TickUnit delta);

    void RenderRTRadiance(Frame *frame);
    void RenderDDGIProbes(Frame *frame);

    void RenderSubsystems(Frame *frame);

private:
    HYP_FORCE_INLINE void AddUpdateMarker(RenderEnvironmentUpdates value, ThreadType thread_type)
    {
        m_update_marker.BitOr(value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(thread_type)), MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE void RemoveUpdateMarker(RenderEnvironmentUpdates value, ThreadType thread_type)
    {
        m_update_marker.BitAnd(~(value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(thread_type))), MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool GetUpdateMarker(RenderEnvironmentUpdates value, ThreadType thread_type) const
    {
        return m_update_marker.Get(MemoryOrder::ACQUIRE) & (value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(thread_type)));
    }

    /*! \brief Finds the TypeID to use to group an instance in for a given clas extending RenderSubsystem.
     *  The type will be the highest HypClass in the heirarchy that is not \ref{RenderSubsystem}.
     *  E.g for a class FooBarThingy -> ThingyBase -> RenderSubsystem, the type will be ThingyBase.
     */ 
    template <class T>
    static TypeID GetRenderSubsystemTypeID()
    {
        static_assert(std::is_base_of_v<RenderSubsystem, T> && !std::is_same_v<RenderSubsystem, T>, "Component should be a derived class of RenderSubsystem");
        
        return GetRenderSubsystemTypeID(T::Class());
    }

    static TypeID GetRenderSubsystemTypeID(const HypClass *hyp_class);

    void AddRenderSubsystem(TypeID type_id, const RC<RenderSubsystem> &render_subsystem);
    void RemoveRenderSubsystem(TypeID type_id, const HypClass *hyp_class, Name name);

    void ApplyTLASUpdates(Frame *frame, RTUpdateStateFlags flags);

    void InitializeRT();
    bool CreateTLAS();

    AtomicVar<RenderEnvironmentUpdates>                                 m_update_marker { RENDER_ENVIRONMENT_UPDATES_NONE };

    mutable Mutex                                                       m_render_subsystems_mutex;
    TypeMap<FlatMap<Name, RC<RenderSubsystem>>>                         m_render_subsystems;
    FixedArray<Array<RC<RenderSubsystem>>, ThreadType::THREAD_TYPE_MAX> m_enabled_render_subsystems;
    uint32                                                              m_current_enabled_render_subsystems_mask;
    uint32                                                              m_next_enabled_render_subsystems_mask;

    Handle<ParticleSystem>                                              m_particle_system;

    Handle<GaussianSplatting>                                           m_gaussian_splatting;

    UniquePtr<RTRadianceRenderer>                                       m_rt_radiance;
    DDGI                                                                m_ddgi;
    bool                                                                m_has_rt_radiance;
    bool                                                                m_has_ddgi_probes;
    bool                                                                m_rt_initialized;
    TLASRef                                                             m_tlas;
    
    uint32                                                              m_frame_counter;
};

} // namespace hyperion

#endif