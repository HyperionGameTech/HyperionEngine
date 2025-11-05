#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Scene Reflection Data

HYP_BEGIN_CLASS(Scene, 53, 0, NAME("AssetObject"), ClassAttribute("postload", "Scene_OnPostLoad"))
    Method(NAME(HYP_STR(GetPrimaryCamera)), &Scene::GetPrimaryCamera),
    Method(NAME(HYP_STR(GetSceneFlags)), &Scene::GetSceneFlags),
    Method(NAME(HYP_STR(SetSceneFlags)), &Scene::SetSceneFlags),
    Method(NAME(HYP_STR(FindNodeByName)), &Scene::FindNodeByName),
    Method(NAME(HYP_STR(GetRoot)), &Scene::GetRoot, Span<const ClassAttribute> { {ClassAttribute("property", "Root"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetRoot)), &Scene::SetRoot, Span<const ClassAttribute> { {ClassAttribute("property", "Root"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetEntityManager)), &Scene::GetEntityManager),
    Method(NAME(HYP_STR(IsAttachedToWorld)), &Scene::IsAttachedToWorld),
    Method(NAME(HYP_STR(GetWorld)), &Scene::GetWorld),
    Method(NAME(HYP_STR(SetWorld)), &Scene::SetWorld),
    Method(NAME(HYP_STR(IsForegroundScene)), &Scene::IsForegroundScene),
    Method(NAME(HYP_STR(IsBackgroundScene)), &Scene::IsBackgroundScene),
    Method(NAME(HYP_STR(IsAudioListener)), &Scene::IsAudioListener),
    Method(NAME(HYP_STR(SetIsAudioListener)), &Scene::SetIsAudioListener),
    Method(NAME(HYP_STR(AddToWorld)), &Scene::AddToWorld),
    Method(NAME(HYP_STR(RemoveFromWorld)), &Scene::RemoveFromWorld),
    Method(NAME(HYP_STR(GetUniqueNodeName)), &Scene::GetUniqueNodeName),
    Field(NAME(HYP_STR(SceneFlags)), &Scene::m_sceneFlags, offsetof(Scene, m_sceneFlags), Span<const ClassAttribute> { {ClassAttribute("property", "SceneFlags") } }),
    Field(NAME(HYP_STR(Root)), &Scene::m_root, offsetof(Scene, m_root), Span<const ClassAttribute> { {ClassAttribute("property", "Root") } }),
    Field(NAME(HYP_STR(OwnerThreadId)), &Scene::m_ownerThreadId, offsetof(Scene, m_ownerThreadId), Span<const ClassAttribute> { {ClassAttribute("property", "OwnerThreadId"), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(World)), &Scene::m_world, offsetof(Scene, m_world), Span<const ClassAttribute> { {ClassAttribute("property", "World"), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(EntityManager)), &Scene::m_entityManager, offsetof(Scene, m_entityManager), Span<const ClassAttribute> { {ClassAttribute("property", "EntityManager"), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(IsAudioListener)), &Scene::m_isAudioListener, offsetof(Scene, m_isAudioListener), Span<const ClassAttribute> { {ClassAttribute("property", "IsAudioListener") } }),
    Field(NAME(HYP_STR(PreviousDelta)), &Scene::m_previousDelta, offsetof(Scene, m_previousDelta), Span<const ClassAttribute> { {ClassAttribute("property", "PreviousDelta"), ClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion Scene Reflection Data

static const ClassCallbackRegistration<ClassCallbackType::ON_POST_LOAD> g_post_load_Scene(TypeId::ForType<Scene>(), ValueWrapper<Scene_OnPostLoad>());
} // namespace hyperion


namespace hyperion {

#pragma region SceneFlags Reflection Data

HYP_BEGIN_ENUM(SceneFlags, 384, 0, {})
    StaticField(NAME(HYP_STR(NONE)), SceneFlags::NONE),
    StaticField(NAME(HYP_STR(FOREGROUND)), SceneFlags::FOREGROUND),
    StaticField(NAME(HYP_STR(DETACHED)), SceneFlags::DETACHED),
    StaticField(NAME(HYP_STR(UI)), SceneFlags::UI),
    StaticField(NAME(HYP_STR(EDITOR)), SceneFlags::EDITOR)
HYP_END_ENUM

#pragma endregion SceneFlags Reflection Data

} // namespace hyperion

