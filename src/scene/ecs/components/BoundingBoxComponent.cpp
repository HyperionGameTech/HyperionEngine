#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(BoundingBoxComponent)
    HYP_FIELD(local_aabb),
    HYP_FIELD(world_aabb),
    HYP_FIELD(transform_hash_code)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(BoundingBoxComponent);

} // namespace hyperion