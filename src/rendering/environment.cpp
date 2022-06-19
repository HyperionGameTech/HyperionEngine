#include "environment.h"
#include <engine.h>

#include <rendering/backend/renderer_frame.h>

namespace hyperion::v2 {

Environment::Environment()
    : EngineComponentBase(),
      m_global_timer(0.0f)
{
}

Environment::~Environment()
{
    Teardown();
}

void Environment::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ENVIRONMENTS, [this](Engine *engine) {
        /*for (auto &texture : m_environment_textures) {
            if (texture != nullptr) {
                texture.Init();
            }
        }*/

        for (auto &light : m_lights) {
            if (light != nullptr) {
                light.Init();
            }
        }

        for (auto &it : m_render_components) {
            AssertThrow(it.second != nullptr);

            it.second->ComponentInit(engine);
        }

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ENVIRONMENTS, [this](Engine *engine) {
            m_lights.clear();
            m_render_components.Clear();

            if (m_has_render_component_updates) {
                std::lock_guard guard(m_render_component_mutex);

                m_render_components_pending_addition.Clear();
                m_render_components_pending_removal.Clear();

                m_has_render_component_updates = false;
            }

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

void Environment::AddLight(Ref<Light> &&light)
{
    if (light != nullptr && IsReady()) {
        light.Init();
    }

    m_lights.push_back(std::move(light));
}

void Environment::Update(Engine *engine, GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_global_timer += delta;

    HYP_USED AtomicWaiter waiter(m_updating_render_components);

    for (const auto &component : m_render_components) {
        component.second->ComponentUpdate(engine, delta);
    }
}

void Environment::RenderComponents(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (m_has_render_component_updates) {
        AtomicLocker locker(m_updating_render_components);

        std::lock_guard guard(m_render_component_mutex);

        for (auto &it : m_render_components_pending_addition) {
            AssertThrow(it.second != nullptr);

            it.second->SetComponentIndex(0); // just using zero for now, when multiple of same components are supported, we will extend this
            it.second->ComponentInit(engine);

            m_render_components.Set(it.first, std::move(it.second));
        }

        m_render_components_pending_addition.Clear();

        for (auto &it : m_render_components_pending_removal) {
            m_render_components.Remove(it);
        }

        m_render_components_pending_removal.Clear();

        m_has_render_component_updates = false;
    }

    for (const auto &component : m_render_components) {
        component.second->ComponentRender(engine, frame);
    }
}

} // namespace hyperion::v2
