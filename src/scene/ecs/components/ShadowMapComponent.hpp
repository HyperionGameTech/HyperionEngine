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

enum ShadowMapFilter : uint32
{
    SHADOW_MAP_FILTER_NONE,
    SHADOW_MAP_FILTER_PCF,
    SHADOW_MAP_FILTER_CONTACT_HARDENED,
    SHADOW_MAP_FILTER_VSM
};

struct ShadowMapComponent
{
    ShadowMapFilter         filter = SHADOW_MAP_FILTER_VSM;
    float                   radius = 20.0f;
    Extent2D                resolution = { 512, 512 };
    RC<RenderComponentBase> render_component;

    uint32                  update_counter = 0;
};

static_assert(sizeof(ShadowMapComponent) == 32, "ShadowMapComponent size mismatch with C#");

} // namespace hyperion::v2

#endif
