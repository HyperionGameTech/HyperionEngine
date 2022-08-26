#include "RenderEnvironment.hpp"
#include <Engine.hpp>

#include <rendering/backend/RendererFrame.hpp>

namespace hyperion::v2 {

RenderEnvironment::RenderEnvironment(Scene *scene)
    : EngineComponentBase(),
      m_scene(scene),
      m_global_timer(0.0f),
      m_current_enabled_render_components_mask(0),
      m_next_enabled_render_components_mask(0)
{
}

RenderEnvironment::~RenderEnvironment()
{
    Teardown();
}

void RenderEnvironment::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ENVIRONMENTS, [this](...) {
        auto *engine = GetEngine();

        const auto update_marker_value = m_update_marker.load();

        { // lights
            if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_LIGHTS) {
                m_light_update_sp.Wait();

                while (m_lights_pending_addition.Any()) {
                    auto &front = m_lights_pending_addition.Front();

                    if (front != nullptr) {
                        const auto id = front->GetId();

                        m_lights.Insert(id, std::move(front));
                    }

                    m_lights_pending_addition.Pop();
                }

                while (m_lights_pending_removal.Any()) {
                    auto &front = m_lights_pending_removal.Front();

                    if (front != nullptr) {
                        const auto id = front->GetId();

                        m_lights.Erase(id);
                    }

                    m_lights_pending_removal.Pop();
                }

                m_light_update_sp.Signal();

                m_update_marker &= ~RENDER_ENVIRONMENT_UPDATES_LIGHTS;
            }

            for (auto &it: m_lights) {
                auto &light = it.second;

                if (light != nullptr) {
                    Attach(light);
                }
            }
        }

        { // env probes
            if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_ENV_PROBES) {
                m_env_probes_update_sp.Wait();

                while (m_env_probes_pending_addition.Any()) {
                    auto &front = m_env_probes_pending_addition.Front();

                    if (front != nullptr) {
                        const auto id = front->GetId();

                        m_env_probes.Insert(id, std::move(front));
                    }

                    m_env_probes_pending_addition.Pop();
                }

                while (m_env_probes_pending_removal.Any()) {
                    auto &front = m_env_probes_pending_removal.Front();

                    if (front != nullptr) {
                        const auto id = front->GetId();

                        m_env_probes.Erase(id);
                    }

                    m_env_probes_pending_removal.Pop();
                }

                m_env_probes_update_sp.Signal();

                m_update_marker &= ~RENDER_ENVIRONMENT_UPDATES_ENV_PROBES;
            }

            for (auto &it: m_env_probes) {
                auto &env_probe = it.second;

                if (env_probe != nullptr) {
                    env_probe.Init();
                }
            }
        }

        SetReady(true);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            m_lights.Clear();

            const auto update_marker_value = m_update_marker.load();

            if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_LIGHTS) {
                m_light_update_sp.Wait();

                m_lights_pending_addition.Clear();
                m_lights_pending_removal.Clear();

                m_light_update_sp.Signal();
            }

            if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_ENV_PROBES) {
                m_env_probes_update_sp.Wait();

                m_env_probes_pending_addition.Clear();
                m_env_probes_pending_removal.Clear();

                m_env_probes_update_sp.Signal();
            }

            // have to store render components into temporary container
            // from render thread so we don't run into a race condition.
            // and can't directly Clear() because destructors may enqueue
            // into render scheduler
            ComponentSetUnique<RenderComponentBase> tmp_render_components;

            engine->GetRenderScheduler().Enqueue([this, &tmp_render_components](...) {
                m_current_enabled_render_components_mask = 0u;
                m_next_enabled_render_components_mask = 0u;

                // m_render_components.Clear();
                tmp_render_components = std::move(m_render_components);

                HYPERION_RETURN_OK;
            });

            if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
                m_render_component_sp.Wait();

                m_render_components_pending_addition.Clear();
                m_render_components_pending_removal.Clear();

                m_render_component_sp.Signal();
            }

            if (update_marker_value) {
                m_update_marker.store(RENDER_ENVIRONMENT_UPDATES_NONE);
            }

            HYP_FLUSH_RENDER_QUEUE(engine);

            tmp_render_components.Clear();

            SetReady(false);
        });
    }));
}

void RenderEnvironment::AddLight(Handle<Light> &&light)
{
    if (!light) {
        return;
    }

    Attach(light);

    m_light_update_sp.Wait();
    m_lights_pending_addition.Push(std::move(light));
    m_light_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_LIGHTS;
}

void RenderEnvironment::RemoveLight(Handle<Light> &&light)
{
    if (!light) {
        return;
    }

    m_light_update_sp.Wait();
    m_lights_pending_removal.Push(std::move(light));
    m_light_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_LIGHTS;
}

