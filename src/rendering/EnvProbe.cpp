#include "EnvProbe.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

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

void EnvProbe::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);
    
    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](...) {
        auto *engine = GetEngine();

        if (m_texture) {
            m_texture->Init(engine);
        }

        SetReady(true);

        // force update first time to init render data
        Update(engine);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            SetReady(false);

            engine->SafeReleaseRenderResource<Texture>(std::move(m_texture));
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        });
    }));
}

void EnvProbe::Update(Engine *engine)
{
    AssertReady();

    // if (!m_needs_update) {
    //     return;
    // }

    const EnvProbeDrawProxy draw_proxy {
        .id = m_id,
        .aabb = GetAABB(),
        .world_position = m_world_position,
        .flags = IsParallaxCorrected()
            ? EnvProbeFlags::ENV_PROBE_PARALLAX_CORRECTED
            : EnvProbeFlags::ENV_PROBE_NONE
    };

    GetEngine()->render_scheduler.Enqueue([this, draw_proxy](...) {
        // update m_draw_proxy on render thread.
        m_draw_proxy = draw_proxy;

        // find the texture slot of this env probe (if it exists)
        UInt texture_index = ~0u;

        const auto it = GetEngine()->render_state.env_probes.Find(m_id);

        if (it != GetEngine()->render_state.env_probes.End() && it->second.Any()) {
            texture_index = it->second.Get();
        }

        GetEngine()->shader_globals->env_probes.Set(
            m_id.value - 1,
            EnvProbeShaderData {
                .aabb_max = Vector4(m_draw_proxy.aabb.max, 1.0f),
                .aabb_min = Vector4(m_draw_proxy.aabb.min, 1.0f),
                .world_position = Vector4(m_draw_proxy.world_position, 1.0f),
                .texture_index = texture_index,
                .flags = static_cast<UInt32>(m_draw_proxy.flags)
            }
        );

        // update cubemap texture in array of bound env probes
        if (texture_index != ~0u) {
            const auto &descriptor_pool = GetEngine()->GetInstance()->GetDescriptorPool();

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                const auto *descriptor_set = descriptor_pool.GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
                auto *descriptor = descriptor_set->GetDescriptor(DescriptorKey::ENV_PROBE_TEXTURES);

                descriptor->SetSubDescriptor({
                    .element_index = texture_index,
                    .image_view = m_texture
                        ? &m_texture->GetImageView()
                        : &GetEngine()->GetPlaceholderData().GetImageViewCube1x1R8()
                });
            }
        }

        HYPERION_RETURN_OK;
    });

    m_needs_update = false;
}

} // namespace hyperion::v2
