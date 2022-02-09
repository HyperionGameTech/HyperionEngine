#include "envmap_probe_camera.h"

#include "../../renderer.h"
#include "../../camera/perspective_camera.h"

namespace hyperion {
EnvMapProbeCamera::EnvMapProbeCamera(const ProbeRegion &region, int width, int height, float near, float far)
    : ProbeCamera(fbom::FBOMObjectType("ENVMAP_PROBE_CAMERA"), region),
      m_texture(nullptr)
{
    m_camera = new PerspectiveCamera(90.0f, width, height, near, far);
}

EnvMapProbeCamera::~EnvMapProbeCamera()
{
    delete m_camera;
}

const Texture *EnvMapProbeCamera::GetTexture() const
{
    return m_texture;
}

void EnvMapProbeCamera::Update(double dt)
{
    //m_camera->SetTranslation(m_region.origin);
    m_camera->SetDirection(m_region.direction);
    m_camera->SetUpVector(m_region.up_vector);
    m_camera->Update(dt);
}

void EnvMapProbeCamera::Render(Renderer *renderer, Camera *camera)
{
    // no-op
    m_camera->SetTranslation(camera->GetTranslation());
}

std::shared_ptr<Renderable> EnvMapProbeCamera::CloneImpl()
{
    return std::make_shared<EnvMapProbeCamera>(m_region, m_camera->GetWidth(), m_camera->GetHeight(), m_camera->GetNear(), m_camera->GetFar());
}
} // namespace hyperion