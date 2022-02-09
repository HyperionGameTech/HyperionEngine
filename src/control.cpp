#include "control.h"

namespace hyperion {

EntityControl::EntityControl(const fbom::FBOMType &loadable_type, const double tps)
    : fbom::FBOMLoadable(fbom::FBOMObjectType("CONTROL").Extend(loadable_type)),
      tps(tps), 
      tick(0.0),
      m_first_run(true)
{
}

EntityControl::~EntityControl()
{
}

void EntityControl::OnFirstRun(double dt)
{
    // no-op
}

std::shared_ptr<Loadable> EntityControl::Clone()
{
    return CloneImpl();
}

} // namespace hyperion
