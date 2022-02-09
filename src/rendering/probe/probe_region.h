#ifndef PROBE_REGION_H
#define PROBE_REGION_H

#include "../../math/bounding_box.h"

namespace hyperion {
struct ProbeRegion {
    Vector3 origin;
    BoundingBox bounds;
    Vector3 direction;
    Vector3 up_vector;
    int index;
};
} // namespace apex

#endif