void RenderEnvironment::AddEnvProbe(Ref<EnvProbe> &&env_probe)
{
    if (env_probe != nullptr && IsReady()) {
        env_probe.Init();
    }

    m_env_probes_update_sp.Wait();

    m_env_probes_pending_addition.Push(std::move(env_probe));

    m_env_probes_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENV_PROBES;
}

void RenderEnvironment::RemoveEnvProbe(Ref<EnvProbe> &&env_probe)
{
    m_env_probes_update_sp.Wait();

    m_env_probes_pending_removal.Push(std::move(env_probe));
    m_env_probes_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENV_PROBES;
}

void RenderEnvironment::Update(Engine *engine, GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_global_timer += delta;

    for (auto &it : m_lights) {
        it.second->Update(engine, delta);
    }

    HYP_USED AtomicWaiter waiter(m_updating_render_components);

    for (const auto &component : m_render_components) {
        component.second->ComponentUpdate(engine, delta);
    }
}

void RenderEnvironment::OnEntityAdded(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entities_pending_addition.Push(entity.IncRef());
    m_entity_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

void RenderEnvironment::OnEntityRemoved(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entities_pending_removal.Push(entity.IncRef());
    m_entity_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

// only called when meaningful attributes have changed
void RenderEnvironment::OnEntityRenderableAttributesChanged(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entity_renderable_attribute_updates.Push(entity.IncRef());
    m_entity_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

void RenderEnvironment::RenderComponents(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_current_enabled_render_components_mask = m_next_enabled_render_components_mask;

    const auto update_marker_value = m_update_marker.load();

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_LIGHTS) {
        m_light_update_sp.Wait();

        while (m_lights_pending_addition.Any()) {
            auto &front = m_lights_pending_addition.Front();

            if (front != nullptr) {
                const auto id = front->GetId();

                GetEngine()->render_state.BindLight(id);

                m_lights.Insert(id, std::move(front));
            }

            m_lights_pending_addition.Pop();
        }

        while (m_lights_pending_removal.Any()) {
            const auto &front = m_lights_pending_removal.Front();

            if (front != nullptr) {
                const auto id = front->GetId();

                // TODO: scene switch wouldnt unbind light
                GetEngine()->render_state.UnbindLight(id);

                m_lights.Erase(id);
            }

            m_lights_pending_removal.Pop();
        }

        m_light_update_sp.Signal();
    }

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_ENV_PROBES) {
        m_env_probes_update_sp.Wait();

        while (m_env_probes_pending_addition.Any()) {
            auto &front = m_env_probes_pending_addition.Front();

            if (front != nullptr) {
                const auto id = front->GetId();

                m_env_probes.Insert(id, std::move(front));
            }

            m_env_probes_pending_addition.Pop();
        }

        while (m_env_probes_pending_removal.Any()) {
            const auto &front = m_env_probes_pending_removal.Front();

            if (front != nullptr) {
                const auto id = front->GetId();

                // TODO: scene switch wouldnt unbind light
                // GetEngine()->render_state.UnbindLight(id);

                m_env_probes.Erase(id);
            }

            m_env_probes_pending_removal.Pop();
        }

        m_env_probes_update_sp.Signal();
    }

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_ENTITIES) {
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

    for (const auto &it : m_lights) {
        GetEngine()->render_state.BindLight(it.first);
    }

    for (const auto &it : m_env_probes) {
        GetEngine()->render_state.BindEnvProbe(it.first);
    }

    for (const auto &component : m_render_components) {
        m_next_enabled_render_components_mask |= 1u << static_cast<UInt32>(component.second->GetName());
        component.second->ComponentRender(engine, frame);
    }

    // add RenderComponents deferred so that the render scheduler can complete
    // initialization tasks before we call ComponentRender() on the newly added RenderComponents

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
        AtomicLocker locker(m_updating_render_components);

        m_render_component_sp.Wait();

        for (auto &it : m_render_components_pending_addition) {
            AssertThrow(it.second != nullptr);

            it.second->SetComponentIndex(0); // just using zero for now, when multiple of same components are supported, we will extend this
            it.second->ComponentInit(engine);

            m_render_components.Set(it.first, std::move(it.second));
        }

        m_render_components_pending_addition.Clear();

        for (const auto &it : m_render_components_pending_removal) {
            m_next_enabled_render_components_mask &= ~(1u << static_cast<UInt32>(it.second));
            m_render_components.Remove(it.first);
        }

        m_render_components_pending_removal.Clear();

        m_render_component_sp.Signal();
    }

    if (update_marker_value != RENDER_ENVIRONMENT_UPDATES_NONE) {
        m_update_marker.store(RENDER_ENVIRONMENT_UPDATES_NONE);
    }
}

} // namespace hyperion::v2
