#include <rendering/Light.hpp>
#include <Engine.hpp>
#include <Threads.hpp>

#include <util/ByteUtil.hpp>

namespace hyperion::v2 {

class Light;

struct RENDER_COMMAND(BindLight) : RenderCommand
{
    ID<Light> id;

    RENDER_COMMAND(BindLight)(ID<Light> id)
        : id(id)
    {
    }

    virtual Result operator()()
    {
        Engine::Get()->GetRenderState().BindLight(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindLight) : RenderCommand
{
    ID<Light> id;

    RENDER_COMMAND(UnbindLight)(ID<Light> id)
        : id(id)
    {
    }

    virtual Result operator()()
    {
        Engine::Get()->GetRenderState().UnbindLight(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateLightShaderData) : RenderCommand
{
    Light &light;
    LightDrawProxy draw_proxy;

    RENDER_COMMAND(UpdateLightShaderData)(Light &light, LightDrawProxy &&draw_proxy)
        : light(light),
          draw_proxy(std::move(draw_proxy))
    {
    }

    virtual Result operator()()
    {
        light.m_draw_proxy = draw_proxy;

        Engine::Get()->GetRenderData()->lights.Set(
            light.GetID().ToIndex(),
            LightShaderData {
                .light_id           = UInt32(draw_proxy.id),
                .light_type         = UInt32(draw_proxy.type),
                .color_packed       = UInt32(draw_proxy.color),
                .radius             = draw_proxy.radius,
                .shadow_map_index   = draw_proxy.shadow_map_index,
                .position_intensity = draw_proxy.position_intensity
            }
        );

        HYPERION_RETURN_OK;
    }
};

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

Light::Light(Light &&other) noexcept
    : EngineComponentBase(std::move(other)),
      m_type(other.m_type),
      m_position(other.m_position),
      m_color(other.m_color),
      m_intensity(other.m_intensity),
      m_radius(other.m_radius),
      m_shadow_map_index(other.m_shadow_map_index),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    other.m_shadow_map_index = ~0u;
}

// Light &Light::operator=(Light &&other) noexcept
// {
//     EngineComponentBase::operator=(std::move(other));

//     m_type = other.m_type;
//     m_position = other.m_position;
//     m_color = other.m_color;
//     m_intensity = other.m_intensity;
//     m_radius = other.m_radius;
//     m_shadow_map_index = other.m_shadow_map_index;
//     m_shader_data_state = ShaderDataState::DIRTY;

//     other.m_shadow_map_index = ~0u;

//     return *this;
// }

Light::~Light()
{
    Teardown();
}

void Light::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    EnqueueRenderUpdates();

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        HYP_SYNC_RENDER();
    });
}

void Light::EnqueueBind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    RenderCommands::Push<RENDER_COMMAND(BindLight)>(m_id);
}

void Light::EnqueueUnbind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    RenderCommands::Push<RENDER_COMMAND(UnbindLight)>(m_id);
}

void Light::Update()
{
    if (m_shader_data_state.IsDirty()) {
        EnqueueRenderUpdates();
    }
}

void Light::EnqueueRenderUpdates()
{
    RenderCommands::Push<RENDER_COMMAND(UpdateLightShaderData)>(*this, LightDrawProxy {
        .id = m_id,
        .type = m_type,
        .color = m_color,
        .radius = m_radius,
        .shadow_map_index = m_shadow_map_index,
        .position_intensity = Vector4(m_position, m_intensity)
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}


} // namespace hyperion::v2
