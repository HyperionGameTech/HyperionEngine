#include <rendering/Light.hpp>
#include <Engine.hpp>
#include <Threads.hpp>

#include <util/ByteUtil.hpp>

namespace hyperion::v2 {

Light::Light(
    LightType type,
    const Vector3 &position,
    const Color &color,
    float intensity,
    float radius
) : EngineComponentBase(),
    m_type(type),
    m_position(position),
    m_color(color),
    m_intensity(intensity),
    m_radius(radius),
    m_shadow_map_index(~0u),
    m_shader_data_state(ShaderDataState::DIRTY)
{
}

Light::~Light()
{
    Teardown();
}

void Light::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    EnqueueRenderUpdates();

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        HYP_FLUSH_RENDER_QUEUE(GetEngine());
    });
}

void Light::EnqueueBind(Engine *engine) const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    GetEngine()->GetRenderScheduler().Enqueue([engine, id = m_id](...) {
        engine->render_state.BindLight(id);

        HYPERION_RETURN_OK;
    });
}

void Light::EnqueueUnbind(Engine *engine) const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    GetEngine()->GetRenderScheduler().Enqueue([engine, id = m_id](...) {
        engine->render_state.UnbindLight(id);

        HYPERION_RETURN_OK;
    });
}

void Light::Update(Engine *)
{
    if (m_shader_data_state.IsDirty()) {
        EnqueueRenderUpdates();
    }
}

void Light::EnqueueRenderUpdates()
{
    LightDrawProxy draw_proxy {
        .id = m_id,
        .type = m_type,
        .color = m_color,
        .radius = m_radius,
        .position_intensity = Vector4(m_position, m_intensity),
        .shadow_map_index = m_shadow_map_index
    };

    GetEngine()->GetRenderScheduler().Enqueue([this, draw_proxy = std::move(draw_proxy)](...) mutable {
        m_draw_proxy = draw_proxy;

        GetEngine()->GetRenderData()->lights.Set(
            GetID().value - 1,
            m_draw_proxy
        );

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}


} // namespace hyperion::v2
