/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Node.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Entity.hpp>
#include <scene/BVH.hpp>

#include <scene/animation/Bone.hpp>

#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypData.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#endif

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <rendering/Mesh.hpp>

#include <cstring>

namespace hyperion {

HYP_API void Node_OnPostLoad(Node& node)
{
    node.SetScene(g_engineDriver->GetDefaultWorld()->GetDetachedScene(g_gameThread));
}

#pragma region NodeTag

String NodeTag::ToString() const
{
    if (!data.IsValid())
    {
        return String::empty;
    }

    String str;

    Visit(data, [&str](const auto& value)
        {
            str = HYP_FORMAT("{}", value);
        });

    return str;
}

#pragma endregion NodeTag

#pragma region Node

// @NOTE: In some places we have a m_scene->GetEntityManager() != nullptr check,
// this only happens in the case that the scene in question is destructing and
// this Node is held on a component that the EntityManager has.
// In practice it only really shows up on UI objects where UIObject holds a reference to a Node.

Node::Node(Name name, const Transform& localTransform, Scene* scene)
    : m_name(name.IsValid() ? name : NAME("<unnamed>")),
      m_parentNode(nullptr),
      m_localTransform(localTransform),
      m_scene(scene != nullptr ? scene : GetDefaultScene()),
      m_transformLocked(false),
      m_delegates(MakeUnique<Delegates>())
{
    if (scene != nullptr)
    {
        for (const Handle<Node>& child : m_childNodes)
        {
            if (!child.IsValid())
            {
                continue;
            }

            child->SetScene(scene);
        }
    }
}

Node::~Node()
{
    for (Handle<Node>& child : m_childNodes)
    {
        if (!child.IsValid())
        {
            continue;
        }

        child->m_parentNode = nullptr;

        SafeDelete(std::move(child));
    }
}

void Node::Init()
{
    for (const Handle<Node>& child : m_childNodes)
    {
        InitObject(child);
    }

    SetReady(true);
}

void Node::SetName(Name name)
{
    if (!name.IsValid())
    {
        name = NAME("<unnamed>");
    }

    if (m_name == name)
    {
        return;
    }

    m_name = name;

#ifdef HYP_EDITOR
    GetEditorDelegates([this](EditorDelegates* editorDelegates)
        {
            editorDelegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Name")));
        });
#endif
}

bool Node::HasName() const
{
    static constexpr WeakName unnamed("<unnamed>");

    return m_name.IsValid() && m_name != unnamed;
}

void Node::SetFlags(EnumFlags<NodeFlags> flags)
{
    if (m_flags == flags)
    {
        return;
    }

    m_flags = flags;

#ifdef HYP_EDITOR
    GetEditorDelegates([this](EditorDelegates* editorDelegates)
        {
            editorDelegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Flags")));
        });
#endif
}

bool Node::IsOrHasParent(const Node* node) const
{
    if (node == nullptr)
    {
        return false;
    }

    if (node == this)
    {
        return true;
    }

    if (m_parentNode == nullptr)
    {
        return false;
    }

    return m_parentNode->IsOrHasParent(node);
}

Node* Node::FindParentWithName(UTF8StringView name) const
{
    if (m_name == name)
    {
        return const_cast<Node*>(this);
    }

    if (m_parentNode == nullptr)
    {
        return nullptr;
    }

    return m_parentNode->FindParentWithName(name);
}

void Node::SetScene(Scene* scene)
{
    if (!scene)
    {
        scene = g_engineDriver->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadId()).Get();
    }

    Assert(scene != nullptr);

    if (m_scene != scene)
    {
        Scene* previousScene = m_scene;

#ifdef HYP_DEBUG_MODE
        Assert(
            previousScene != nullptr,
            "Previous scene is null when setting new scene for Node %s - should be set to detached world scene by default",
            m_name.LookupString());
#endif

        m_scene = scene;

        for (const Handle<Node>& child : m_childNodes)
        {
            if (!child.IsValid())
            {
                continue;
            }

            child->SetScene(m_scene);
        }

#ifdef HYP_EDITOR
        GetEditorDelegates([this](EditorDelegates* editorDelegates)
            {
                editorDelegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Scene")));
            });
#endif
    }
}

World* Node::GetWorld() const
{
    return m_scene != nullptr
        ? m_scene->GetWorld()
        : g_engineDriver->GetDefaultWorld().Get();
}

void Node::OnTransformUpdated(const Transform& transform)
{
    // Do nothing
}

void Node::OnAttachedToNode(Node* node)
{
    Assert(node != nullptr);

    m_parentNode = node;
}

