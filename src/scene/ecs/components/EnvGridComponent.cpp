#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(EnvGridComponent)
    HYP_FIELD(env_grid_type),
    HYP_FIELD(grid_size),
    HYP_FIELD(render_component),
    HYP_FIELD(transform_hash_code)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(EnvGridComponent);

} // namespace hyperion