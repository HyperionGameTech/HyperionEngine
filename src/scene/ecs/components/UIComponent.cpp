#include <scene/ecs/components/UIComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(UIComponent)
    HYP_FIELD(ui_object),

    HYP_PROPERTY(UIObject, &UIComponent::ui_object)
HYP_END_STRUCT

} // namespace hyperion