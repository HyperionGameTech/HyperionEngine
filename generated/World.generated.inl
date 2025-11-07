#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region World Reflection Data

HYP_BEGIN_CLASS(World, 147, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetName)), &World::GetName),
    Method(NAME(HYP_STR(SetName)), &World::SetName),
    Method(NAME(HYP_STR(GetRenderStats)), &World::GetRenderStats),
    Method(NAME(HYP_STR(GetSubsystemByName)), &World::GetSubsystemByName),
    Method(NAME(HYP_STR(RemoveSubsystem)), &World::RemoveSubsystem),
    Method(NAME(HYP_STR(GetWorldGrid)), &World::GetWorldGrid),
    Method(NAME(HYP_STR(GetGameState)), &World::GetGameState),
    Method(NAME(HYP_STR(StartSimulating)), &World::StartSimulating),
    Method(NAME(HYP_STR(StopSimulating)), &World::StopSimulating),
    Method(NAME(HYP_STR(AddScene)), &World::AddScene),
    Method(NAME(HYP_STR(RemoveScene)), &World::RemoveScene),
    Method(NAME(HYP_STR(HasScene)), &World::HasScene),
    Method(NAME(HYP_STR(GetSceneByName)), &World::GetSceneByName),
    Method(NAME(HYP_STR(AddView)), &World::AddView),
    Method(NAME(HYP_STR(RemoveView)), &World::RemoveView),
    Field(NAME(HYP_STR(Name)), &World::m_name, offsetof(World, m_name), Span<const ClassAttribute> { {ClassAttribute("property", "Name"), ClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion World Reflection Data

} // namespace hyperion

