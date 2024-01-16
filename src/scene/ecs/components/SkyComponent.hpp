#ifndef HYPERION_V2_ECS_SKY_COMPONENT_HPP
#define HYPERION_V2_ECS_SKY_COMPONENT_HPP

#include <core/Handle.hpp>

namespace hyperion::v2 {

class SkydomeRenderer;

struct SkyComponent
{
    RC<SkydomeRenderer> render_component;
};

} // namespace hyperion::v2

#endif