#include "probe.h"

#include "../camera/perspective_camera.h"
#include "../../math/matrix_util.h"

namespace hyperion {
Probe::Probe(const fbom::FBOMType &loadable_type, const Vector3 &origin, const BoundingBox &bounds)
    : Renderable(fbom::FBOMObjectType("PROBE").Extend(loadable_type), RB_BUFFER),
      m_cameras({ nullptr }),
      m_origin(origin),
      m_bounds(bounds),
      m_directions({
        std::make_pair(Vector3(1, 0, 0), Vector3(0, -1, 0)),
        std::make_pair(Vector3(-1, 0, 0), Vector3(0, -1, 0)),
        std::make_pair(Vector3(0, 1, 0), Vector3(0, 0, 1)),
        std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, -1)),
        std::make_pair(Vector3(0, 0, 1), Vector3(0, -1, 0)),
        std::make_pair(Vector3(0, 0, -1), Vector3(0, -1, 0))
      })
{
}

Probe::~Probe()
{
}

void Probe::SetBounds(const BoundingBox &bounds)
{
    m_bounds = bounds;

    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        cam->GetRegion().bounds = bounds;
    }
}

void Probe::SetOrigin(const Vector3 &origin)
{
    m_origin = origin;

    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        cam->GetRegion().origin = origin;
    }
}
} // namespace hyperion
