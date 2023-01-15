#include <rendering/PointShadowRenderer.hpp>

#include <Engine.hpp>
#include <rendering/Light.hpp>
#include <rendering/Texture.hpp>

#include <scene/camera/PerspectiveCamera.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

PointShadowRenderer::PointShadowRenderer(
    const Handle<Light> &light,
    const Extent2D &extent
) : RenderComponent(5),
    m_light(light),
    m_extent(extent)
{
}

PointShadowRenderer::~PointShadowRenderer() = default;

void PointShadowRenderer::Init()
{
    if (!InitObject(m_light)) {
        DebugLog(
            LogType::Warn,
            "Point shadow renderer attached to invalid Light\n"
        );

        return;
    }

    m_aabb = m_light->GetWorldAABB();

    m_env_probe = CreateObject<EnvProbe>(
        Handle<Scene>(GetParent()->GetScene()->GetID()),
        m_aabb,
        m_extent,
        ENV_PROBE_TYPE_SHADOW
    );

    InitObject(m_env_probe);

    m_light->SetShadowMapIndex(m_env_probe->GetID().ToIndex());
}

// called from game thread
void PointShadowRenderer::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_env_probe.IsValid());
    GetParent()->GetScene()->AddEnvProbe(m_env_probe);
}

void PointShadowRenderer::OnRemoved()
{
    AssertThrow(m_env_probe.IsValid());
    GetParent()->GetScene()->RemoveEnvProbe(m_env_probe->GetID());
}

void PointShadowRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_env_probe.IsValid());
    AssertThrow(m_light.IsValid());

    m_env_probe->SetAABB(m_light->GetWorldAABB());
}

void PointShadowRenderer::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(m_env_probe.IsValid());
    AssertThrow(m_light.IsValid());

    if (m_light->GetDrawProxy().visibility_bits & (1ull << SizeType(GetParent()->GetSceneID().ToIndex()))) {
        if (!m_last_visibility_state) {
            Engine::Get()->GetRenderState().BindEnvProbe(m_env_probe->GetEnvProbeType(), m_env_probe->GetID());

            m_last_visibility_state = true;
        }

        // TODO: Add Octree hash check, only re-render if something has changed.

        m_env_probe->Render(frame);
    } else {
        // No point in keeping it bound if the light is not visible on the screen.
        if (m_last_visibility_state) {
            Engine::Get()->GetRenderState().UnbindEnvProbe(m_env_probe->GetEnvProbeType(), m_env_probe->GetID());

            m_last_visibility_state = false;
        }
    }
}

void PointShadowRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

} // namespace hyperion::v2
