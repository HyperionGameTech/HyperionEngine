#include "Light.hpp"
#include "../Engine.hpp"

#include <util/ByteUtil.hpp>

namespace hyperion::v2 {

Light::Light(
    LightType type,
    const Vector3 &position,
    const Vector4 &color,
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

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_LIGHTS, [this](Engine *engine) {
        EnqueueRenderUpdates();

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_LIGHTS, [this](Engine *engine) {
            SetReady(false);

            HYP_FLUSH_RENDER_QUEUE(engine);
        }), engine);
    }));
}

void Light::Update(Engine *engine, GameCounter::TickUnit delta)
{
    if (m_shader_data_state.IsDirty()) {
        EnqueueRenderUpdates();
    }
}

void Light::EnqueueRenderUpdates() const
{
    LightShaderData shader_data {
        .position         = Vector4(m_position, m_type == LightType::DIRECTIONAL ? 0.0f : 1.0f),
        .color            = ByteUtil::PackColorU32(m_color),
        .light_type       = static_cast<UInt32>(m_type),
        .intensity        = m_intensity,
        .radius           = m_radius,
        .shadow_map_index = m_shadow_map_index
    };

    GetEngine()->GetRenderScheduler().Enqueue([this, shader_data](...) {
        GetEngine()->shader_globals->lights.Set(GetId().value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}


} // namespace hyperion::v2
