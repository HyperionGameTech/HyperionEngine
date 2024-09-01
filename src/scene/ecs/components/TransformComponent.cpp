#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(TransformComponent)
    HYP_FIELD(transform)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(TransformComponent);

} // namespace hyperion