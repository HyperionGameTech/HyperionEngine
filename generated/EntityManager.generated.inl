#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EntityManager Reflection Data

HYP_BEGIN_CLASS(EntityManager, 130, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetWorld)), &EntityManager::GetWorld),
    Method(NAME(HYP_STR(GetScene)), &EntityManager::GetScene),
    Method(NAME(HYP_STR(AddExistingEntity)), &EntityManager::AddExistingEntity),
    Method(NAME(HYP_STR(AddTypedEntity)), &EntityManager::AddTypedEntity),
    Method(NAME(HYP_STR(AddSystem)), &EntityManager::AddSystem),
    Method(NAME(HYP_STR(GetSystemByTypeId)), &EntityManager::GetSystemByTypeId),
    Method(NAME(HYP_STR(AddBasicEntity)), &EntityManager::AddBasicEntity)
HYP_END_CLASS

#pragma endregion EntityManager Reflection Data

} // namespace hyperion

