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

    m_particle_system = engine->CreateHandle<ParticleSystem>();
    engine->InitObject(m_particle_system);
    
    const auto update_marker_value = m_update_marker.load();

    if (m_render_components_pending_addition.Any()) {
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

        const auto update_marker_value = m_update_marker.load();

        // have to store render components into temporary container
        // from render thread so we don't run into a race condition.
        // and can't directly Clear() because destructors may enqueue
        // into render scheduler
        TypeMap<UniquePtr<RenderComponentBase>> tmp_render_components;

        engine->GetRenderScheduler().Enqueue([this, &tmp_render_components](...) {
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

        if (update_marker_value) {
            m_update_marker.store(RENDER_ENVIRONMENT_UPDATES_NONE);
        }

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

    m_render_component_sp.Signal();
}

void RenderEnvironment::OnEntityAdded(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entities_pending_addition.Push(Handle<Entity>(entity));
    m_entity_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

void RenderEnvironment::OnEntityRemoved(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entities_pending_removal.Push(Handle<Entity>(entity));
    m_entity_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

// only called when meaningful attributes have changed
void RenderEnvironment::OnEntityRenderableAttributesChanged(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_update_sp.Wait();
    m_entity_renderable_attribute_updates.Push(Handle<Entity>(entity));
    m_entity_update_sp.Signal();
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

void RenderEnvironment::RenderComponents(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_current_enabled_render_components_mask = m_next_enabled_render_components_mask;

    const auto update_marker_value = m_update_marker.load();

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

    for (const auto &component : m_render_components) {
        m_next_enabled_render_components_mask |= 1u << static_cast<UInt32>(component.second->GetName());
        component.second->ComponentRender(engine, frame);
    }

    // add RenderComponents deferred so that the render scheduler can complete
    // initialization tasks before we call ComponentRender() on the newly added RenderComponents

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
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

    if (update_marker_value != RENDER_ENVIRONMENT_UPDATES_NONE) {
        m_update_marker.store(RENDER_ENVIRONMENT_UPDATES_NONE);
    }
}

} // namespace hyperion::v2
