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

#define HYP_MAKE_ENTITY_TAG_DEF(X, Value, ...) HYP_REGISTER_ENTITY_TAG(X, __VA_ARGS__);
HYP_FOR_EACH_ENTITY_TAG(HYP_MAKE_ENTITY_TAG_DEF);
#undef HYP_MAKE_ENTITY_TAG_DEF

// We need to register the sentinel tag as well, so that it can be used in the EntityManager.
HYP_REGISTER_ENTITY_TAG(EntityTypeSentinel, false, false);

} // namespace Hyperion
