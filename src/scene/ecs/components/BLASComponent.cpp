#include <scene/ecs/components/BLASComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(BLASComponent)
    HYP_FIELD(blas),
    HYP_FIELD(transform_hash_code)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(BLASComponent);

} // namespace hyperion