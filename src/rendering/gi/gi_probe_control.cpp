#include "gi_probe_control.h"
#include "gi_mapper.h"
#include <memory>
#include "gi_manager.h"
#include "../../entity.h"

// TODO: NOTE:
// Currently everything for the voxel cone tracing algo is just rendered from
// the main camera perspective (just one pass)
// so this creates some jank as you're looking around and the global illumination changes
// we'll have to use some kind of cubemap setup, although that will be hard on the VRAM and GPU

namespace hyperion {
GIProbeControl::GIProbeControl(const BoundingBox &bounds)
    : EntityControl(10.0),
      m_gi_mapper_node(new Entity("GI Mapper Node")),
      m_gi_mapper(std::make_shared<GIMapper>(bounds))
{
    m_gi_mapper_node->SetRenderable(m_gi_mapper);
}

void GIProbeControl::OnAdded()
{
    // Add node to parent, with renderable as GIMapper
    parent->AddChild(m_gi_mapper_node);
    GIManager::GetInstance()->AddProbe(m_gi_mapper);
}

void GIProbeControl::OnRemoved()
{
    parent->RemoveChild(m_gi_mapper_node);
    GIManager::GetInstance()->RemoveProbe(m_gi_mapper);
}

void GIProbeControl::OnUpdate(double dt)
{
    m_gi_mapper->SetOrigin(parent->GetGlobalTranslation());
    m_gi_mapper->UpdateRenderTick(dt);
}
} // namespace hyperion