void Node::OnDetachedFromNode(Node* node)
{
    Assert(node != nullptr);

    m_parentNode = nullptr;
}

Handle<Node> Node::AddChild(const Handle<Node>& node)
{
    if (!node.IsValid())
    {
        return AddChild(Handle<Node>(CreateObject<Node>()));
    }

    if (node.Get() == this || node->GetParent() == this)
    {
        return node;
    }

    if (node->GetParent() != nullptr)
    {
        HYP_LOG(Node, Warning, "Attaching node {} to {} when it already has a parent node ({}). Node will be detached from parent.",
            node->GetName(), GetName(), node->GetParent()->GetName());

        node->Remove();
    }

    Assert(
        !IsOrHasParent(node.Get()),
        "Attaching node %s to %s would create a circular reference",
        node->GetName().LookupString(),
        GetName().LookupString());

    InitObject(node);

    bool wasTransformLocked = false;

    if (node->IsTransformLocked())
    {
        wasTransformLocked = true;

        node->UnlockTransform();
    }

    node->SetScene(m_scene);
    node->OnAttachedToNode(this);
    node->UpdateWorldTransform();

    if (wasTransformLocked)
    {
        node->LockTransform();
    }

    m_childNodes.PushBack(node);

    Node* currentParent = this;

    while (currentParent != nullptr && currentParent->m_delegates != nullptr)
    {
        currentParent->m_delegates->OnChildAdded(node, /* direct */ currentParent == this);

        currentParent = currentParent->m_parentNode;
    }

    return node;
}

bool Node::RemoveChild(const Node* node)
{
    if (!node)
    {
        return false;
    }

    auto it = m_childNodes.FindIf([node](const Handle<Node>& it)
        {
            return it.Get() == node;
        });

    if (it == m_childNodes.End())
    {
        return false;
    }

    Handle<Node> childNode = std::move(*it);
    m_childNodes.Erase(it);

    Assert(childNode.IsValid());
    Assert(childNode->GetParent() == this);

    bool wasTransformLocked = false;

    if (node->IsTransformLocked())
    {
        wasTransformLocked = true;

        childNode->UnlockTransform();
    }

    childNode->OnDetachedFromNode(this);
    childNode->SetScene(nullptr);
    childNode->UpdateWorldTransform();

    if (wasTransformLocked)
    {
        childNode->LockTransform();
    }

    Node* currentParent = this;

    while (currentParent != nullptr && currentParent->m_delegates != nullptr)
    {
        currentParent->m_delegates->OnChildRemoved(const_cast<Node*>(node), /* direct */ currentParent == this);

        currentParent = currentParent->m_parentNode;
    }

    UpdateWorldTransform();

    SafeDelete(std::move(childNode));

    return true;
}

bool Node::RemoveAt(int index)
{
    if (index < 0)
    {
        index = int(m_childNodes.Size()) + index;
    }

    if (index >= m_childNodes.Size())
    {
        return false;
    }

    const Handle<Node>& childNode = m_childNodes[index];

    return RemoveChild(childNode.Get());
}

bool Node::Remove()
{
    if (!m_parentNode)
    {
        SetScene(nullptr);

        return true;
    }

    return m_parentNode->RemoveChild(this);
}

void Node::RemoveAllChildren()
{
    for (auto it = m_childNodes.begin(); it != m_childNodes.end();)
    {
        if (const Handle<Node>& node = *it)
        {
            Assert(node.IsValid());
            Assert(node->GetParent() == this);

            node->OnDetachedFromNode(this);
            node->SetScene(nullptr);

            Node* currentParent = this;

            while (currentParent != nullptr && currentParent->m_delegates != nullptr)
            {
                currentParent->m_delegates->OnChildRemoved(node, /* direct */ currentParent == this);

                currentParent = currentParent->m_parentNode;
            }
        }

        it = m_childNodes.Erase(it);
        SafeDelete(std::move(*it));
    }

    UpdateWorldTransform();
}

Handle<Node> Node::GetChild(int index) const
{
    if (index < 0)
    {
        index = int(m_childNodes.Size()) + index;
    }

    if (index >= m_childNodes.Size())
    {
        return Handle<Node>::empty;
    }

    return m_childNodes[index];
}

