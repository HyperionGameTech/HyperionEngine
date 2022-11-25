#include "EnvProbe.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

class EnvProbe;

struct RENDER_COMMAND(BindEnvProbe) : RenderCommandBase2
{
    EnvProbe::ID id;

    RENDER_COMMAND(BindEnvProbe)(EnvProbe::ID id)
        : id(id)
    {
    }

    virtual Result operator()()
    {
        Engine::Get()->GetRenderState().BindEnvProbe(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindEnvProbe) : RenderCommandBase2
{
    EnvProbe::ID id;

    RENDER_COMMAND(UnbindEnvProbe)(EnvProbe::ID id)
        : id(id)
    {
    }

    virtual Result operator()()
    {
        Engine::Get()->GetRenderState().UnbindEnvProbe(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateEnvProbeRenderData) : RenderCommandBase2
{
    EnvProbe &env_probe;
    EnvProbeDrawProxy draw_proxy;

    RENDER_COMMAND(UpdateEnvProbeRenderData)(EnvProbe &env_probe, EnvProbeDrawProxy &&draw_proxy)
        : env_probe(env_probe),
          draw_proxy(std::move(draw_proxy))
    {
    }

    virtual Result operator()()
    {
        // find the texture slot of this env probe (if it exists)
        UInt texture_index = ~0u;

        const auto it = Engine::Get()->render_state.env_probes.Find(env_probe.GetID());

        if (it != Engine::Get()->render_state.env_probes.End() && it->second.Any()) {
            texture_index = it->second.Get();
        }

        Engine::Get()->GetRenderData()->env_probes.Set(
            env_probe.GetID().ToIndex(),
            EnvProbeShaderData {
                .aabb_max = Vector4(draw_proxy.aabb.max, 1.0f),
                .aabb_min = Vector4(draw_proxy.aabb.min, 1.0f),
                .world_position = Vector4(draw_proxy.world_position, 1.0f),
                .texture_index = texture_index,
                .flags = static_cast<UInt32>(draw_proxy.flags)
            }
        );

        // update cubemap texture in array of bound env probes
        if (texture_index != ~0u) {
            const auto &descriptor_pool = Engine::Get()->GetInstance()->GetDescriptorPool();

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                const auto *descriptor_set = descriptor_pool.GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
                auto *descriptor = descriptor_set->GetDescriptor(DescriptorKey::ENV_PROBE_TEXTURES);

                descriptor->SetElementSRV(
                    texture_index,
                    env_probe.m_texture
                        ? &env_probe.m_texture->GetImageView()
                        : &Engine::Get()->GetPlaceholderData().GetImageViewCube1x1R8()
                );
            }
        }

        // update m_draw_proxy on render thread.
        env_probe.m_draw_proxy = draw_proxy;

        HYPERION_RETURN_OK;
    }
};


EnvProbe::EnvProbe(Handle<Texture> &&texture)
    : EngineComponentBase(),
      m_texture(std::move(texture)),
      m_needs_update(true)
{
}

EnvProbe::EnvProbe(Handle<Texture> &&texture, const BoundingBox &aabb)
    : EngineComponentBase(),
      m_texture(std::move(texture)),
      m_aabb(aabb),
      m_world_position(aabb.GetCenter()),
      m_needs_update(true)
{
}

EnvProbe::~EnvProbe()
{
    Teardown();
}

void EnvProbe::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(Engine::Get());

    Engine::Get()->InitObject(m_texture);

    SetReady(true);

    // force update first time to init render data
    Update();

    OnTeardown([this]() {
        SetReady(false);

        Engine::Get()->SafeReleaseHandle<Texture>(std::move(m_texture));
        
        HYP_FLUSH_RENDER_QUEUE();
    });
}

void EnvProbe::EnqueueBind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    RenderCommands::Push<RENDER_COMMAND(BindEnvProbe)>(m_id);
}

void EnvProbe::EnqueueUnbind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    RenderCommands::Push<RENDER_COMMAND(UnbindEnvProbe)>(m_id);
}

void EnvProbe::Update()
{
    AssertReady();

    // if (!m_needs_update) {
    //     return;
    // }

    RenderCommands::Push<RENDER_COMMAND(UpdateEnvProbeRenderData)>(*this, EnvProbeDrawProxy {
        .id = m_id,
        .aabb = GetAABB(),
        .world_position = m_world_position,
        .flags = IsParallaxCorrected()
            ? EnvProbeFlags::ENV_PROBE_PARALLAX_CORRECTED
            : EnvProbeFlags::ENV_PROBE_NONE
    });

    m_needs_update = false;
}

} // namespace hyperion::v2
