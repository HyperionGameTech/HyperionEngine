/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityTag.hpp>
#include <scene/ecs/ComponentInterface.hpp>

namespace hyperion {

HYP_REGISTER_ENTITY_TAG(NONE);
HYP_REGISTER_ENTITY_TAG(STATIC);
HYP_REGISTER_ENTITY_TAG(DYNAMIC);
HYP_REGISTER_ENTITY_TAG(LIGHT);
HYP_REGISTER_ENTITY_TAG(UI);

} // namespace hyperion