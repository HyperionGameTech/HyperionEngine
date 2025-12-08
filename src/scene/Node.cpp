/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/Node.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Entity.hpp>
#include <scene/BVH.hpp>
#include <scene/DetachedScene.hpp>

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

#include <core/reflection/Class.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorState.hpp>
#include <editor/EditorPickCache.hpp>
#endif

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/util/SafeDeleter.hpp>

#include <cstring>

#include <Node.generated.inl>

namespace hyperion {

#ifdef HYP_EDITOR
extern Handle<EditorState> g_editorState;
#endif

void Node_OnPostLoad(Node& node)
{
    node.SetScene(GetDetachedSceneForThread(g_gameThread));
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
    : m_name(name),
      m_nodeFlags(NodeFlags::DEFAULT),
      m_parentNode(nullptr),
      m_localTransform(localTransform),
      m_scene(scene != nullptr ? scene : GetDetachedSceneForCurrentThread()),
      m_transformLocked(false)
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
    if (m_name == name)
    {
        return;
    }

    m_name = name;
}

bool Node::HasName() const
{
    return m_name.IsValid();
}

void Node::SetNodeFlags(EnumFlags<NodeFlags> flags)
{
    if (m_nodeFlags == flags)
    {
        return;
    }

    const bool wasStatic = IsStatic();

    m_nodeFlags = flags;

    const bool isStatic = IsStatic();

    if (wasStatic != isStatic)
    {
        OnMobilityChanged(isStatic);
    }
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
    HYP_SCOPE;

    if (!scene)
    {
        scene = GetDetachedSceneForCurrentThread();
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

void Node::OnMobilityChanged(bool isStatic)
{
    HYP_SCOPE;

    if (isStatic)
    {
        for (const Handle<Node>& child : m_childNodes)
        {
            if (!child.IsValid())
            {
                continue;
            }

            child->SetNodeFlags(child->GetNodeFlags() & ~MOBILITY_STATIC_BY_PROXY);
        }
    }
    else
    {
        for (const Handle<Node>& child : m_childNodes)
        {
            if (!child.IsValid())
            {
                continue;
            }

            child->SetNodeFlags(child->GetNodeFlags() | MOBILITY_STATIC_BY_PROXY);
        }
    }
}

void Node::OnAttachedToNode(Node* node)
{
    Assert(node != nullptr);

    m_parentNode = node;

    if (m_parentNode->IsStatic())
    {
        SetNodeFlags(GetNodeFlags() | MOBILITY_STATIC_BY_PROXY);
    }
    else
    {
        SetNodeFlags(GetNodeFlags() & ~MOBILITY_STATIC_BY_PROXY);
    }
}

void Node::OnDetachedFromNode(Node* node)
{
    Assert(node != nullptr);

    SetNodeFlags(GetNodeFlags() & ~MOBILITY_STATIC_BY_PROXY);

    m_parentNode = nullptr;
}

void Node::SetChildren(const NodeList& children)
{
    HYP_SCOPE;

    RemoveAllChildren();

    for (const Handle<Node>& child : children)
    {
        AddChild(child);
    }
}

Handle<Node> Node::AddChild(const Handle<Node>& node)
{
    HYP_SCOPE;

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

    while (currentParent != nullptr)
    {
        currentParent->OnChildAdded(node, /* direct */ currentParent == this);

        currentParent = currentParent->m_parentNode;
    }

    return node;
}

bool Node::RemoveChild(const Node* node)
{
    HYP_SCOPE;

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

    while (currentParent != nullptr)
    {
        currentParent->OnChildRemoved(const_cast<Node*>(node), /* direct */ currentParent == this);

        currentParent = currentParent->m_parentNode;
    }

    UpdateWorldTransform();

    SafeDelete(std::move(childNode));

    return true;
}

bool Node::RemoveAt(int index)
{
    HYP_SCOPE;

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
    HYP_SCOPE;

    if (!m_parentNode)
    {
        SetScene(nullptr);

        return true;
    }

    return m_parentNode->RemoveChild(this);
}

void Node::RemoveAllChildren()
{
    HYP_SCOPE;

    for (auto it = m_childNodes.begin(); it != m_childNodes.end();)
    {
        if (const Handle<Node>& node = *it)
        {
            Assert(node.IsValid());
            Assert(node->GetParent() == this);

            node->OnDetachedFromNode(this);
            node->SetScene(nullptr);

            Node* currentParent = this;

            while (currentParent != nullptr)
            {
                currentParent->OnChildRemoved(node, /* direct */ currentParent == this);

                currentParent = currentParent->m_parentNode;
            }
        }

        it = m_childNodes.Erase(it);
        SafeDelete(std::move(*it));
    }

    UpdateWorldTransform();
}

int Node::NumChildren() const
{
    return int(m_childNodes.Size());
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
    HYP_SCOPE;

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
    const StringHash stringHash { name };

    return m_childNodes.FindIf([stringHash](const auto& it)
        {
            if (!it.IsValid())
            {
                return false;
            }

            return it->GetName() == stringHash;
        });
}

Node::NodeList::ConstIterator Node::FindChild(const char* name) const
{
    const StringHash stringHash { name };

    return m_childNodes.FindIf([stringHash](const auto& it)
        {
            if (!it.IsValid())
            {
                return false;
            }

            return it->GetName() == stringHash;
        });
}

Array<Node*> Node::GetDescendantsArray() const
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

void Node::SetIsStatic(bool isStatic)
{
    constexpr EnumFlags<NodeFlags> ControlledMobilityFlags = NodeFlags::MOBILITY_STATIC | NodeFlags::MOBILITY_DYNAMIC;

    EnumFlags<NodeFlags> newNodeFlags = m_nodeFlags;

    newNodeFlags &= ~ControlledMobilityFlags;

    if (isStatic)
    {
        newNodeFlags |= NodeFlags::MOBILITY_STATIC;
    }
    else
    {
        newNodeFlags |= NodeFlags::MOBILITY_DYNAMIC;
    }

    if (newNodeFlags == m_nodeFlags)
    {
        return;
    }

    SetNodeFlags(newNodeFlags);
}

void Node::SetIsDynamic(bool isDynamic)
{
    SetIsStatic(!isDynamic);
}

void Node::SetLocalBounds(const BoundingBox& aabb)
{
    if (m_localBounds == aabb)
    {
        return;
    }

    m_localBounds = aabb;
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

        if (!(child->GetNodeFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB))
        {
            aabb = aabb.Union(child->GetLocalTransform() * child->GetLocalAABB());
        }
    }

    return aabb;
}

BoundingBox Node::GetLocalAABB() const
{
    BoundingBox aabb = m_localBounds.IsValid() ? m_localBounds : BoundingBox::Zero();

    for (const Handle<Node>& child : GetChildren())
    {
        if (!child.IsValid())
        {
            continue;
        }

        if (!(child->GetNodeFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB))
        {
            aabb = aabb.Union(child->GetLocalTransform() * child->GetLocalAABB());
        }
    }

    return aabb;
}

BoundingBox Node::GetWorldAABB() const
{
    BoundingBox aabb = m_worldTransform * (m_localBounds.IsValid() ? m_localBounds : BoundingBox::Zero());

    for (const Handle<Node>& child : GetChildren())
    {
        if (!child.IsValid())
        {
            continue;
        }

        if (!(child->GetNodeFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB))
        {
            aabb = aabb.Union(child->GetWorldAABB());
        }
    }

    return aabb;
}

void Node::UpdateWorldTransform(bool updateChildTransforms)
{
    HYP_SCOPE;

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

        if (m_nodeFlags & NodeFlags::IGNORE_PARENT_TRANSFORM)
        {
            if (m_nodeFlags & NodeFlags::IGNORE_PARENT_TRANSLATION)
            {
                worldTransform.GetTranslation() = m_localTransform.GetTranslation();
            }

            if (m_nodeFlags & NodeFlags::IGNORE_PARENT_ROTATION)
            {
                worldTransform.GetRotation() = m_localTransform.GetRotation();
            }

            if (m_nodeFlags & NodeFlags::IGNORE_PARENT_SCALE)
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

bool Node::TestRay(const Ray& ray, RayTestResults& outResults, EnumFlags<RayTestFlags> flags) const
{
    HYP_SCOPE;

    const BoundingBox worldAabb = GetWorldAABB();

    bool hasEntityHit = false;

    if (ray.TestAABB(worldAabb))
    {
        if (IsA(Entity::StaticClass()))
        {
            ResourceHandle resourceHandle;
            MeshAsset* meshAsset = nullptr;

#ifdef HYP_EDITOR
            EditorPickCacheEntry* pickCacheEntry = nullptr;
#endif

            const Entity* entity = static_cast<const Entity*>(this);

            const BVHNode* bvh = nullptr;
            Mat4f modelMatrix = Mat4f::Identity();

            if (flags & RTF_USE_BVH)
            {
                if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->mesh.IsValid())
                {
                    if (meshComponent->mesh->GetBVH().IsValid())
                    {
                        bvh = &meshComponent->mesh->GetBVH();
                        meshAsset = meshComponent->mesh->GetAsset();

#ifdef HYP_EDITOR
                        if (flags & RTF_EDITOR_PICK)
                        {
                            pickCacheEntry = g_editorState->GetPickCache().GetEntry(meshComponent->mesh);
                        }

                        if (!pickCacheEntry)
#endif

                            if (meshAsset != nullptr)
                            {
                                resourceHandle = ResourceHandle(*meshAsset->GetResource());
                            }
                    }
                }

                if (TransformComponent* transformComponent = entity->TryGetComponent<TransformComponent>())
                {
                    modelMatrix = transformComponent->transform.GetMatrix();
                }
            }

            if (bvh)
            {
                RayTestResults localBvhResults;

                const Ray localSpaceRay = modelMatrix.Inverted() * ray;

#ifdef HYP_EDITOR
                if ((flags & RTF_EDITOR_PICK) && pickCacheEntry)
                {
                    localBvhResults = bvh->TestRay(
                        localSpaceRay,
                        pickCacheEntry->positions.ToSpan(),
                        pickCacheEntry->indices.ToSpan());
                }

                else
#endif
                {
                    AssertDebug(resourceHandle && meshAsset);

                    const MeshData& meshData = *meshAsset->GetMeshData();

                    localBvhResults = bvh->TestRay(
                        localSpaceRay,
                        meshData.vertexData.ToSpan(),
                        Span<const uint32>(reinterpret_cast<const uint32*>(meshData.indexData.Data()), meshData.indexData.Size() / sizeof(uint32)));
                }

                if (localBvhResults.Any())
                {
                    const Mat4f normalMatrix = modelMatrix.Transposed().Inverted();

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

            if (childNode->TestRay(ray, outResults, flags))
            {
                hasEntityHit = true;
            }
        }
    }

    return hasEntityHit;
}

Handle<Node> Node::FindChildByName(StringHash name) const
{
    HYP_SCOPE;

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

Handle<Node> Node::FindChildByUUID(const Uuid& uuid) const
{
    HYP_SCOPE;

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

    m_tags.Add(std::move(value));
}

bool Node::RemoveTag(StringHash key)
{
    HYP_SCOPE;

    return m_tags.Remove(key);
}

const NodeTag& Node::GetTag(StringHash key) const
{
    HYP_SCOPE;

    static const NodeTag emptyTag = NodeTag();

    const NodeTag* pTag = m_tags.Get(key);

    return pTag != nullptr ? *pTag : emptyTag;
}

bool Node::HasTag(StringHash key) const
{
    HYP_SCOPE;

    return m_tags.Has(key);
}

#pragma endregion Node

} // namespace hyperion