Handle<Node> Node::Select(ANSIStringView selector) const
{
    Handle<Node> result;

    if (selector.Size() == 0)
    {
        return result;
    }

    char ch;

    char buffer[256];
    uint32 bufferIndex = 0;

    const Node* searchNode = this;

    const char* strBegin = selector.Data();
    const char* strEnd = strBegin + selector.Size();

    const char* str = strBegin;

    for (; str != strEnd && (ch = *str) != '\0'; ++str)
    {
        const char prevSelectorChar = str == strBegin ? '\0' : *(str - 1);

        if (ch == '/' && prevSelectorChar != '\\')
        {
            const auto it = searchNode->FindChild(buffer);

            if (it == searchNode->GetChildren().End())
            {
                return Handle<Node>::empty;
            }

            searchNode = it->Get();
            result = *it;

            if (!searchNode)
            {
                return Handle<Node>::empty;
            }

            bufferIndex = 0;
        }
        else if (ch != '\\')
        {
            buffer[bufferIndex] = ch;

            ++bufferIndex;

            if (bufferIndex == std::size(buffer))
            {
                HYP_LOG(Node, Warning, "Node search string too long, must be within buffer size limit of {}",
                    std::size(buffer));

                return Handle<Node>::empty;
            }
        }

        buffer[bufferIndex] = '\0';
    }

    // find remaining
    if (bufferIndex != 0)
    {
        const auto it = searchNode->FindChild(buffer);

        if (it == searchNode->GetChildren().End())
        {
            return Handle<Node>::empty;
        }

        searchNode = it->Get();
        result = *it;
    }

    return result;
}

Node::NodeList::Iterator Node::FindChild(const Node* node)
{
    return m_childNodes.FindIf([node](const auto& it)
        {
            return it.Get() == node;
        });
}

Node::NodeList::ConstIterator Node::FindChild(const Node* node) const
{
    return m_childNodes.FindIf([node](const auto& it)
        {
            return it.Get() == node;
        });
}

Node::NodeList::Iterator Node::FindChild(const char* name)
{
    const WeakName weakName { name };

    return m_childNodes.FindIf([weakName](const auto& it)
        {
            if (!it.IsValid())
            {
                return false;
            }

            return it->GetName() == weakName;
        });
}

Node::NodeList::ConstIterator Node::FindChild(const char* name) const
{
    const WeakName weakName { name };

    return m_childNodes.FindIf([weakName](const auto& it)
        {
            if (!it.IsValid())
            {
                return false;
            }

            return it->GetName() == weakName;
        });
}

Array<Node*> Node::GetDescendants() const
{
    // add all children to the list
    Array<Node*> descendants;

    typedef void (*CollectFunc)(Array<Node*>& descendants, const Node& target, void* collectFunc);

    CollectFunc collectFunc = [](Array<Node*>& descendants, const Node& target, void* collectFunc)
    {
        descendants.Reserve(descendants.Size() + target.GetChildren().Size());

        for (const Handle<Node>& child : target.GetChildren())
        {
            AssertDebug(child != nullptr);

            descendants.PushBack(child.Get());

            reinterpret_cast<CollectFunc>(collectFunc)(descendants, *child, collectFunc);
        }
    };

    collectFunc(descendants, *this, reinterpret_cast<void*>(collectFunc));

    return descendants;
}

void Node::LockTransform()
{
    m_transformLocked = true;

    for (const Handle<Node>& child : m_childNodes)
    {
        if (!child.IsValid())
        {
            continue;
        }

        child->LockTransform();
    }
}

void Node::UnlockTransform()
{
    m_transformLocked = false;

    for (const Handle<Node>& child : m_childNodes)
    {
        if (!child.IsValid())
        {
            continue;
        }

        child->UnlockTransform();
    }
}

void Node::SetLocalTransform(const Transform& transform)
{
    if (IsTransformLocked())
    {
        return;
    }

    if (m_localTransform == transform)
    {
        return;
    }

    m_localTransform = transform;

    UpdateWorldTransform();
}

Transform Node::GetRelativeTransform(const Transform& parentTransform) const
{
    return parentTransform.GetInverse() * m_worldTransform;
}

void Node::SetEntityAABB(const BoundingBox& aabb)
{
    if (m_entityAabb == aabb)
    {
        return;
    }

    m_entityAabb = aabb;

#ifdef HYP_EDITOR
    GetEditorDelegates([this](EditorDelegates* editorDelegates)
        {
            editorDelegates->OnNodeUpdate(this, Class()->GetProperty(NAME("EntityAABB")));
            editorDelegates->OnNodeUpdate(this, Class()->GetProperty(NAME("LocalAABB")));
            editorDelegates->OnNodeUpdate(this, Class()->GetProperty(NAME("WorldAABB")));
        });
#endif
}

BoundingBox Node::GetLocalAABBExcludingSelf() const
{
    BoundingBox aabb = BoundingBox::Zero();

    for (const Handle<Node>& child : GetChildren())
    {
        if (!child.IsValid())
        {
            continue;
        }

        if (!(child->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB))
        {
            aabb = aabb.Union(child->GetLocalTransform() * child->GetLocalAABB());
        }
    }

    return aabb;
}

