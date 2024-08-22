#include <scene/ecs/components/RigidBodyComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(RigidBodyComponent)
    HYP_FIELD(rigid_body),
    HYP_FIELD(physics_material),
    HYP_FIELD(flags),
    HYP_FIELD(transform_hash_code)
HYP_END_STRUCT

} // namespace hyperion