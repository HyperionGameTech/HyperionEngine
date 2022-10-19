#include "RenderEnvironment.hpp"
#include <Engine.hpp>

#include <rendering/backend/RendererFrame.hpp>

namespace hyperion::v2 {

RenderEnvironment::RenderEnvironment(Scene *scene)
    : EngineComponentBase(),
      m_scene(scene),
      m_global_timer(0.0f),
      m_frame_counter(0),
      m_current_enabled_render_components_mask(0),
      m_next_enabled_render_components_mask(0),
      m_rt_radiance(Extent2D { 1024, 1024 }),
      m_probe_system({
          .aabb = {{-300.0f, -10.0f, -300.0f}, {300.0f, 300.0f, 300.0f}}
      }),
      m_has_rt_radiance(false)
{
}

RenderEnvironment::~RenderEnvironment()
{
    Teardown();
}

void RenderEnvironment::SetTLAS(const Handle<TLAS> &tlas)
{
    m_tlas = tlas;

    if (!IsInitCalled()) {
        return;
    }

    GetEngine()->InitObject(m_tlas);

    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_TLAS);
}

void RenderEnvironment::SetTLAS(Handle<TLAS> &&tlas)
{
    m_tlas = std::move(tlas);
    
    if (!IsInitCalled()) {
        return;
    }

    GetEngine()->InitObject(m_tlas);

    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_TLAS);
}

void RenderEnvironment::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    m_particle_system = engine->CreateHandle<ParticleSystem>();
    engine->InitObject(m_particle_system);
    
    if (auto tlas = m_tlas.Lock()) {
        m_rt_radiance.SetTLAS(tlas);
        m_rt_radiance.Create(engine);

        m_probe_system.SetTLAS(tlas);
        m_probe_system.Init(engine);

        m_has_rt_radiance = true;
    }

    {
        std::lock_guard guard(m_render_component_mutex);
        
        for (auto &it : m_render_components_pending_addition) {
            AssertThrow(it.second != nullptr);
            it.second->ComponentInit(engine);
        }
    }

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        auto *engine = GetEngine();

        m_particle_system.Reset();
            
        if (engine->GetConfig().Get(CONFIG_RT_SUPPORTED) && m_has_rt_radiance) {
            m_rt_radiance.Destroy(engine);

            m_probe_system.Destroy(engine);

            m_has_rt_radiance = false;
        }

        const auto update_marker_value = m_update_marker.load();

        // have to store render components into temporary container
        // from render thread so we don't run into a race condition.
        // and can't directly Clear() because destructors may enqueue
        // into render scheduler
        TypeMap<UniquePtr<RenderComponentBase>> tmp_render_components;

        engine->GetRenderScheduler().Enqueue([this, engine, &tmp_render_components](...) {
            m_current_enabled_render_components_mask = 0u;
            m_next_enabled_render_components_mask = 0u;

            // m_render_components.Clear();
            tmp_render_components = std::move(m_render_components);

            HYPERION_RETURN_OK;
        });

        if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
            std::lock_guard guard(m_render_component_mutex);

            m_render_components_pending_addition.Clear();
            m_render_components_pending_removal.Clear();
        }

        m_update_marker.fetch_and(~RENDER_ENVIRONMENT_UPDATES_CONTAINERS);

        HYP_FLUSH_RENDER_QUEUE(engine);

        tmp_render_components.Clear();
    });
}

void RenderEnvironment::Update(Engine *engine, GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_global_timer += delta;

    std::lock_guard guard(m_render_component_mutex);

    for (const auto &component : m_render_components) {
        component.second->ComponentUpdate(engine, delta);
    }
}

void RenderEnvironment::OnEntityAdded(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entities_pending_addition.Push(Handle<Entity>(entity));
    m_entity_update_sp.Signal();
    
    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_ENTITIES);
}

void RenderEnvironment::OnEntityRemoved(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entities_pending_removal.Push(Handle<Entity>(entity));
    m_entity_update_sp.Signal();
    
    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_ENTITIES);
}

// only called when meaningful attributes have changed
void RenderEnvironment::OnEntityRenderableAttributesChanged(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entity_renderable_attribute_updates.Push(Handle<Entity>(entity));
    m_entity_update_sp.Signal();
    
    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_ENTITIES);
}

