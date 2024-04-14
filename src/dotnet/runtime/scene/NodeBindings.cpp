#include <dotnet/runtime/ManagedHandle.hpp>
#include <dotnet/runtime/scene/ManagedNode.hpp>
#include <dotnet/runtime/scene/ManagedSceneTypes.hpp>
#include <dotnet/runtime/math/ManagedMathTypes.hpp>

#include <scene/NodeProxy.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT void Node_Create(ManagedNode *out_managed_node)
{
    *out_managed_node = CreateManagedNodeFromNodeProxy(NodeProxy(new Node()));
}

HYP_EXPORT const char *Node_GetName(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return "";
    }

    return node->GetName().Data();
}

HYP_EXPORT void Node_SetName(ManagedNode managed_node, const char *name)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetName(name);
}

HYP_EXPORT ManagedEntity Node_GetEntity(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetEntity();
}

HYP_EXPORT void Node_SetEntity(ManagedNode managed_node, ManagedEntity managed_entity)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetEntity(managed_entity);
}

HYP_EXPORT void Node_AddChild(ManagedNode parent, ManagedNode child, ManagedNode *out_child_managed_node)
{
    NodeProxy parent_node_proxy = CreateNodeProxyFromManagedNode(parent);

    if (!parent_node_proxy) {
        return;
    }

    NodeProxy child_node = CreateNodeProxyFromManagedNode(child);

    if (!child_node) {
        return;
    }

    parent_node_proxy.AddChild(child_node);

    *out_child_managed_node = CreateManagedNodeFromNodeProxy(std::move(child_node));
}

HYP_EXPORT void Node_FindChild(ManagedNode managed_node, const char *name, ManagedNode *out_result)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    NodeProxy child_node = node->FindChildByName(name);

    if (!child_node) {
        return;
    }

    *out_result = CreateManagedNodeFromNodeProxy(std::move(child_node));
}

HYP_EXPORT void Node_FindChildWithEntity(ManagedNode managed_node, ManagedEntity entity, ManagedNode *out_result)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    NodeProxy child_node = node->FindChildWithEntity(entity);

    if (!child_node) {
        return;
    }

    *out_result = CreateManagedNodeFromNodeProxy(std::move(child_node));
}

HYP_EXPORT bool Node_RemoveChild(ManagedNode managed_node, ManagedNode managed_child)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return false;
    }

    NodeProxy child = CreateNodeProxyFromManagedNode(managed_child);

    if (!child) {
        return false;
    }

    return node->RemoveChild(&child);
}

HYP_EXPORT void Node_GetWorldTransform(ManagedNode managed_node, Transform *out_transform)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_transform = node->GetWorldTransform();
}

HYP_EXPORT void Node_SetWorldTransform(ManagedNode managed_node, Transform transform)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldTransform(transform);
}

HYP_EXPORT void Node_GetLocalTransform(ManagedNode managed_node, Transform *out_transform)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_transform = node->GetLocalTransform();
}

HYP_EXPORT void Node_SetLocalTransform(ManagedNode managed_node, Transform transform)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetLocalTransform(transform);
}

HYP_EXPORT void Node_GetWorldTranslation(ManagedNode managed_node, Vec3f *out_translation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_translation = node->GetWorldTranslation();
}

HYP_EXPORT void Node_SetWorldTranslation(ManagedNode managed_node, ManagedVec3f translation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldTranslation(translation);
}

HYP_EXPORT void Node_GetLocalTranslation(ManagedNode managed_node, Vec3f *out_translation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_translation = node->GetLocalTranslation();
}

HYP_EXPORT void Node_SetLocalTranslation(ManagedNode managed_node, ManagedVec3f translation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetLocalTranslation(translation);
}

HYP_EXPORT void Node_Translate(ManagedNode managed_node, ManagedVec3f translation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->Translate(translation);
}

HYP_EXPORT void Node_GetWorldRotation(ManagedNode managed_node, Quaternion *out_rotation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_rotation = node->GetWorldRotation();
}

HYP_EXPORT void Node_SetWorldRotation(ManagedNode managed_node, Quaternion rotation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldRotation(rotation);
}

HYP_EXPORT void Node_GetLocalRotation(ManagedNode managed_node, Quaternion *out_rotation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_rotation = node->GetLocalRotation();
}

HYP_EXPORT void Node_SetLocalRotation(ManagedNode managed_node, Quaternion rotation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetLocalRotation(rotation);
}

HYP_EXPORT void Node_Rotate(ManagedNode managed_node, Quaternion rotation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->Rotate(rotation);
}

HYP_EXPORT void Node_GetWorldScale(ManagedNode managed_node, Vec3f *out_scale)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_scale = node->GetWorldScale();
}

HYP_EXPORT void Node_SetWorldScale(ManagedNode managed_node, ManagedVec3f scale)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldScale(scale);
}

HYP_EXPORT void Node_GetLocalScale(ManagedNode managed_node, Vec3f *out_scale)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_scale = node->GetLocalScale();
}

HYP_EXPORT void Node_SetLocalScale(ManagedNode managed_node, ManagedVec3f scale)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetLocalScale(scale);
}

HYP_EXPORT bool Node_IsTransformLocked(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return false;
    }

    return node->IsTransformLocked();
}

HYP_EXPORT void Node_LockTransform(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->LockTransform();
}

HYP_EXPORT void Node_UnlockTransform(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->UnlockTransform();
}

HYP_EXPORT void Node_Scale(ManagedNode managed_node, ManagedVec3f scale)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->Scale(scale);
}

HYP_EXPORT void Node_GetWorldAABB(ManagedNode managed_node, ManagedBoundingBox *out_aabb)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_aabb = node->GetWorldAABB();
}

HYP_EXPORT void Node_SetWorldAABB(ManagedNode managed_node, ManagedBoundingBox aabb)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldAABB(aabb);
}

HYP_EXPORT void Node_GetLocalAABB(ManagedNode managed_node, ManagedBoundingBox *out_aabb)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    *out_aabb = node->GetLocalAABB();
}

HYP_EXPORT void Node_SetLocalAABB(ManagedNode managed_node, ManagedBoundingBox aabb)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetLocalAABB(aabb);
}
} // extern "C"