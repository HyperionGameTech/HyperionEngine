#include <rendering/Light.hpp>
#include <Engine.hpp>
#include <Threads.hpp>

#include <util/ByteUtil.hpp>

namespace hyperion::v2 {

class Light;

struct RENDER_COMMAND(BindLight) : RenderCommandBase2
{
    Light::ID id;

    RENDER_COMMAND(BindLight)(Light::ID id)
        : id(id)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        Engine::Get()->GetRenderState().BindLight(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindLight) : RenderCommandBase2
{
    Light::ID id;

    RENDER_COMMAND(UnbindLight)(Light::ID id)
        : id(id)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        Engine::Get()->GetRenderState().UnbindLight(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateLightShaderData) : RenderCommandBase2
{
    Light &light;
    LightDrawProxy draw_proxy;

    RENDER_COMMAND(UpdateLightShaderData)(Light &light, LightDrawProxy &&draw_proxy)
        : light(light),
          draw_proxy(std::move(draw_proxy))
    {
    }

    virtual Result operator()(Engine *engine)
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

Light::~Light()
{
    Teardown();
}

void Light::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(Engine::Get());

    EnqueueRenderUpdates();

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        HYP_FLUSH_RENDER_QUEUE();
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