void RenderEnvironment::ApplyTLASUpdates(Engine *engine, Frame *frame, RTUpdateStateFlags flags)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    AssertThrow(engine->GetConfig().Get(CONFIG_RT_SUPPORTED));
    
    if (m_has_rt_radiance) {
        m_rt_radiance.ApplyTLASUpdates(engine, flags);
        m_probe_system.ApplyTLASUpdates(engine, flags);
    }
}

void RenderEnvironment::RenderRTRadiance(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    AssertThrow(engine->GetConfig().Get(CONFIG_RT_SUPPORTED));
    
    if (m_has_rt_radiance) {
        m_rt_radiance.Render(engine, frame);

        m_probe_system.RenderProbes(engine, frame);
        m_probe_system.ComputeIrradiance(engine, frame);
    }
}

void RenderEnvironment::RenderComponents(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    m_current_enabled_render_components_mask = m_next_enabled_render_components_mask;

    const auto update_marker_value = m_update_marker.load();
    UInt8 inverse_mask = 0;
    

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_ENTITIES) {
        inverse_mask |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;

        m_entity_update_sp.Wait();

        while (m_entities_pending_addition.Any()) {
            for (auto &it : m_render_components) {
                it.second->OnEntityAdded(m_entities_pending_addition.Front());
            }

            m_entities_pending_addition.Pop();
        }

        while (m_entity_renderable_attribute_updates.Any()) {
            for (auto &it : m_render_components) {
                it.second->OnEntityRenderableAttributesChanged(m_entity_renderable_attribute_updates.Front());
            }

            m_entity_renderable_attribute_updates.Pop();
        }

        while (m_entities_pending_removal.Any()) {
            for (auto &it : m_render_components) {
                it.second->OnEntityRemoved(m_entities_pending_removal.Front());
            }

            m_entities_pending_removal.Pop();
        }

        m_entity_update_sp.Signal();
    }

    for (const auto &component : m_render_components) {
        m_next_enabled_render_components_mask |= 1u << static_cast<UInt32>(component.second->GetName());
        component.second->ComponentRender(engine, frame);
    }

    // add RenderComponents AFTER rendering, so that the render scheduler can complete
    // initialization tasks before we call ComponentRender() on the newly added RenderComponents

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
        inverse_mask |= RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS;

        std::lock_guard guard(m_render_component_mutex);

        for (auto &it : m_render_components_pending_addition) {
            m_render_components.Set(it.first, std::move(it.second));
        }

        m_render_components_pending_addition.Clear();

        for (const auto &it : m_render_components_pending_removal) {
            m_next_enabled_render_components_mask &= ~(1u << static_cast<UInt32>(it.second));
            m_render_components.Remove(it.first);
        }

        m_render_components_pending_removal.Clear();
    }

    if (engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_TLAS) {
            inverse_mask |= RENDER_ENVIRONMENT_UPDATES_TLAS;

            if (auto tlas = m_tlas.Lock()) {
                if (m_has_rt_radiance) {
                    m_rt_radiance.Destroy(engine);
                    m_probe_system.Destroy(engine);
                }
                
                m_probe_system.SetTLAS(tlas);
                m_rt_radiance.SetTLAS(tlas);

                m_rt_radiance.Create(engine);
                m_probe_system.Init(engine);
                m_has_rt_radiance = true;

                HYP_FLUSH_RENDER_QUEUE(engine);
            } else {
                if (m_has_rt_radiance) {
                    m_rt_radiance.Destroy(engine);
                    m_probe_system.Destroy(engine);

                    HYP_FLUSH_RENDER_QUEUE(engine);
                }

                m_has_rt_radiance = false;
            }
        }

        // for RT, we may need to perform resizing of buffers, and thus modification of descriptor sets
        if (const auto &tlas = m_scene->GetTLAS()) {
            RTUpdateStateFlags update_state_flags = renderer::RT_UPDATE_STATE_FLAGS_NONE;

            tlas->UpdateRender(
                engine,
                frame,
                update_state_flags
            );

            if (update_state_flags) {
                ApplyTLASUpdates(engine, frame, update_state_flags);
            }
        }
    }

    if (update_marker_value) {
        m_update_marker.fetch_and(~inverse_mask);
    }

    ++m_frame_counter;
}

} // namespace hyperion::v2
