#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <scene/camera/PerspectiveCamera.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

ReflectionProbeRenderer::ReflectionProbeRenderer(
    Name name,
    const Vector3 &origin
) : BasicObject(),
    RenderComponent(name, 5),
    m_aabb(BoundingBox(origin - 150.0f, origin + 150.0f))
{
}

ReflectionProbeRenderer::ReflectionProbeRenderer(
    Name name,
    const BoundingBox &aabb
) : BasicObject(),
    RenderComponent(name, 5),
    m_aabb(aabb)
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();
    
    m_env_probe = CreateObject<EnvProbe>(
        Handle<Scene>(GetParent()->GetScene()->GetID()),
        m_aabb,
        Extent2D { 256, 256 },
        ENV_PROBE_TYPE_REFLECTION
    );

    InitObject(m_env_probe);

    SetReady(true);
}

// called from game thread
void ReflectionProbeRenderer::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    AssertThrow(m_env_probe.IsValid());
    m_env_probe->EnqueueBind();

    m_last_visibility_state = true;
}

void ReflectionProbeRenderer::OnRemoved()
{
    AssertReady();

    AssertThrow(m_env_probe.IsValid());
    m_env_probe->EnqueueUnbind();
}

void ReflectionProbeRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    m_env_probe->Update(delta);

    // if (!GetParent()->GetScene()->GetCamera().IsValid()) {
    //     return;
    // }

    // const Handle<Camera> &camera = GetParent()->GetScene()->GetCamera();

    // const bool is_env_probe_in_frustum = camera->GetFrustum().ContainsAABB(m_env_probe->GetAABB());

    // m_env_probe->SetIsVisible(camera.GetID(), is_env_probe_in_frustum);
}

void ReflectionProbeRenderer::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    m_env_probe->Render(frame);

    // if (m_env_probe->GetDrawProxy().visibility_bits & (1ull << SizeType(GetParent()->GetScene()->GetCamera().GetID().ToIndex()))) {
    //     if (!m_last_visibility_state) {
    //         g_engine->GetRenderState().BindEnvProbe(m_env_probe->GetEnvProbeType(), m_env_probe->GetID());

    //         m_last_visibility_state = true;
    //     }

    //     m_env_probe->Render(frame);
    // } else {
    //     // No point in keeping it bound if the light is not visible on the screen.
    //     if (m_last_visibility_state) {
    //         g_engine->GetRenderState().UnbindEnvProbe(m_env_probe->GetEnvProbeType(), m_env_probe->GetID());

    //         m_last_visibility_state = false;
    //     }
    // }
}

void ReflectionProbeRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

} // namespace hyperion::v2
