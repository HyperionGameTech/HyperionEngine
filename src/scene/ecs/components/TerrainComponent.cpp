#include <scene/ecs/components/TerrainComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(TerrainComponent)
    HYP_FIELD(seed),
    HYP_FIELD(patch_size),
    HYP_FIELD(scale),
    HYP_FIELD(max_distance)
HYP_END_STRUCT

} // namespace hyperion