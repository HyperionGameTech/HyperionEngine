/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/EntityTag.hpp>
#include <Scene/ComponentInterface.hpp>

#include <EntityTag.generated.inl>

namespace Hyperion {

HYP_REGISTER_ENTITY_TAG(None, false, false);

HYP_REGISTER_ENTITY_TAG(MobStatic, true, false);
HYP_REGISTER_ENTITY_TAG(MobDynamic, true, false);
HYP_REGISTER_ENTITY_TAG(Light, true, false);
HYP_REGISTER_ENTITY_TAG(PrimaryCamera, true, true);
HYP_REGISTER_ENTITY_TAG(EditorCamera, true, false);
HYP_REGISTER_ENTITY_TAG(LightmapElement, true, false);
HYP_REGISTER_ENTITY_TAG(Replicated, true, true);

HYP_REGISTER_ENTITY_TAG(ReceivesUpdate, false, true);

HYP_REGISTER_ENTITY_TAG(FocusedInEditor, false);

HYP_REGISTER_ENTITY_TAG(UIVisible, false);

HYP_REGISTER_ENTITY_TAG(UpdateRenderProxy, false);
HYP_REGISTER_ENTITY_TAG(UpdateVisibility, false);
HYP_REGISTER_ENTITY_TAG(UpdateInstancedMeshData, false);
HYP_REGISTER_ENTITY_TAG(UpdateReplication, false);

HYP_REGISTER_ENTITY_TAG(UpdatePhysicsShape, false);
HYP_REGISTER_ENTITY_TAG(UpdatePhysicsMaterial, false);

HYP_REGISTER_ENTITY_TAG(EntityTypeSentinel, false);

} // namespace Hyperion
