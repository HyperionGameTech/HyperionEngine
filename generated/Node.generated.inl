#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region NodeFlags Reflection Data

HYP_BEGIN_ENUM(NodeFlags, 362, 0, {}, ClassAttribute("flags", true))
    StaticField(NAME(HYP_STR(NONE)), NodeFlags::NONE),
    StaticField(NAME(HYP_STR(IGNORE_PARENT_TRANSLATION)), NodeFlags::IGNORE_PARENT_TRANSLATION),
    StaticField(NAME(HYP_STR(IGNORE_PARENT_SCALE)), NodeFlags::IGNORE_PARENT_SCALE),
    StaticField(NAME(HYP_STR(IGNORE_PARENT_ROTATION)), NodeFlags::IGNORE_PARENT_ROTATION),
    StaticField(NAME(HYP_STR(IGNORE_PARENT_TRANSFORM)), NodeFlags::IGNORE_PARENT_TRANSFORM),
    StaticField(NAME(HYP_STR(EXCLUDE_FROM_PARENT_AABB)), NodeFlags::EXCLUDE_FROM_PARENT_AABB),
    StaticField(NAME(HYP_STR(TRANSIENT)), NodeFlags::TRANSIENT),
    StaticField(NAME(HYP_STR(HIDE_IN_SCENE_OUTLINE)), NodeFlags::HIDE_IN_SCENE_OUTLINE)
HYP_END_ENUM

#pragma endregion NodeFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region NodeTagSet Reflection Data

HYP_BEGIN_STRUCT(NodeTagSet, 363, 0, {})
HYP_END_STRUCT

#pragma endregion NodeTagSet Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region NodeTag Reflection Data

