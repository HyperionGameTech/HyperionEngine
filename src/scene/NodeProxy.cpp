#include "NodeProxy.hpp"
#include "Node.hpp"

namespace hyperion::v2 {

// NodeProxyChildren

uint NodeProxyChildren::Size() const
{
    return node ? uint(node->GetChildren().Size()) : 0;
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

NodeProxy &NodeProxyChildren::Iterator::operator*()
{
    AssertThrow(node != nullptr && index < node->GetChildren().Size());

    return const_cast<Node *>(node)->GetChildren()[index];
}

const NodeProxy &NodeProxyChildren::ConstIterator::operator*() const
{
    AssertThrow(node != nullptr && index < node->GetChildren().Size());

    return node->GetChildren()[index];
}

NodeProxy *NodeProxyChildren::Iterator::operator->()
{
    return &const_cast<Node *>(node)->GetChildren()[index];
}

const NodeProxy *NodeProxyChildren::ConstIterator::operator->() const
{
    return &node->GetChildren()[index];
}

// NodeProxy

const NodeProxy NodeProxy::empty = NodeProxy();

NodeProxy::NodeProxy()
    : Base()
{
}

NodeProxy::NodeProxy(Node *ptr)
    : Base(ptr)
{
}

NodeProxy::NodeProxy(const Base &other)
    : Base(other)
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
    if (auto *node = Get()) {
        node->SetName(name);
    }
}

ID<Entity> NodeProxy::GetEntity() const
{
    return Get()
        ? Get()->GetEntity()
        : ID<Entity>::invalid;
}

void NodeProxy::SetEntity(ID<Entity> entity)
{
    if (auto *node = Get()) {
        node->SetEntity(entity);
    }
}

Node *NodeProxy::GetParent() const
{
    if (auto *node = Get()) {
        return node->GetParent();
    }

    return nullptr;
}

bool NodeProxy::IsOrHasParent(const Node *node) const
{
    if (auto *this_node = Get()) {
        return this_node->IsOrHasParent(node);
    }

    return false;
}

NodeProxy NodeProxy::GetChild(SizeType index)
{
    if (Get() && index < Get()->GetChildren().Size()) {
        return Get()->GetChild(index);
    }
    
    return empty;
}

NodeProxy NodeProxy::Select(const char *selector) const
{
    if (Get()) {
        return Get()->Select(selector);
    }
    
    return empty;
}

NodeProxy NodeProxy::AddChild()
{
    if (Get()) {
        return Get()->AddChild();
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

bool NodeProxy::Remove()
{
    if (!Get()) {
        return false;
    }

    return Get()->Remove();
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
    if (Get()) {
        Get()->SetLocalTransform(transform);
    }
}

const Vec3f &NodeProxy::GetLocalTranslation() const
    { return GetLocalTransform().GetTranslation(); }

void NodeProxy::SetLocalTranslation(const Vec3f &translation)
{
    if (Get()) {
        Get()->SetLocalTranslation(translation);
    }
}

const Vec3f &NodeProxy::GetLocalScale() const
    { return GetLocalTransform().GetScale(); }

void NodeProxy::SetLocalScale(const Vec3f &scale)
{
    if (Get()) {
        Get()->SetLocalScale(scale);
    }
}

const Quaternion &NodeProxy::GetLocalRotation() const
    { return GetLocalTransform().GetRotation(); }

void NodeProxy::SetLocalRotation(const Quaternion &rotation)
{
    if (Get()) {
        Get()->SetLocalRotation(rotation);
    }
}

const Transform &NodeProxy::GetWorldTransform() const
{
    if (Get()) {
        return Get()->GetWorldTransform();
    }

    return Transform::identity;
}

const Vec3f &NodeProxy::GetWorldTranslation() const
    { return GetWorldTransform().GetTranslation(); }

void NodeProxy::SetWorldTranslation(const Vec3f &translation)
{
    if (Get()) {
        Get()->SetWorldTranslation(translation);
    }
}

const Vec3f &NodeProxy::GetWorldScale() const
    { return GetWorldTransform().GetScale(); }

void NodeProxy::SetWorldScale(const Vec3f &scale)
{
    if (Get()) {
        Get()->SetWorldScale(scale);
    }
}

const Quaternion &NodeProxy::GetWorldRotation() const
    { return GetWorldTransform().GetRotation(); }

void NodeProxy::SetWorldRotation(const Quaternion &rotation)
{
    if (Get()) {
        Get()->SetWorldRotation(rotation);
    }
}

const BoundingBox &NodeProxy::GetLocalAABB() const
{
    if (Get()) {
        return Get()->GetLocalAABB();
    }

    return BoundingBox::Empty();
}

void NodeProxy::SetLocalAABB(const BoundingBox &aabb)
{
    if (Get()) {
        Get()->SetLocalAABB(aabb);
    }
}

const BoundingBox &NodeProxy::GetWorldAABB() const
{
    if (Get()) {
        return Get()->GetWorldAABB();
    }

    return BoundingBox::Empty();
}

void NodeProxy::SetWorldAABB(const BoundingBox &aabb)
{
    if (Get()) {
        Get()->SetWorldAABB(aabb);
    }
}

bool NodeProxy::IsTransformLocked() const
{
    if (Get()) {
        return Get()->IsTransformLocked();
    }

    return false;
}

void NodeProxy::LockTransform()
{
    if (Get()) {
        Get()->LockTransform();
    }
}

void NodeProxy::UnlockTransform()
{
    if (Get()) {
        Get()->UnlockTransform();
    }
}

uint NodeProxy::CalculateDepth() const
{
    if (Get()) {
        return Get()->CalculateDepth();
    }

    return 0;
}

HashCode NodeProxy::GetHashCode() const
{
    HashCode hc;

    if (Get()) {
        hc.Add(Get()->GetHashCode());
    }

    return hc;
}

} // namespace hyperion::v2