/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/NodeProxy.hpp>
#include <scene/Node.hpp>

namespace hyperion {

#pragma region NodeProxyChildren

SizeType NodeProxyChildren::Size() const
{
    return node ? node->GetChildren().Size() : 0;
}

NodeProxyChildren::Iterator NodeProxyChildren::Begin()
{
    return Iterator { node, 0 };
}

NodeProxyChildren::ConstIterator NodeProxyChildren::Begin() const
{
    return ConstIterator { node, 0 };
}

NodeProxyChildren::Iterator NodeProxyChildren::End()
{
    return Iterator { const_cast<Node *>(node), node ? node->GetChildren().Size() : 0 };
}

NodeProxyChildren::ConstIterator NodeProxyChildren::End() const
{
    return ConstIterator { node, node ? node->GetChildren().Size() : 0 };
}

const NodeProxy &NodeProxyChildren::Iterator::operator*() const
{
    AssertThrow(node != nullptr && index < node->GetChildren().Size());

    return const_cast<Node *>(node)->GetChildren()[index];
}

const NodeProxy &NodeProxyChildren::ConstIterator::operator*() const
{
    AssertThrow(node != nullptr && index < node->GetChildren().Size());

    return node->GetChildren()[index];
}

const NodeProxy *NodeProxyChildren::Iterator::operator->() const
{
    return &const_cast<Node *>(node)->GetChildren()[index];
}

const NodeProxy *NodeProxyChildren::ConstIterator::operator->() const
{
    return &node->GetChildren()[index];
}

#pragma endregion NodeProxyChildren

#pragma region NodeProxy

const NodeProxy NodeProxy::empty = NodeProxy();

NodeProxy::NodeProxy()
    : Base()
{
}

NodeProxy::NodeProxy(const RC<Node> &node)
    : Base(node)
{
}

NodeProxy::NodeProxy(const NodeProxy &other)
    : Base(other)
{
}

NodeProxy &NodeProxy::operator=(const NodeProxy &other)
{
    Base::operator=(other);

    return *this;
}

NodeProxy::NodeProxy(NodeProxy &&other) noexcept
    : Base(std::move(other))
{
}

NodeProxy &NodeProxy::operator=(NodeProxy &&other) noexcept
{
    Base::operator=(std::move(other));

    return *this;
}

NodeProxy::~NodeProxy() = default;

const String &NodeProxy::GetName() const
{
    return Get()
        ? Get()->GetName()
        : String::empty;
}

void NodeProxy::SetName(const String &name)
{
    if (Node *node = Get()) {
        node->SetName(name);
    }
}

const Handle<Entity> &NodeProxy::GetEntity() const
{
    return Get()
        ? Get()->GetEntity()
        : Handle<Entity>::empty;
}

void NodeProxy::SetEntity(const Handle<Entity> &entity)
{
    if (Node *node = Get()) {
        node->SetEntity(entity);
    }
}

Node *NodeProxy::GetParent() const
{
    if (Node *node = Get()) {
        return node->GetParent();
    }

    return nullptr;
}

bool NodeProxy::IsOrHasParent(const Node *other) const
{
    if (Node *node = Get()) {
        return node->IsOrHasParent(other);
    }

    return false;
}

NodeProxy NodeProxy::GetChild(SizeType index)
{
    if (Node *node = Get()) {
        if (node && index < node->GetChildren().Size()) {
            return node->GetChild(index);
        }
    }
    
    return empty;
}

NodeProxy NodeProxy::Select(const char *selector) const
{
    if (Node *node = Get()) {
        return node->Select(selector);
    }
    
    return empty;
}

NodeProxy NodeProxy::AddChild()
{
    if (Node *node = Get()) {
        return node->AddChild();
    }

    return empty;
}

NodeProxy NodeProxy::AddChild(const NodeProxy &node)
{
    if (!Get()) {
        return node;
    }

    AssertThrowMsg(!Get()->IsOrHasParent(node.Get()), "Circular node structure detected");

    return Get()->AddChild(node);
}

void NodeProxy::Remove()
{
    if (Node *node = Get()) {
        node->Remove();
    }
}

const Transform &NodeProxy::GetLocalTransform() const
{
    if (Get()) {
        return Get()->GetLocalTransform();
    }

    return Transform::identity;
}

void NodeProxy::SetLocalTransform(const Transform &transform)
{
    if (Node *node = Get()) {
        node->SetLocalTransform(transform);
    }
}

const Vec3f &NodeProxy::GetLocalTranslation() const
    { return GetLocalTransform().GetTranslation(); }

void NodeProxy::SetLocalTranslation(const Vec3f &translation)
{
    if (Node *node = Get()) {
        node->SetLocalTranslation(translation);
    }
}

const Vec3f &NodeProxy::GetLocalScale() const
    { return GetLocalTransform().GetScale(); }

void NodeProxy::SetLocalScale(const Vec3f &scale)
{
    if (Node *node = Get()) {
        node->SetLocalScale(scale);
    }
}

const Quaternion &NodeProxy::GetLocalRotation() const
    { return GetLocalTransform().GetRotation(); }

void NodeProxy::SetLocalRotation(const Quaternion &rotation)
{
    if (Node *node = Get()) {
        node->SetLocalRotation(rotation);
    }
}

const Transform &NodeProxy::GetWorldTransform() const
{
    if (Node *node = Get()) {
        return node->GetWorldTransform();
    }

    return Transform::identity;
}

const Vec3f &NodeProxy::GetWorldTranslation() const
    { return GetWorldTransform().GetTranslation(); }

void NodeProxy::SetWorldTranslation(const Vec3f &translation)
{
    if (Node *node = Get()) {
        node->SetWorldTranslation(translation);
    }
}

const Vec3f &NodeProxy::GetWorldScale() const
    { return GetWorldTransform().GetScale(); }

void NodeProxy::SetWorldScale(const Vec3f &scale)
{
    if (Node *node = Get()) {
        node->SetWorldScale(scale);
    }
}

const Quaternion &NodeProxy::GetWorldRotation() const
    { return GetWorldTransform().GetRotation(); }

void NodeProxy::SetWorldRotation(const Quaternion &rotation)
{
    if (Node *node = Get()) {
        node->SetWorldRotation(rotation);
    }
}

BoundingBox NodeProxy::GetEntityAABB() const
{
    if (Node *node = Get()) {
        return node->GetEntityAABB();
    }

    return BoundingBox::Empty();
}

void NodeProxy::SetEntityAABB(const BoundingBox &aabb)
{
    if (Node *node = Get()) {
        return node->SetEntityAABB(aabb);
    }
}

BoundingBox NodeProxy::GetLocalAABB() const
{
    if (Node *node = Get()) {
        return node->GetLocalAABB();
    }

    return BoundingBox::Empty();
}

BoundingBox NodeProxy::GetWorldAABB() const
{
    if (Node *node = Get()) {
        return node->GetWorldAABB();
    }

    return BoundingBox::Empty();
}

bool NodeProxy::IsTransformLocked() const
{
    if (Node *node = Get()) {
        return node->IsTransformLocked();
    }

    return false;
}

void NodeProxy::LockTransform()
{
    if (Node *node = Get()) {
        node->LockTransform();
    }
}

void NodeProxy::UnlockTransform()
{
    if (Node *node = Get()) {
        node->UnlockTransform();
    }
}

uint32 NodeProxy::CalculateDepth() const
{
    if (Node *node = Get()) {
        return node->CalculateDepth();
    }

    return 0;
}

HashCode NodeProxy::GetHashCode() const
{
    HashCode hc;

    if (Node *node = Get()) {
        hc.Add(node->GetHashCode());
    }

    return hc;
}

#pragma endregion NodeProxy

} // namespace hyperion