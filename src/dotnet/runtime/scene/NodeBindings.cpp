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
    const char *Node_GetName(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return "";
        }

        return node->GetName().Data();
    }

    void Node_SetName(ManagedNode managed_node, const char *name)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetName(name);
    }

    ManagedEntity Node_GetEntity(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetEntity();
    }

    void Node_SetEntity(ManagedNode managed_node, ManagedEntity managed_entity)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetEntity(managed_entity);
    }

    ManagedNode Node_AddChild(ManagedNode managed_node)
    {
        NodeProxy parent_node_proxy = CreateNodeProxyFromManagedNode(managed_node);

        if (!parent_node_proxy) {
            return ManagedNode { nullptr };
        }

        NodeProxy child_node = parent_node_proxy.AddChild();

        return CreateManagedNodeFromNodeProxy(std::move(child_node));
    }

    ManagedNode Node_FindChild(ManagedNode managed_node, const char *name)
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

    ManagedNode Node_FindChildWithEntity(ManagedNode managed_node, ManagedEntity entity)
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

    bool Node_RemoveChild(ManagedNode managed_node, ManagedNode managed_child)
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

    ManagedTransform Node_GetWorldTransform(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetWorldTransform();
    }

    void Node_SetWorldTransform(ManagedNode managed_node, ManagedTransform transform)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetWorldTransform(transform);
    }

    ManagedTransform Node_GetLocalTransform(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetLocalTransform();
    }

    void Node_SetLocalTransform(ManagedNode managed_node, ManagedTransform transform)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetLocalTransform(transform);
    }

    ManagedVec3f Node_GetWorldTranslation(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetWorldTranslation();
    }

    void Node_SetWorldTranslation(ManagedNode managed_node, ManagedVec3f translation)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetWorldTranslation(translation);
    }

    ManagedVec3f Node_GetLocalTranslation(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetLocalTranslation();
    }

    void Node_SetLocalTranslation(ManagedNode managed_node, ManagedVec3f translation)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetLocalTranslation(translation);
    }

    void Node_Translate(ManagedNode managed_node, ManagedVec3f translation)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->Translate(translation);
    }

    ManagedQuaternion Node_GetWorldRotation(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetWorldRotation();
    }

    void Node_SetWorldRotation(ManagedNode managed_node, ManagedQuaternion rotation)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetWorldRotation(rotation);
    }

    ManagedQuaternion Node_GetLocalRotation(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetLocalRotation();
    }

    void Node_SetLocalRotation(ManagedNode managed_node, ManagedQuaternion rotation)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetLocalRotation(rotation);
    }

    void Node_Rotate(ManagedNode managed_node, ManagedQuaternion rotation)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->Rotate(rotation);
    }

    ManagedVec3f Node_GetWorldScale(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetWorldScale();
    }

    void Node_SetWorldScale(ManagedNode managed_node, ManagedVec3f scale)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetWorldScale(scale);
    }

    ManagedVec3f Node_GetLocalScale(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetLocalScale();
    }

    void Node_SetLocalScale(ManagedNode managed_node, ManagedVec3f scale)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetLocalScale(scale);
    }

    void Node_Scale(ManagedNode managed_node, ManagedVec3f scale)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->Scale(scale);
    }

    ManagedBoundingBox Node_GetWorldAABB(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetWorldAABB();
    }

    ManagedBoundingBox Node_GetLocalAABB(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return { };
        }

        return node->GetLocalAABB();
    }
}