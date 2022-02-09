#include "probe_camera.h"

#include "../renderer.h"
#include "../texture_3D.h"
#include "../camera/perspective_camera.h"

namespace hyperion {
ProbeCamera::ProbeCamera(const fbom::FBOMType &loadable_type, const ProbeRegion &region)
    : Renderable(fbom::FBOMObjectType("PROBE_CAMERA").Extend(loadable_type), RB_BUFFER),
      m_region(region),
      m_camera(nullptr)
{
}

ProbeCamera::~ProbeCamera()
{
}
} // namespace hyperion