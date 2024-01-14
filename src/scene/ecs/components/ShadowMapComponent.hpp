#ifndef HYPERION_V2_ECS_SHADOW_MAP_COMPONENT_HPP
#define HYPERION_V2_ECS_SHADOW_MAP_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/Name.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <scene/camera/Camera.hpp>
#include <math/Matrix4.hpp>
#include <math/BoundingBox.hpp>
#include <HashCode.hpp>

namespace hyperion::v2 {

class ShadowMapRenderer;

enum ShadowMapFilter
{
    SHADOW_MAP_FILTER_NONE,
    SHADOW_MAP_FILTER_PCF,
    SHADOW_MAP_FILTER_CONTACT_HARDENED,
    SHADOW_MAP_FILTER_VSM
};

struct ShadowMapComponent
{
    ShadowMapFilter         filter = SHADOW_MAP_FILTER_PCF;
    Float                   radius = 15.0f;
    BoundingBox             aabb;
    RC<ShadowMapRenderer>   shadow_map_renderer;

    UInt                    update_counter = 0;
};

} // namespace hyperion::v2

#endif