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
HYP_EXPORT ManagedNode Node_Create()
{
    return CreateManagedNodeFromNodeProxy(NodeProxy(new Node()));
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

HYP_EXPORT ManagedNode Node_AddChild(ManagedNode parent, ManagedNode child)
{
    NodeProxy parent_node_proxy = CreateNodeProxyFromManagedNode(parent);

    if (!parent_node_proxy) {
        return ManagedNode { nullptr };
    }

    NodeProxy child_node = CreateNodeProxyFromManagedNode(child);

    if (!child_node) {
        return ManagedNode { nullptr };
    }

    parent_node_proxy.AddChild(child_node);

    return CreateManagedNodeFromNodeProxy(std::move(child_node));
}

HYP_EXPORT ManagedNode Node_FindChild(ManagedNode managed_node, const char *name)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return ManagedNode();
    }

    NodeProxy child_node = node->FindChildByName(name);

    if (!child_node) {
        return ManagedNode { nullptr };
    }

    return CreateManagedNodeFromNodeProxy(std::move(child_node));
}

HYP_EXPORT ManagedNode Node_FindChildWithEntity(ManagedNode managed_node, ManagedEntity entity)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return ManagedNode();
    }

    NodeProxy child_node = node->FindChildWithEntity(entity);

    if (!child_node) {
        return ManagedNode { nullptr };
    }

    return CreateManagedNodeFromNodeProxy(std::move(child_node));
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

HYP_EXPORT ManagedTransform Node_GetWorldTransform(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetWorldTransform();
}

HYP_EXPORT void Node_SetWorldTransform(ManagedNode managed_node, ManagedTransform transform)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldTransform(transform);
}

HYP_EXPORT ManagedTransform Node_GetLocalTransform(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetLocalTransform();
}

HYP_EXPORT void Node_SetLocalTransform(ManagedNode managed_node, ManagedTransform transform)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetLocalTransform(transform);
}

HYP_EXPORT ManagedVec3f Node_GetWorldTranslation(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetWorldTranslation();
}

HYP_EXPORT void Node_SetWorldTranslation(ManagedNode managed_node, ManagedVec3f translation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldTranslation(translation);
}

HYP_EXPORT ManagedVec3f Node_GetLocalTranslation(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetLocalTranslation();
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

HYP_EXPORT ManagedQuaternion Node_GetWorldRotation(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetWorldRotation();
}

HYP_EXPORT void Node_SetWorldRotation(ManagedNode managed_node, ManagedQuaternion rotation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldRotation(rotation);
}

HYP_EXPORT ManagedQuaternion Node_GetLocalRotation(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetLocalRotation();
}

HYP_EXPORT void Node_SetLocalRotation(ManagedNode managed_node, ManagedQuaternion rotation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetLocalRotation(rotation);
}

HYP_EXPORT void Node_Rotate(ManagedNode managed_node, ManagedQuaternion rotation)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->Rotate(rotation);
}

HYP_EXPORT ManagedVec3f Node_GetWorldScale(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetWorldScale();
}

HYP_EXPORT void Node_SetWorldScale(ManagedNode managed_node, ManagedVec3f scale)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldScale(scale);
}

HYP_EXPORT ManagedVec3f Node_GetLocalScale(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetLocalScale();
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

HYP_EXPORT ManagedBoundingBox Node_GetWorldAABB(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetWorldAABB();
}

HYP_EXPORT void Node_SetWorldAABB(ManagedNode managed_node, ManagedBoundingBox aabb)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return;
    }

    node->SetWorldAABB(aabb);
}

HYP_EXPORT ManagedBoundingBox Node_GetLocalAABB(ManagedNode managed_node)
{
    Node *node = managed_node.GetNode();

    if (node == nullptr) {
        return { };
    }

    return node->GetLocalAABB();
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