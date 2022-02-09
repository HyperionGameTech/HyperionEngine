#include "gi_probe_control.h"
#include "gi_mapper.h"
#include "../probe_manager.h"
#include "../../../entity.h"

#include <memory>

namespace hyperion {
GIProbeControl::GIProbeControl(const Vector3 &origin, const BoundingBox &bounds)
    : EntityControl(fbom::FBOMObjectType("GI_PROBE_CONTROL"), 10.0),
      m_origin(origin),
      m_bounds(bounds),
      m_gi_mapper_node(new Entity("GI Mapper Node")),
      m_gi_mapper(std::make_shared<GIMapper>(origin, bounds))
{
    m_gi_mapper_node->SetRenderable(m_gi_mapper);
}

void GIProbeControl::OnAdded()
{
    // Add node to parent, with renderable as GIMapper
    parent->AddChild(m_gi_mapper_node);
    ProbeManager::GetInstance()->AddProbe(m_gi_mapper);
}

void GIProbeControl::OnRemoved()
{
    parent->RemoveChild(m_gi_mapper_node);
    ProbeManager::GetInstance()->RemoveProbe(m_gi_mapper);
}

void GIProbeControl::OnUpdate(double dt)
{
    m_gi_mapper->SetOrigin(parent->GetGlobalTranslation());
    m_gi_mapper->Update(dt);
}

std::shared_ptr<EntityControl> GIProbeControl::CloneImpl()
{
    return std::make_shared<GIProbeControl>(m_origin, m_bounds);
}
} // namespace hyperion