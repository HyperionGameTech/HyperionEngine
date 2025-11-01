#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EntityManager Reflection Data

HYP_BEGIN_CLASS(EntityManager, 130, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetWorld)), &EntityManager::GetWorld),
    HypMethod(NAME(HYP_STR(GetScene)), &EntityManager::GetScene),
    HypMethod(NAME(HYP_STR(AddExistingEntity)), &EntityManager::AddExistingEntity),
    HypMethod(NAME(HYP_STR(AddTypedEntity)), &EntityManager::AddTypedEntity),
    HypMethod(NAME(HYP_STR(AddSystem)), &EntityManager::AddSystem),
    HypMethod(NAME(HYP_STR(GetSystemByTypeId)), &EntityManager::GetSystemByTypeId),
    HypMethod(NAME(HYP_STR(AddBasicEntity)), &EntityManager::AddBasicEntity)
HYP_END_CLASS

#pragma endregion EntityManager Reflection Data

} // namespace hyperion

