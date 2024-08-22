#include <scene/ecs/components/ScriptComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(ScriptComponent)
    HYP_FIELD(script),
    HYP_FIELD(assembly),
    HYP_FIELD(object),
    HYP_FIELD(flags)
HYP_END_STRUCT

} // namespace hyperion