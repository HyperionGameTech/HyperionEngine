#include "light.h"
#include "../engine.h"

#include <util/byte_util.h>

namespace hyperion::v2 {

Light::Light(
    LightType type,
    const Vector3 &position,
    const Vector4 &color,
    float intensity
) : m_type(type),
    m_position(position),
    m_color(color),
    m_intensity(intensity),
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
        EnqueueRenderUpdates(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_LIGHTS, [this](Engine *engine) {
            SetReady(false);

            /* no-op */
        }), engine);
    }));
}

void Light::EnqueueRenderUpdates(Engine *engine) const
{
    LightShaderData shader_data{
        .position         = Vector4(m_position, m_type == LightType::DIRECTIONAL ? 0.0f : 1.0f),
        .color            = ByteUtil::PackColorU32(m_color),
        .light_type       = static_cast<uint32_t>(m_type),
        .intensity        = m_intensity,
        .shadow_map_index = ~0u
    };
    
    engine->shader_globals->lights.Set(m_id.value - 1, shader_data);

    m_shader_data_state = ShaderDataState::CLEAN;
}


} // namespace hyperion::v2
