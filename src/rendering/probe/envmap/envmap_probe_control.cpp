#include "envmap_probe_control.h"
#include "envmap_probe.h"
#include "../probe_manager.h"
#include "../../../entity.h"

#include <memory>

namespace hyperion {
EnvMapProbeControl::EnvMapProbeControl(const Vector3 &origin, BoundingBox bounds)
    : EntityControl(fbom::FBOMObjectType("ENVMAP_PROBE_CONTROL"), 5.0),
      m_env_probe_node(new Entity("EnvMapProbeControl")),
      m_env_probe(new EnvMapProbe(origin, bounds, 128, 128, 0.1f, 50.0f)) // TODO
{
    m_env_probe_node->SetRenderable(m_env_probe);
}

void EnvMapProbeControl::OnAdded()
{
    parent->AddChild(m_env_probe_node);
    ProbeManager::GetInstance()->AddProbe(m_env_probe);
}

void EnvMapProbeControl::OnRemoved()
{
    parent->RemoveChild(m_env_probe_node);
    ProbeManager::GetInstance()->RemoveProbe(m_env_probe);
}

void EnvMapProbeControl::OnUpdate(double dt)
{
    if (m_env_probe->GetBounds().Empty()) {
        m_env_probe->SetBounds(parent->GetAABB());
    }

    m_env_probe->SetOrigin(parent->GetGlobalTranslation());
    m_env_probe->Update(dt);
}

std::shared_ptr<EntityControl> EnvMapProbeControl::CloneImpl()
{
    return std::make_shared<EnvMapProbeControl>(m_env_probe->GetOrigin(), m_env_probe->GetBounds());
}
} // namespace hyperion