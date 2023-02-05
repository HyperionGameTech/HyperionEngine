#include <rendering/CubemapRenderer.hpp>

#include <Engine.hpp>
#include <scene/camera/PerspectiveCamera.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

CubemapRenderer::CubemapRenderer(
    const Vector3 &origin
) : EngineComponentBase(),
    RenderComponent(5),
    m_aabb(BoundingBox(origin - 150.0f, origin + 150.0f))
{
}

CubemapRenderer::CubemapRenderer(
    const BoundingBox &aabb
) : EngineComponentBase(),
    RenderComponent(5),
    m_aabb(aabb)
{
}

CubemapRenderer::~CubemapRenderer()
{
    SetReady(false);
}

void CubemapRenderer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();
    
    m_env_probe = CreateObject<EnvProbe>(
        Handle<Scene>(GetParent()->GetScene()->GetID()),
        m_aabb,
        Extent2D { 512, 512 },
        ENV_PROBE_TYPE_REFLECTION
    );

    InitObject(m_env_probe);

    SetReady(true);
}

// called from game thread
void CubemapRenderer::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    AssertThrow(m_env_probe.IsValid());
    GetParent()->GetScene()->AddEnvProbe(m_env_probe);
}

void CubemapRenderer::OnRemoved()
{
    AssertReady();

    AssertThrow(m_env_probe.IsValid());
    GetParent()->GetScene()->RemoveEnvProbe(m_env_probe->GetID());
}

void CubemapRenderer::OnUpdate(GameCounter::TickUnit delta)
{
}

void CubemapRenderer::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    m_env_probe->Render(frame);
}

void CubemapRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

} // namespace hyperion::v2