BoundingBox Node::GetLocalAABB() const
{
    BoundingBox aabb = m_entityAabb.IsValid() ? m_entityAabb : BoundingBox::Zero();

    for (const Handle<Node>& child : GetChildren())
    {
        if (!child.IsValid())
        {
            continue;
        }

        if (!(child->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB))
        {
            aabb = aabb.Union(child->GetLocalTransform() * child->GetLocalAABB());
        }
    }

    return aabb;
}

BoundingBox Node::GetWorldAABB() const
{
    BoundingBox aabb = m_worldTransform * (m_entityAabb.IsValid() ? m_entityAabb : BoundingBox::Zero());

    for (const Handle<Node>& child : GetChildren())
    {
        if (!child.IsValid())
        {
            continue;
        }

        if (!(child->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB))
        {
            aabb = aabb.Union(child->GetWorldAABB());
        }
    }

    return aabb;
}

void Node::UpdateWorldTransform(bool updateChildTransforms)
{
    if (IsTransformLocked())
    {
        return;
    }

    if (IsA<Bone>())
    {
        static_cast<Bone*>(this)->UpdateBoneTransform(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    const Transform transformBefore = m_worldTransform;

    Transform worldTransform = m_localTransform;

    if (m_parentNode != nullptr)
    {
        worldTransform = m_parentNode->GetWorldTransform() * m_localTransform;

        if (m_flags & NodeFlags::IGNORE_PARENT_TRANSFORM)
        {
            if (m_flags & NodeFlags::IGNORE_PARENT_TRANSLATION)
            {
                worldTransform.GetTranslation() = m_localTransform.GetTranslation();
            }

            if (m_flags & NodeFlags::IGNORE_PARENT_ROTATION)
            {
                worldTransform.GetRotation() = m_localTransform.GetRotation();
            }

            if (m_flags & NodeFlags::IGNORE_PARENT_SCALE)
            {
                worldTransform.GetScale() = m_localTransform.GetScale();
            }
        }
    }

    m_worldTransform = worldTransform;

    if (m_worldTransform == transformBefore)
    {
        return;
    }

    OnTransformUpdated(m_worldTransform);

    if (updateChildTransforms)
    {
        for (const Handle<Node>& node : m_childNodes)
        {
            node->UpdateWorldTransform(true);
        }
    }

#ifdef HYP_EDITOR
    GetEditorDelegates([this](EditorDelegates* editorDelegates)
        {
            editorDelegates->OnNodeUpdate(this, Node::Class()->GetProperty(NAME("LocalTransform")));
            editorDelegates->OnNodeUpdate(this, Node::Class()->GetProperty(NAME("WorldTransform")));
            editorDelegates->OnNodeUpdate(this, Node::Class()->GetProperty(NAME("LocalAABB")));
            editorDelegates->OnNodeUpdate(this, Node::Class()->GetProperty(NAME("WorldAABB")));
        });
#endif
}

void Node::RefreshEntityTransform()
{
}

uint32 Node::CalculateDepth() const
{
    uint32 depth = 0;

    Node* parent = m_parentNode;

    while (parent != nullptr)
    {
        ++depth;
        parent = parent->GetParent();
    }

    return depth;
}

uint32 Node::FindSelfIndex() const
{
    if (m_parentNode == nullptr)
    {
        return ~0u;
    }

    const auto it = m_parentNode->FindChild(this);

    if (it == m_parentNode->GetChildren().End())
    {
        return 0;
    }

    return uint32(it - m_parentNode->GetChildren().Begin());
}

bool Node::TestRay(const Ray& ray, RayTestResults& outResults, bool useBvh) const
{
    const BoundingBox worldAabb = GetWorldAABB();

    bool hasEntityHit = false;

    if (ray.TestAABB(worldAabb))
    {
        if (InstanceClass() == Entity::Class())
        {
            const Entity* entity = static_cast<const Entity*>(this);

            const BVHNode* bvh = nullptr;
            Matrix4 modelMatrix = Matrix4::Identity();

            if (useBvh)
            {
                if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->mesh.IsValid())
                {
                    if (meshComponent->mesh->GetBVH().IsValid())
                    {
                        bvh = &meshComponent->mesh->GetBVH();
                    }
                }

                if (TransformComponent* transformComponent = entity->TryGetComponent<TransformComponent>())
                {
                    modelMatrix = transformComponent->transform.GetMatrix();
                }
            }

            if (bvh)
            {
                const Ray localSpaceRay = modelMatrix.Inverted() * ray;

                RayTestResults localBvhResults = bvh->TestRay(localSpaceRay);

                if (localBvhResults.Any())
                {
                    const Matrix4 normalMatrix = modelMatrix.Transposed().Inverted();

                    RayTestResults bvhResults;

                    for (RayHit hit : localBvhResults)
                    {
                        hit.id = Id().Value();
                        hit.userData = nullptr;

                        Vec4f transformedNormal = normalMatrix * Vec4f(hit.normal, 0.0f);
                        hit.normal = transformedNormal.GetXYZ().Normalized();

                        Vec4f transformedPosition = modelMatrix * Vec4f(hit.hitpoint, 1.0f);
                        transformedPosition /= transformedPosition.w;

                        hit.hitpoint = transformedPosition.GetXYZ();

                        hit.distance = (hit.hitpoint - ray.position).Length();

                        bvhResults.AddHit(hit);
                    }

                    outResults.Merge(std::move(bvhResults));

                    hasEntityHit = true;
                }
            }
            else
            {
                hasEntityHit = ray.TestAABB(worldAabb, entity->Id().Value(), nullptr, outResults);
            }
        }

        for (const Handle<Node>& childNode : m_childNodes)
        {
            if (!childNode.IsValid())
            {
                continue;
            }

            if (childNode->TestRay(ray, outResults, useBvh))
            {
                hasEntityHit = true;
            }
        }
    }

    return hasEntityHit;
}

Handle<Node> Node::FindChildByName(WeakName name) const
{
    // breadth-first search
    Queue<const Node*> queue;
    queue.Push(this);

    while (queue.Any())
    {
        const Node* parent = queue.Pop();

        for (const Handle<Node>& child : parent->GetChildren())
        {
            if (!child.IsValid())
            {
                continue;
            }

            if (child->GetName() == name)
            {
                return child;
            }

            queue.Push(child.Get());
        }
    }

    return Handle<Node>::empty;
}

Handle<Node> Node::FindChildByUUID(const UUID& uuid) const
{
    // breadth-first search
    Queue<const Node*> queue;
    queue.Push(this);

    while (queue.Any())
    {
        const Node* parent = queue.Pop();

        for (const Handle<Node>& child : parent->GetChildren())
        {
            if (!child)
            {
                continue;
            }

            if (child->GetUUID() == uuid)
            {
                return child;
            }

            queue.Push(child.Get());
        }
    }

    return Handle<Node>::empty;
}

void Node::AddTag(NodeTag&& value)
{
    HYP_SCOPE;

    m_tags.Set(std::move(value));
}

bool Node::RemoveTag(WeakName key)
{
    HYP_SCOPE;

    auto it = m_tags.FindAs(key);

    if (it == m_tags.End())
    {
        return false;
    }

    m_tags.Erase(it);

    return true;
}

const NodeTag& Node::GetTag(WeakName key) const
{
    HYP_SCOPE;

    static const NodeTag emptyTag = NodeTag();

    const auto it = m_tags.FindAs(key);

    if (it == m_tags.End())
    {
        return emptyTag;
    }

    return *it;
}

bool Node::HasTag(WeakName key) const
{
    HYP_SCOPE;

    return m_tags.FindAs(key) != m_tags.End();
}

Scene* Node::GetDefaultScene()
{
    return g_engineDriver->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadId()).Get();
}

