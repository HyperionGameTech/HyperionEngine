/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENVIRONMENT_HPP
#define HYPERION_ENVIRONMENT_HPP

#include <rendering/ParticleSystem.hpp>
#include <rendering/GaussianSplatting.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/rt/DDGI.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/Base.hpp>
#include <core/Name.hpp>
#include <core/utilities/Pair.hpp>

#include <math/MathUtil.hpp>
#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {

class Engine;
class Scene;

using renderer::RTUpdateStateFlags;

using RenderEnvironmentUpdates = uint8;

enum RenderEnvironmentUpdateBits : RenderEnvironmentUpdates
{
    RENDER_ENVIRONMENT_UPDATES_NONE                 = 0x0,
    RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS    = 0x1,
    RENDER_ENVIRONMENT_UPDATES_PLACEHOLDER          = 0x2,

    RENDER_ENVIRONMENT_UPDATES_CONTAINERS           = RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS,

    RENDER_ENVIRONMENT_UPDATES_TLAS                 = 0x4
};

class HYP_API RenderEnvironment : public BasicObject<RenderEnvironment>
{
    using RenderComponentPendingRemovalEntry = Pair<TypeID, Name>;

public:
    RenderEnvironment();
    RenderEnvironment(Scene *scene);
    RenderEnvironment(const RenderEnvironment &other)               = delete;
    RenderEnvironment &operator=(const RenderEnvironment &other)    = delete;
    ~RenderEnvironment();

    void SetTLAS(const TLASRef &tlas);

    Scene *GetScene() const
        { return m_scene; }

    const Handle<ParticleSystem> &GetParticleSystem() const
        { return m_particle_system; }

    const Handle<GaussianSplatting> &GetGaussianSplatting() const
        { return m_gaussian_splatting; }

    template <class T>
    RC<T> AddRenderComponent(const RC<T> &component)
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        AssertThrow(component != nullptr);

        AddRenderComponent(TypeID::ForType<T>(), component);

        return component;
    }

    template <class T, class... Args>
    RC<T> AddRenderComponent(Name name, Args &&... args)
    {
        return AddRenderComponent(MakeRefCountedPtr<T>(name, std::forward<Args>(args)...));
    }

    template <class T>
    T *GetRenderComponent(Name name = Name::Invalid())
    {
        Mutex::Guard guard(m_render_components_mutex);

        if (!m_render_components.Contains<T>()) {
            return nullptr;
        }

        auto &items = m_render_components.At<T>();

        if (name) {
            const auto it = items.Find(name);

            if (it == items.End()) {
                return nullptr;
            }

            return static_cast<T *>(it->second.Get());
        } else {
            return items.Any() ? static_cast<T *>(items.Front().second.Get()) : nullptr;
        }
    }

    template <class T>
    bool HasRenderComponent() const
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        Mutex::Guard guard(m_render_components_mutex);

        if (!m_render_components.Contains<T>()) {
            return false;
        }

        const FlatMap<Name, RC<RenderComponentBase>> &items = m_render_components.At<T>();

        return items.Any();
    }

    template <class T>
    bool HasRenderComponent(Name name) const
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        Mutex::Guard guard(m_render_components_mutex);

        if (!m_render_components.Contains<T>()) {
            return false;
        }

        const FlatMap<Name, RC<RenderComponentBase>> &items = m_render_components.At<T>();

        return items.Contains(name);
    }

    /*! \brief Remove a RenderComponent of the given type T and the given name value.
     *  If the name value is Name::Invalid(), then all items of the type T are removed.
     */
    template <class T>
    void RemoveRenderComponent(Name name = Name::Invalid())
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        RemoveRenderComponent(TypeID::ForType<T>(), name);

        DebugLog(LogType::Debug, "Remove render component of type %s with name %s\n", TypeName<T>().Data(), name.LookupString());
    }

    // only touch from render thread!
    uint32 GetEnabledRenderComponentsMask() const
        { return m_current_enabled_render_components_mask; }

    uint32 GetFrameCounter() const
        { return m_frame_counter; }

    void Init();
    void Update(GameCounter::TickUnit delta);

    void RenderRTRadiance(Frame *frame);
    void RenderDDGIProbes(Frame *frame);

    void RenderComponents(Frame *frame);

private:
    void AddRenderComponent(TypeID type_id, const RC<RenderComponentBase> &render_component);
    void RemoveRenderComponent(TypeID type_id, Name name);

    void ApplyTLASUpdates(Frame *frame, RTUpdateStateFlags flags);

    Scene                                           *m_scene;

    AtomicVar<RenderEnvironmentUpdates>             m_update_marker { RENDER_ENVIRONMENT_UPDATES_NONE };

    mutable Mutex                                   m_render_components_mutex;
    TypeMap<FlatMap<Name, RC<RenderComponentBase>>> m_render_components; // only touch from render thread!
    Array<RC<RenderComponentBase>>                  m_enabled_render_components;
    uint32                                          m_current_enabled_render_components_mask;
    uint32                                          m_next_enabled_render_components_mask;

    Handle<ParticleSystem>                          m_particle_system;

    Handle<GaussianSplatting>                       m_gaussian_splatting;

    UniquePtr<RTRadianceRenderer>                   m_rt_radiance;
    DDGI                                            m_ddgi;
    bool                                            m_has_rt_radiance;
    bool                                            m_has_ddgi_probes;
    TLASRef                                         m_tlas;
    
    uint32                                          m_frame_counter;
};

} // namespace hyperion

#endif