#ifndef HYPERION_V2_ENVIRONMENT_H
#define HYPERION_V2_ENVIRONMENT_H

#include <rendering/Shadows.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/ParticleSystem.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/rt/ProbeSystem.hpp>

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/lib/AtomicLock.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/Pair.hpp>

#include <math/MathUtil.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <vector>

namespace hyperion::renderer {
class Frame;
} // namespace hyperion::renderer

namespace hyperion::v2 {

class Engine;
class Scene;

using renderer::RTUpdateStateFlags;

using RenderEnvironmentUpdates = UInt8;

enum RenderEnvironmentUpdateBits : RenderEnvironmentUpdates
{
    RENDER_ENVIRONMENT_UPDATES_NONE = 0x0,
    RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS = 0x1,
    RENDER_ENVIRONMENT_UPDATES_ENTITIES = 0x2,

    RENDER_ENVIRONMENT_UPDATES_CONTAINERS = RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS | RENDER_ENVIRONMENT_UPDATES_ENTITIES,

    RENDER_ENVIRONMENT_UPDATES_TLAS = 0x4
};

class RenderEnvironment
    : public EngineComponentBase<STUB_CLASS(RenderEnvironment)>
{
    using RenderComponentPendingRemovalEntry = Pair<TypeID, RenderComponentName>;

public:
    RenderEnvironment(Scene *scene);
    RenderEnvironment(const RenderEnvironment &other) = delete;
    RenderEnvironment &operator=(const RenderEnvironment &other) = delete;
    ~RenderEnvironment();

    void SetTLAS(Handle<TLAS> &&tlas);
    void SetTLAS(const Handle<TLAS> &tlas);

    Scene *GetScene() const
        { return m_scene; }

    Handle<ParticleSystem> &GetParticleSystem()
        { return m_particle_system; }

    const Handle<ParticleSystem> &GetParticleSystem() const
        { return m_particle_system; }

    template <class T>
    void AddRenderComponent(UniquePtr<T> &&component)
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        AssertThrow(component != nullptr);

        component->SetParent(this);
        component->SetComponentIndex(0);

        if (IsInitCalled()) {
            component->ComponentInit();
        }

        std::lock_guard guard(m_render_component_mutex);

        m_render_components_pending_addition.Set<T>(std::move(component));
        
        m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS);
    }

    template <class T, class ...Args>
    void AddRenderComponent(Args &&... args)
    {
        AddRenderComponent(UniquePtr<T>::Construct(std::forward<Args>(args)...));
    }

    /*! CALL FROM RENDER THREAD ONLY */
    template <class T>
    T *GetRenderComponent()
    {
        Threads::AssertOnThread(THREAD_RENDER);

        if (!m_render_components.Contains<T>()) {
            return nullptr;
        }

        return static_cast<T *>(m_render_components.At<T>().Get());
    }

    /*! CALL FROM RENDER THREAD ONLY */
    template <class T>
    bool HasRenderComponent() const
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        Threads::AssertOnThread(THREAD_RENDER);

        return m_render_components.Contains<T>();
    }

    template <class T>
    void RemoveRenderComponent()
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        std::lock_guard guard(m_render_component_mutex);

        m_render_components_pending_removal.Insert(RenderComponentPendingRemovalEntry {
            TypeID::ForType<T>(),
            T::ComponentName
        });

        m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS);
    }

    // only touch from render thread!
    UInt32 GetEnabledRenderComponentsMask() const
        { return m_current_enabled_render_components_mask; }

    void OnEntityAdded(Handle<Entity> &entity);
    void OnEntityRemoved(Handle<Entity> &entity);
    // only called when meaningful attributes have changed
    void OnEntityRenderableAttributesChanged(Handle<Entity> &entity);

    Float GetGlobalTimer() const
        { return m_global_timer; }

    UInt32 GetFrameCounter() const
        { return m_frame_counter; }

    void Init();
    void Update(GameCounter::TickUnit delta);

    void RenderRTRadiance(Frame *frame);

    void RenderComponents(Frame *frame);

private:
    void ApplyTLASUpdates(Frame *frame, RTUpdateStateFlags flags);

    Scene *m_scene;

    std::atomic<RenderEnvironmentUpdates> m_update_marker { RENDER_ENVIRONMENT_UPDATES_NONE };

    Queue<Handle<Entity>> m_entities_pending_addition;
    Queue<Handle<Entity>> m_entities_pending_removal;
    Queue<Handle<Entity>> m_entity_renderable_attribute_updates;
    BinarySemaphore m_entity_update_sp;

    TypeMap<UniquePtr<RenderComponentBase>> m_render_components; // only touch from render thread
    TypeMap<UniquePtr<RenderComponentBase>> m_render_components_pending_init;
    TypeMap<UniquePtr<RenderComponentBase>> m_render_components_pending_addition;
    FlatSet<RenderComponentPendingRemovalEntry> m_render_components_pending_removal;
    std::mutex m_render_component_mutex;
    UInt32 m_current_enabled_render_components_mask;
    UInt32 m_next_enabled_render_components_mask;

    Handle<ParticleSystem> m_particle_system;

    RTRadianceRenderer m_rt_radiance;
    ProbeGrid m_probe_system;
    bool m_has_rt_radiance;
    Handle<TLAS> m_tlas;

    Float m_global_timer;
    UInt32 m_frame_counter;
};

} // namespace hyperion::v2

#endif