HYP_BEGIN_STRUCT(NodeTag, 364, 0, {})
    Field(NAME(HYP_STR(Name)), &NodeTag::name, offsetof(NodeTag, name), Span<const ClassAttribute> { {ClassAttribute("property", "Name"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Data)), &NodeTag::data, offsetof(NodeTag, data), Span<const ClassAttribute> { {ClassAttribute("property", "Data"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion NodeTag Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Node Reflection Data

HYP_BEGIN_CLASS(Node, 131, 14, NAME("HypObjectBase"), ClassAttribute("postload", "Node_OnPostLoad"))
    Method(NAME(HYP_STR(GetUUID)), &Node::GetUUID),
    Method(NAME(HYP_STR(GetName)), &Node::GetName),
    Method(NAME(HYP_STR(SetName)), &Node::SetName),
    Method(NAME(HYP_STR(HasName)), &Node::HasName),
    Method(NAME(HYP_STR(GetNodeFlags)), &Node::GetNodeFlags, Span<const ClassAttribute> { {ClassAttribute("property", "NodeFlags"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(SetNodeFlags)), &Node::SetNodeFlags, Span<const ClassAttribute> { {ClassAttribute("property", "NodeFlags") } }),
    Method(NAME(HYP_STR(GetParent)), &Node::GetParent, Span<const ClassAttribute> { {ClassAttribute("property", "Parent"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(IsOrHasParent)), &Node::IsOrHasParent),
    Method(NAME(HYP_STR(FindParentWithName)), &Node::FindParentWithName),
    Method(NAME(HYP_STR(IsRoot)), &Node::IsRoot),
    Method(NAME(HYP_STR(GetScene)), &Node::GetScene, Span<const ClassAttribute> { {ClassAttribute("property", "Scene"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetScene)), &Node::SetScene, Span<const ClassAttribute> { {ClassAttribute("property", "Scene"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetWorld)), &Node::GetWorld, Span<const ClassAttribute> { {ClassAttribute("property", "World"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetEntityAABB)), &Node::GetEntityAABB, Span<const ClassAttribute> { {ClassAttribute("property", "Aabb"), ClassAttribute("editor", true), ClassAttribute("label", "Bounding Box"), ClassAttribute("description", "The underlying axis-aligned bounding box for this node, not considering child nodes or transform") } }),
    Method(NAME(HYP_STR(SetEntityAABB)), &Node::SetEntityAABB, Span<const ClassAttribute> { {ClassAttribute("property", "Aabb"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(AddChild)), &Node::AddChild),
    Method(NAME(HYP_STR(RemoveAt)), &Node::RemoveAt),
    Method(NAME(HYP_STR(Remove)), &Node::Remove),
    Method(NAME(HYP_STR(RemoveAllChildren)), &Node::RemoveAllChildren),
    Method(NAME(HYP_STR(GetChild)), &Node::GetChild),
    Method(NAME(HYP_STR(Select)), &Node::Select),
    Method(NAME(HYP_STR(SetLocalTransform)), &Node::SetLocalTransform, Span<const ClassAttribute> { {ClassAttribute("property", "LocalTransform"), ClassAttribute("editor", true), ClassAttribute("label", "Local-space Transform"), ClassAttribute("editorpropertypanelclass", "TransformEditorPropertyPanel") } }),
    Method(NAME(HYP_STR(GetLocalTransform)), &Node::GetLocalTransform, Span<const ClassAttribute> { {ClassAttribute("property", "LocalTransform"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetLocalTranslation)), &Node::GetLocalTranslation, Span<const ClassAttribute> { {ClassAttribute("property", "LocalTranslation"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetLocalTranslation)), &Node::SetLocalTranslation, Span<const ClassAttribute> { {ClassAttribute("property", "LocalTranslation"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(Translate)), &Node::Translate),
    Method(NAME(HYP_STR(GetLocalScale)), &Node::GetLocalScale, Span<const ClassAttribute> { {ClassAttribute("property", "LocalScale"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetLocalScale)), &Node::SetLocalScale, Span<const ClassAttribute> { {ClassAttribute("property", "LocalScale"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(Scale)), &Node::Scale),
    Method(NAME(HYP_STR(GetLocalRotation)), &Node::GetLocalRotation, Span<const ClassAttribute> { {ClassAttribute("property", "LocalRotation"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetLocalRotation)), &Node::SetLocalRotation, Span<const ClassAttribute> { {ClassAttribute("property", "LocalRotation"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(Rotate)), &Node::Rotate),
    Method(NAME(HYP_STR(GetWorldTransform)), &Node::GetWorldTransform, Span<const ClassAttribute> { {ClassAttribute("property", "WorldTransform"), ClassAttribute("transient", true), ClassAttribute("editor", true), ClassAttribute("label", "World-space Transform"), ClassAttribute("editorpropertypanelclass", "TransformEditorPropertyPanel") } }),
    Method(NAME(HYP_STR(SetWorldTransform)), &Node::SetWorldTransform, Span<const ClassAttribute> { {ClassAttribute("property", "WorldTransform"), ClassAttribute("transient", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetWorldTranslation)), &Node::GetWorldTranslation, Span<const ClassAttribute> { {ClassAttribute("property", "WorldTranslation"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetWorldTranslation)), &Node::SetWorldTranslation, Span<const ClassAttribute> { {ClassAttribute("property", "WorldTranslation"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetWorldScale)), &Node::GetWorldScale, Span<const ClassAttribute> { {ClassAttribute("property", "WorldScale"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetWorldScale)), &Node::SetWorldScale, Span<const ClassAttribute> { {ClassAttribute("property", "WorldScale"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetWorldRotation)), &Node::GetWorldRotation, Span<const ClassAttribute> { {ClassAttribute("property", "WorldRotation"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetWorldRotation)), &Node::SetWorldRotation, Span<const ClassAttribute> { {ClassAttribute("property", "WorldRotation"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetRelativeTransform)), &Node::GetRelativeTransform),
    Method(NAME(HYP_STR(IsTransformLocked)), &Node::IsTransformLocked),
    Method(NAME(HYP_STR(GetLocalAABBExcludingSelf)), &Node::GetLocalAABBExcludingSelf),
    Method(NAME(HYP_STR(GetLocalAABB)), &Node::GetLocalAABB, Span<const ClassAttribute> { {ClassAttribute("property", "LocalAabb"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetWorldAABB)), &Node::GetWorldAABB, Span<const ClassAttribute> { {ClassAttribute("property", "WorldAabb"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(UpdateWorldTransform)), &Node::UpdateWorldTransform),
    Method(NAME(HYP_STR(CalculateDepth)), &Node::CalculateDepth),
    Method(NAME(HYP_STR(FindSelfIndex)), &Node::FindSelfIndex),
    Method(NAME(HYP_STR(FindChildByName)), &Node::FindChildByName),
    Method(NAME(HYP_STR(FindChildByUUID)), &Node::FindChildByUUID),
    Field(NAME(HYP_STR(Name)), &Node::m_name, offsetof(Node, m_name), Span<const ClassAttribute> { {ClassAttribute("property", "Name"), ClassAttribute("editor", true), ClassAttribute("label", "Name"), ClassAttribute("description", "The name of the node.") } }),
    Field(NAME(HYP_STR(ParentNode)), &Node::m_parentNode, offsetof(Node, m_parentNode), Span<const ClassAttribute> { {ClassAttribute("property", "Parent"), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(ChildNodes)), &Node::m_childNodes, offsetof(Node, m_childNodes), Span<const ClassAttribute> { {ClassAttribute("property", "Children"), ClassAttribute("loadorder", -1) } }),
    Field(NAME(HYP_STR(LocalTransform)), &Node::m_localTransform, offsetof(Node, m_localTransform), Span<const ClassAttribute> { {ClassAttribute("property", "LocalTransform") } }),
    Field(NAME(HYP_STR(WorldTransform)), &Node::m_worldTransform, offsetof(Node, m_worldTransform), Span<const ClassAttribute> { {ClassAttribute("property", "LocalTransform") } }),
    Field(NAME(HYP_STR(EntityAabb)), &Node::m_entityAabb, offsetof(Node, m_entityAabb), Span<const ClassAttribute> { {ClassAttribute("property", "Aabb") } }),
    Field(NAME(HYP_STR(Scene)), &Node::m_scene, offsetof(Node, m_scene), Span<const ClassAttribute> { {ClassAttribute("property", "Scene"), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(Tags)), &Node::m_tags, offsetof(Node, m_tags), Span<const ClassAttribute> { {ClassAttribute("property", "NodeTags") } }),
    Field(NAME(HYP_STR(Uuid)), &Node::m_uuid, offsetof(Node, m_uuid), Span<const ClassAttribute> { {ClassAttribute("property", "Uuid") } })
HYP_END_CLASS

#pragma endregion Node Reflection Data

static const ClassCallbackRegistration<ClassCallbackType::ON_POST_LOAD> g_post_load_Node(TypeId::ForType<Node>(), ValueWrapper<Node_OnPostLoad>());
} // namespace hyperion

