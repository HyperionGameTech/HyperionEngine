/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/EntityTag.hpp>
#include <scene/ComponentInterface.hpp>

#include <EntityTag.generated.inl>

namespace Hyperion {

HYP_REGISTER_ENTITY_TAG(None, false);

HYP_REGISTER_ENTITY_TAG(MobStatic, true);
HYP_REGISTER_ENTITY_TAG(MobDynamic, true);
HYP_REGISTER_ENTITY_TAG(Light, true);
HYP_REGISTER_ENTITY_TAG(PrimaryCamera, true);
HYP_REGISTER_ENTITY_TAG(LightmapElement, true);

HYP_REGISTER_ENTITY_TAG(ReceivesUpdate, false);

HYP_REGISTER_ENTITY_TAG(FocusedInEditor, false);

HYP_REGISTER_ENTITY_TAG(UIVisible, false);

HYP_REGISTER_ENTITY_TAG(UpdateRenderProxy, false);
HYP_REGISTER_ENTITY_TAG(UpdateVisibility, false);

HYP_REGISTER_ENTITY_TAG(EntityType, false);

} // namespace Hyperion
