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
#include <scene/components/NodeLinkComponent.hpp>
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

Node::Node(Name name, const Transform& localTransform)
    : Node(name, Handle<Entity>::empty, localTransform)
{
}

Node::Node(Name name, const Handle<Entity>& entity, const Transform& localTransform)
    : Node(Type::NODE, name, entity, localTransform, GetDefaultScene())
{
}

Node::Node(Name name, const Handle<Entity>& entity, const Transform& localTransform, Scene* scene)
    : Node(Type::NODE, name, entity, localTransform, scene)
{
}

Node::Node(Type type, Name name, const Handle<Entity>& entity, const Transform& localTransform, Scene* scene)
    : m_type(type),
      m_name(name.IsValid() ? name : NAME("<unnamed>")),
      m_parentNode(nullptr),
      m_localTransform(localTransform),
      m_scene(scene != nullptr ? scene : GetDefaultScene()),
      m_transformLocked(false),
      m_transformChanged(false),
      m_delegates(MakeUnique<Delegates>())
{
    SetEntity(entity);

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

Node::Node(Node&& other) noexcept
    : m_type(other.m_type),
      m_flags(other.m_flags),
      m_name(other.m_name),
      m_parentNode(other.m_parentNode),
      m_localTransform(other.m_localTransform),
      m_worldTransform(other.m_worldTransform),
      m_entityAabb(other.m_entityAabb),
      m_scene(other.m_scene),
      m_transformLocked(other.m_transformLocked),
      m_transformChanged(other.m_transformChanged),
      m_delegates(std::move(other.m_delegates))
{
    other.m_type = Type::NODE;
    other.m_flags = NodeFlags::NONE;
    other.m_name = NAME("<unnamed>");
    other.m_parentNode = nullptr;
    other.m_localTransform = Transform::identity;
    other.m_worldTransform = Transform::identity;
    other.m_entityAabb = BoundingBox::Empty();
    other.m_scene = GetDefaultScene();
    other.m_transformLocked = false;
    other.m_transformChanged = false;

    Handle<Entity> entity = other.m_entity;
    other.m_entity = Handle<Entity>::empty;
    SetEntity(entity);

    m_childNodes = std::move(other.m_childNodes);
    m_descendants = std::move(other.m_descendants);

    for (const Handle<Node>& node : m_childNodes)
    {
        Assert(node.IsValid());

        node->m_parentNode = this;
        node->SetScene(m_scene);
    }
}

Node& Node::operator=(Node&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    RemoveAllChildren();

    SetEntity(Handle<Entity>::empty);
    SetScene(nullptr);

    m_delegates = std::move(other.m_delegates);

    m_type = other.m_type;
    other.m_type = Type::NODE;

    m_flags = other.m_flags;
    other.m_flags = NodeFlags::NONE;

    m_parentNode = other.m_parentNode;
    other.m_parentNode = nullptr;

    m_transformLocked = other.m_transformLocked;
    other.m_transformLocked = false;

    m_transformChanged = other.m_transformChanged;
    other.m_transformChanged = false;

    m_localTransform = other.m_localTransform;
    other.m_localTransform = Transform::identity;

    m_worldTransform = other.m_worldTransform;
    other.m_worldTransform = Transform::identity;

    m_entityAabb = other.m_entityAabb;
    other.m_entityAabb = BoundingBox::Empty();

    m_scene = other.m_scene;
    other.m_scene = GetDefaultScene();

    Handle<Entity> entity = other.m_entity;
    other.m_entity = Handle<Entity>::empty;
    SetEntity(entity);

    m_name = std::move(other.m_name);

    m_childNodes = std::move(other.m_childNodes);
    m_descendants = std::move(other.m_descendants);

    for (const Handle<Node>& node : m_childNodes)
    {
        Assert(node.IsValid());

        node->m_parentNode = this;
        node->SetScene(m_scene);
    }

    return *this;
}

Node::~Node()
{
    for (const Handle<Node>& child : m_childNodes)
    {
        if (!child.IsValid())
        {
            continue;
        }

        child->m_parentNode = nullptr;
    }
}

void Node::Init()
{
    if (m_entity.IsValid())
    {
        InitObject(m_entity);
    }

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

void Node::OnNestedNodeAdded(Node* node, bool direct)
{
    m_descendants.PushBack(node);

    if (m_parentNode != nullptr)
    {
        m_parentNode->OnNestedNodeAdded(node, false);
    }
}

void Node::OnNestedNodeRemoved(Node* node, bool direct)
{
    const auto it = m_descendants.Find(node);

    if (it != m_descendants.End())
    {
        m_descendants.Erase(it);
    }

    if (m_parentNode != nullptr)
    {
        m_parentNode->OnNestedNodeRemoved(node, false);
    }
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

    if (node->m_transformLocked)
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

    OnNestedNodeAdded(node, true);

    for (Node* nested : node->GetDescendants())
    {
        OnNestedNodeAdded(nested, false);
    }

    return node;
}

bool Node::RemoveChild(const Node* node)
{
    if (!node)
    {
        return false;
    }

    // TEMP
    if (node == m_entity.Get())
    {
        SetEntity(nullptr);
        return true;
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

    if (node->m_transformLocked)
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

    for (Node* nested : node->GetDescendants())
    {
        OnNestedNodeRemoved(nested, false);
    }

    OnNestedNodeRemoved(childNode, true);

    UpdateWorldTransform();

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

            // TEMP
            if (node.Get() == m_entity.Get())
            {
                ++it;
                continue;
            }

            node->OnDetachedFromNode(this);
            node->SetScene(nullptr);

            Node* currentParent = this;

            while (currentParent != nullptr && currentParent->m_delegates != nullptr)
            {
                currentParent->m_delegates->OnChildRemoved(node, /* direct */ currentParent == this);

                currentParent = currentParent->m_parentNode;
            }

            for (Node* nested : node->GetDescendants())
            {
                OnNestedNodeRemoved(nested, false);
            }

            OnNestedNodeRemoved(node, true);
        }

        it = m_childNodes.Erase(it);
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

Handle<Node> Node::Select(UTF8StringView selector) const
{
    Handle<Node> result;

    if (selector.Size() == 0)
    {
        return result;
    }

    char ch;

    char buffer[256];
    uint32 bufferIndex = 0;
    uint32 selectorIndex = 0;

    const Node* searchNode = this;

    const char* str = selector.Data();

    while ((ch = str[selectorIndex]) != '\0')
    {
        const char prevSelectorChar = selectorIndex == 0
            ? '\0'
            : str[selectorIndex - 1];

        ++selectorIndex;

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

void Node::LockTransform()
{
    m_transformLocked = true;

    // set entity to static
    if (m_entity.IsValid())
    {
        if (const Handle<EntityManager>& entityManager = m_scene->GetEntityManager())
        {
            entityManager->AddTag<EntityTag::STATIC>(m_entity);
            entityManager->RemoveTag<EntityTag::DYNAMIC>(m_entity);
        }

        m_transformChanged = false;
    }

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
    if (m_transformLocked)
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

void Node::SetEntity(const Handle<Entity>& entity)
{
    if (m_entity == entity)
    {
        return;
    }

    // Remove the NodeLinkComponent from the old entity
    if (m_entity.IsValid())
    {
        RemoveChild(m_entity);

        if (m_scene != nullptr && m_scene->GetEntityManager() != nullptr)
        {
            m_scene->GetEntityManager()->RemoveComponent<NodeLinkComponent>(m_entity);
        }
    }

    if (entity.IsValid())
    {
        AssertDebug(m_scene && m_scene->GetEntityManager());

        EntityManager* previousEntityManager = entity->GetEntityManager();

        // TEMP
        if (previousEntityManager)
        {
            // Update / add a NodeLinkComponent to the new entity
            if (NodeLinkComponent* nodeLinkComponent = previousEntityManager->TryGetComponent<NodeLinkComponent>(entity))
            {
                nodeLinkComponent->node = WeakHandleFromThis();
            }
            else
            {
                previousEntityManager->AddComponent<NodeLinkComponent>(entity, { WeakHandleFromThis() });
            }
        }

        AddChild(entity);

        // sanity check
        AssertDebug(entity->GetScene() == m_scene);

        m_entity = entity;

        // TEMP
        {

            // Update / add a NodeLinkComponent to the new entity
            if (NodeLinkComponent* nodeLinkComponent = m_scene->GetEntityManager()->TryGetComponent<NodeLinkComponent>(m_entity))
            {
                nodeLinkComponent->node = WeakHandleFromThis();
            }
            else
            {
                m_scene->GetEntityManager()->AddComponent<NodeLinkComponent>(m_entity, { WeakHandleFromThis() });
            }
        }

#ifdef HYP_EDITOR
        GetEditorDelegates([this](EditorDelegates* editorDelegates)
            {
                editorDelegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Entity")));
            });
#endif

        // If a TransformComponent already exists on the Entity, allow it to keep its current transform by moving the Node
        // to match it, as long as we're not locked
        // If transform is locked, the Entity's TransformComponent will be synced with the Node's current transform
        if (TransformComponent* transformComponent = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity))
        {
            if (!IsTransformLocked())
            {
                SetWorldTransform(transformComponent->transform);
            }
        }

        RefreshEntityTransform();

        // set entity to static by default
        if (m_scene->GetEntityManager()->HasTag<EntityTag::DYNAMIC>(m_entity))
        {
            m_scene->GetEntityManager()->RemoveTag<EntityTag::STATIC>(m_entity);
        }
        else
        {
            m_scene->GetEntityManager()->AddTag<EntityTag::STATIC>(m_entity);
            m_scene->GetEntityManager()->RemoveTag<EntityTag::DYNAMIC>(m_entity);
        }

        // set transformChanged to false until entity is set to DYNAMIC
        m_transformChanged = false;

        if (!m_scene->GetEntityManager()->HasComponent<VisibilityStateComponent>(m_entity))
        {
            m_scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(m_entity, {});
        }

        InitObject(m_entity);
    }
    else
    {
        m_entity = Handle<Entity>::empty;

        m_transformChanged = false;

#ifdef HYP_EDITOR
        GetEditorDelegates([this](EditorDelegates* editorDelegates)
            {
                editorDelegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Entity")));
            });
#endif

        SetEntityAABB(BoundingBox::Empty());

        UpdateWorldTransform();
    }
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
    if (m_transformLocked)
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

    if (m_entity.IsValid())
    {
        const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();
        Assert(entityManager.Get() == m_entity->GetEntityManager());

        // if (!m_transformChanged) {
        //     // Set to dynamic
        //     if (entityManager != nullptr) {
        //         entityManager->AddTag<EntityTag::DYNAMIC>(m_entity);
        //         entityManager->RemoveTag<EntityTag::STATIC>(m_entity);

        //         HYP_LOG(Node, Debug, "Node: {}; Make Entity #{} dynamic",
        //             GetName(),
        //             m_entity.Id().Value());
        //     }

        //     m_transformChanged = true;
        // }

        if (entityManager)
        {
            if (TransformComponent* transformComponent = entityManager->TryGetComponent<TransformComponent>(m_entity))
            {
                transformComponent->transform = m_worldTransform;
            }
            else
            {
                entityManager->AddComponent<TransformComponent>(m_entity, TransformComponent { m_worldTransform });
            }

            entityManager->AddTags<EntityTag::UPDATE_AABB>(m_entity);
        }
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
    if (m_entity.IsValid() && m_scene != nullptr && m_scene->GetEntityManager() != nullptr)
    {
        if (BoundingBoxComponent* boundingBoxComponent = m_scene->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(m_entity))
        {
            SetEntityAABB(boundingBoxComponent->localAabb);
        }
        else
        {
            SetEntityAABB(BoundingBox::Empty());
        }

        if (TransformComponent* transformComponent = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity))
        {
            transformComponent->transform = m_worldTransform;
        }
        else
        {
            m_scene->GetEntityManager()->AddComponent<TransformComponent>(m_entity, TransformComponent { m_worldTransform });
        }

        m_scene->GetEntityManager()->AddTags<EntityTag::UPDATE_AABB>(m_entity);
    }
    else
    {
        SetEntityAABB(BoundingBox::Empty());
    }
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
        if (m_entity.IsValid())
        {
            const BVHNode* bvh = nullptr;
            Matrix4 modelMatrix = Matrix4::Identity();

            if (useBvh && m_scene && m_scene->GetEntityManager())
            {
                if (MeshComponent* meshComponent = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(m_entity); meshComponent && meshComponent->mesh.IsValid())
                {
                    if (meshComponent->mesh->GetBVH().IsValid())
                    {
                        bvh = &meshComponent->mesh->GetBVH();
                    }
                }

                if (TransformComponent* transformComponent = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity))
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
                        hit.id = m_entity.Id().Value();
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
                hasEntityHit = ray.TestAABB(worldAabb, m_entity.Id().Value(), nullptr, outResults);
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
