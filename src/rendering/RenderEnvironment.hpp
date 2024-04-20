/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENVIRONMENT_HPP
#define HYPERION_ENVIRONMENT_HPP

#include <rendering/ParticleSystem.hpp>
#include <rendering/GaussianSplatting.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/rt/DDGI.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <core/Base.hpp>
#include <core/Name.hpp>
#include <core/Containers.hpp>
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

class HYP_API RenderEnvironment
    : public BasicObject<STUB_CLASS(RenderEnvironment)>
{
    using RenderComponentPendingRemovalEntry = Pair<TypeID, Name>;

public:
    RenderEnvironment(Scene *scene);
    RenderEnvironment(const RenderEnvironment &other) = delete;
    RenderEnvironment &operator=(const RenderEnvironment &other) = delete;
    ~RenderEnvironment();

    void SetTLAS(Handle<TLAS> tlas);

    Scene *GetScene() const
        { return m_scene; }

    const Handle<ParticleSystem> &GetParticleSystem() const
        { return m_particle_system; }

    const Handle<GaussianSplatting> &GetGaussianSplatting() const
        { return m_gaussian_splatting; }

    template <class T>
    RC<T> AddRenderComponent(RC<T> component)
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        AssertThrow(component != nullptr);

        const Name name = component->GetName();

        component->SetParent(this);
        component->SetComponentIndex(0);

        if (IsInitCalled()) {
            component->ComponentInit();
        }

        std::lock_guard guard(m_render_component_mutex);

        const auto it = m_render_components_pending_addition.Find<T>();

        if (it != m_render_components_pending_addition.End()) {
            const auto name_it = it->second.Find(name);

            AssertThrowMsg(
                name_it == it->second.End(),
                "Render component with name %s already exists! Name must be unique.\n",
                name.LookupString()
            );

            it->second.Insert(name, component);
        } else {
            FlatMap<Name, RC<RenderComponentBase>> component_map;
            component_map.Set(name, component);

            m_render_components_pending_addition.Set<T>(std::move(component_map));
        }
        
        m_update_marker.BitOr(RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS, MemoryOrder::RELEASE);

        return component;
    }

    template <class T, class ...Args>
    RC<T> AddRenderComponent(Name name, Args &&... args)
    {
        return AddRenderComponent(RC<T>::Construct(name, std::forward<Args>(args)...));
    }

    /*! \note CALL FROM RENDER THREAD ONLY */
    template <class T>
    T *GetRenderComponent(Name name = Name::Invalid())
    {
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

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

    /*! \note CALL FROM RENDER THREAD ONLY */
    template <class T>
    bool HasRenderComponent() const
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        if (!m_render_components.Contains<T>()) {
            return false;
        }

        const FlatMap<Name, RC<RenderComponentBase>> &items = m_render_components.At<T>();

        return items.Any();
    }

    /*! \note CALL FROM RENDER THREAD ONLY */
    template <class T>
    bool HasRenderComponent(Name name) const
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

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

        std::lock_guard guard(m_render_component_mutex);

        m_render_components_pending_removal.Insert(RenderComponentPendingRemovalEntry {
            TypeID::ForType<T>(),
            name
        });

        m_update_marker.BitOr(RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS, MemoryOrder::RELEASE);
    }

    // only touch from render thread!
    uint32 GetEnabledRenderComponentsMask() const
        { return m_current_enabled_render_components_mask; }

    float GetGlobalTimer() const
        { return m_global_timer; }

    uint32 GetFrameCounter() const
        { return m_frame_counter; }

    void Init();
    void Update(GameCounter::TickUnit delta);

    void RenderRTRadiance(Frame *frame);
    void RenderDDGIProbes(Frame *frame);

    void RenderComponents(Frame *frame);

private:
    void ApplyTLASUpdates(Frame *frame, RTUpdateStateFlags flags);

    Scene                                           *m_scene;

    AtomicVar<RenderEnvironmentUpdates>             m_update_marker { RENDER_ENVIRONMENT_UPDATES_NONE };

    TypeMap<FlatMap<Name, RC<RenderComponentBase>>> m_render_components; // only touch from render thread
    TypeMap<FlatMap<Name, RC<RenderComponentBase>>> m_render_components_pending_init;
    TypeMap<FlatMap<Name, RC<RenderComponentBase>>> m_render_components_pending_addition;
    FlatSet<RenderComponentPendingRemovalEntry>     m_render_components_pending_removal;
    std::mutex                                      m_render_component_mutex;
    uint32                                          m_current_enabled_render_components_mask;
    uint32                                          m_next_enabled_render_components_mask;

    Handle<ParticleSystem>                          m_particle_system;

    Handle<GaussianSplatting>                       m_gaussian_splatting;

    UniquePtr<RTRadianceRenderer>                   m_rt_radiance;
    DDGI                                            m_ddgi;
    bool                                            m_has_rt_radiance;
    bool                                            m_has_ddgi_probes;
    Handle<TLAS>                                    m_tlas;

    float                                           m_global_timer;
    uint32                                          m_frame_counter;
};

} // namespace hyperion

#endif