#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(NodeLinkComponent)
    HYP_FIELD(node)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(NodeLinkComponent);

} // namespace hyperion