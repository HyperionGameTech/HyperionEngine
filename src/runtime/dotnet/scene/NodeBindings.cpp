#include <runtime/dotnet/ManagedHandle.hpp>
#include <runtime/dotnet/scene/ManagedNode.hpp>
#include <runtime/dotnet/math/ManagedMathTypes.hpp>

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

        auto it = node->FindChild(name);

        if (it == node->GetChildren().End()) {
            return ManagedNode { nullptr };
        }

        return CreateManagedNodeFromNodeProxy(*it);
    }

    Transform Node_GetWorldTransform(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return Transform();
        }

        return node->GetWorldTransform();
    }

    void Node_SetWorldTransform(ManagedNode managed_node, Transform transform)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return;
        }

        node->SetWorldTransform(transform);
    }

    Transform Node_GetLocalTransform(ManagedNode managed_node)
    {
        Node *node = managed_node.GetNode();

        if (node == nullptr) {
            return Transform();
        }

        return node->GetLocalTransform();
    }

    void Node_SetLocalTransform(ManagedNode managed_node, Transform transform)
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

        DebugLog(LogType::Debug, "Setting local translation to: %f, %f, %f\n", translation.x, translation.y, translation.z);

        if (node == nullptr) {
            return;
        }

        node->SetLocalTranslation(translation);
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
}