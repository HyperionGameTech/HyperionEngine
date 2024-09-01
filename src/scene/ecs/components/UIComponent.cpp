#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(UIComponent)
    HYP_FIELD(ui_object)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(UIComponent);

} // namespace hyperion