#ifdef HYP_EDITOR

EditorDelegates* Node::GetEditorDelegates()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    if (EditorSubsystem* editorSubsystem = g_engineDriver->GetDefaultWorld()->GetSubsystem<EditorSubsystem>())
    {
        return editorSubsystem->GetEditorDelegates();
    }

    return nullptr;
}

template <class Function>
void Node::GetEditorDelegates(Function&& func)
{
    if (Threads::IsOnThread(g_gameThread))
    {
        if (EditorSubsystem* editorSubsystem = g_engineDriver->GetDefaultWorld()->GetSubsystem<EditorSubsystem>())
        {
            func(editorSubsystem->GetEditorDelegates());
        }
    }
    else
    {
        Threads::GetThread(g_gameThread)->GetScheduler().Enqueue([weakThis = WeakHandleFromThis(), func = std::forward<Function>(func)]()
            {
                if (Handle<Node> strongThis = weakThis.Lock())
                {
                    if (EditorSubsystem* editorSubsystem = g_engineDriver->GetDefaultWorld()->GetSubsystem<EditorSubsystem>())
                    {
                        func(editorSubsystem->GetEditorDelegates());
                    }

                    return;
                }

                HYP_LOG(Node, Warning, "Node is no longer valid when trying to get editor delegates");
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

#endif

#pragma endregion Node

} // namespace hyperion
