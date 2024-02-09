#ifndef HYPERION_V2_ECS_SHADOW_MAP_COMPONENT_HPP
#define HYPERION_V2_ECS_SHADOW_MAP_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/Name.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <scene/camera/Camera.hpp>
#include <math/Matrix4.hpp>
#include <math/Extent.hpp>
#include <math/BoundingBox.hpp>
#include <HashCode.hpp>

namespace hyperion::v2 {

class RenderComponentBase;

enum ShadowMapFilter
{
    SHADOW_MAP_FILTER_NONE,
    SHADOW_MAP_FILTER_PCF,
    SHADOW_MAP_FILTER_CONTACT_HARDENED,
    SHADOW_MAP_FILTER_VSM
};

struct ShadowMapComponent
{
    ShadowMapFilter         filter = SHADOW_MAP_FILTER_VSM;
    float                   radius = 60.0f;
    Extent2D                resolution = { 1024, 1024 };
    RC<RenderComponentBase> render_component;

    uint                    update_counter = 0;
};

} // namespace hyperion::v2

#endif