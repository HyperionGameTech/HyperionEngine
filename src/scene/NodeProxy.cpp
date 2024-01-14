#include "NodeProxy.hpp"
#include "Node.hpp"

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>

namespace hyperion::v2 {

UInt NodeProxyChildren::Size() const
{
    return node ? UInt(node->GetChildren().Size()) : 0;
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

const NodeProxy NodeProxy::empty = NodeProxy();

NodeProxy::NodeProxy()
    : Base()
{
}

NodeProxy::NodeProxy(Node *ptr)
    : Base()
{
    Base::Reset(ptr);
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

    AssertThrow(node.Get() != Get());

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

const Vector3 &NodeProxy::GetLocalTranslation() const
    { return GetLocalTransform().GetTranslation(); }

void NodeProxy::SetLocalTranslation(const Vector3 &translation)
{
    if (Get()) {
        Get()->SetLocalTranslation(translation);
    }
}

const Vector3 &NodeProxy::GetLocalScale() const
    { return GetLocalTransform().GetScale(); }

void NodeProxy::SetLocalScale(const Vector3 &scale)
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

const Vector3 &NodeProxy::GetWorldTranslation() const
    { return GetWorldTransform().GetTranslation(); }

void NodeProxy::SetWorldTranslation(const Vector3 &translation)
{
    if (Get()) {
        Get()->SetWorldTranslation(translation);
    }
}

const Vector3 &NodeProxy::GetWorldScale() const
    { return GetWorldTransform().GetScale(); }

void NodeProxy::SetWorldScale(const Vector3 &scale)
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

    return BoundingBox::empty;
}

const BoundingBox &NodeProxy::GetWorldAABB() const
{
    if (Get()) {
        return Get()->GetWorldAABB();
    }

    return BoundingBox::empty;
}

HashCode NodeProxy::GetHashCode() const
{
    HashCode hc;

    if (Get()) {
        hc.Add(Get()->GetHashCode());
    }

    return hc;
}


static NodeProxy ScriptCreateNodeProxy(void *)
{
    return NodeProxy(new Node);
}

static struct NodeProxyScriptBindings : ScriptBindingsBase
{
    NodeProxyScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<NodeProxy>())
    {
    }

    virtual void Generate(APIInstance &api_instance) override
    {
        api_instance.Module(Config::global_module_name)
            .Class<NodeProxy>(
                "NodeProxy",
                {
                    API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                    API::NativeMemberDefine(
                        "$construct",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxFn< NodeProxy, void *, ScriptCreateNodeProxy >
                    ),
                    API::NativeMemberDefine(
                        "GetName",
                        BuiltinTypes::STRING,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< const String &, NodeProxy, &NodeProxy::GetName >
                    ),
                    API::NativeMemberDefine(
                        "SetName",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "name", BuiltinTypes::STRING }
                        },
                        CxxMemberFn< void, NodeProxy, const String &, &NodeProxy::SetName >
                    )
                }
            );
    }
} node_proxy_script_bindings = { };

} // namespace hyperion::v2