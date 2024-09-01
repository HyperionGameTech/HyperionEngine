#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/utilities/Variant.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(MeshComponent)
    HYP_FIELD(mesh),
    HYP_FIELD(material),
    HYP_FIELD(skeleton),
    HYP_FIELD(num_instances),
    HYP_FIELD(proxy),
    HYP_FIELD(flags),
    HYP_FIELD(previous_model_matrix),
    HYP_FIELD(user_data)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(MeshComponent);

} // namespace hyperion