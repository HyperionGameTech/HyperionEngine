#include <scene/ecs/components/TransformComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(TransformComponent)
    HYP_FIELD(transform),

    HYP_PROPERTY(Transform, &TransformComponent::transform)
HYP_END_STRUCT

} // namespace hyperion