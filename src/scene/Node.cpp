/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Node.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

#include <scene/animation/Bone.hpp>

#include <core/system/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#include <editor/EditorDelegates.hpp>

#include <Engine.hpp>

#include <cstring>

namespace hyperion {

static const String g_unnamed_node_name = "<unnamed>";

#pragma region NodeTag

String NodeTag::ToString() const
{
    if (value.GetTypeID() == TypeID::ForType<String>()) {
        return value.Get<String>();
    }

    if (value.GetTypeID() == TypeID::ForType<UUID>()) {
        return value.Get<UUID>().ToString();
    }

    if (value.GetTypeID() == TypeID::ForType<Name>()) {
        return value.Get<Name>().LookupString();
    }

    if (value.GetTypeID() == TypeID::ForType<int32>()) {
        return HYP_FORMAT("{}", value.Get<int32>());
    }

    if (value.GetTypeID() == TypeID::ForType<uint32>()) {
        return HYP_FORMAT("{}", value.Get<uint32>());
    }

    if (value.GetTypeID() == TypeID::ForType<float>()) {
        return HYP_FORMAT("{}", value.Get<float>());
    }

    if (value.GetTypeID() == TypeID::ForType<double>()) {
        return HYP_FORMAT("{}", value.Get<double>());
    }

    if (value.GetTypeID() == TypeID::ForType<bool>()) {
        return value.Get<bool>() ? "true" : "false";
    }

    return String::empty;   
}

#pragma endregion NodeTag

#pragma region Node

// @NOTE: In some places we have a m_scene->GetEntityManager() != nullptr check,
// this only happens in the case that the scene in question is destructing and
// this Node is held on a component that the EntityManager has.
// In practice it only really shows up on UI objects where UIObject holds a reference to a Node.

Node::Node(
    const String &name,
    const Transform &local_transform
) : Node(
        name,
        ID<Entity>::invalid,
        local_transform
    )
{
}

Node::Node(
    const String &name,
    ID<Entity> entity,
    const Transform &local_transform
) : Node(
        Type::NODE,
        name,
        entity,
        local_transform,
        GetDefaultScene()
    )
{
}

Node::Node(
    const String &name,
    ID<Entity> entity,
    const Transform &local_transform,
    Scene *scene
) : Node(
        Type::NODE,
        name,
        entity,
        local_transform,
        scene
    )
{
}

Node::Node(
    Type type,
    const String &name,
    ID<Entity> entity,
    const Transform &local_transform,
    Scene *scene
) : m_type(type),
    m_name(name.Empty() ? g_unnamed_node_name : name),
    m_parent_node(nullptr),
    m_local_transform(local_transform),
    m_scene(scene != nullptr ? scene : GetDefaultScene()),
    m_transform_locked(false),
    m_delegates(MakeUnique<Delegates>())
{
    SetEntity(entity);
}

Node::Node(Node &&other) noexcept
    : m_type(other.m_type),
      m_flags(other.m_flags),
      m_name(std::move(other.m_name)),
      m_parent_node(other.m_parent_node),
      m_local_transform(other.m_local_transform),
      m_world_transform(other.m_world_transform),
      m_entity_aabb(other.m_entity_aabb),
      m_scene(other.m_scene),
      m_transform_locked(other.m_transform_locked),
      m_delegates(std::move(other.m_delegates))
{
    other.m_type = Type::NODE;
    other.m_flags = NodeFlags::NONE;
    other.m_name = g_unnamed_node_name;
    other.m_parent_node = nullptr;
    other.m_local_transform = Transform::identity;
    other.m_world_transform = Transform::identity;
    other.m_entity_aabb = BoundingBox::Empty();
    other.m_scene = GetDefaultScene();
    other.m_transform_locked = false;

    const ID<Entity> entity = other.m_entity;
    other.m_entity = ID<Entity>::invalid;
    SetEntity(entity);

    m_child_nodes = std::move(other.m_child_nodes);
    other.m_child_nodes = {};

    m_descendents = std::move(other.m_descendents);
    other.m_descendents = {};

    for (NodeProxy &node : m_child_nodes) {
        AssertThrow(node.IsValid());

        node->m_parent_node = this;
    }
}

Node &Node::operator=(Node &&other) noexcept
{
    RemoveAllChildren();

    SetEntity(ID<Entity>::invalid);
    SetScene(nullptr);

    m_delegates = std::move(other.m_delegates);

    m_type = other.m_type;
    other.m_type = Type::NODE;

    m_flags = other.m_flags;
    other.m_flags = NodeFlags::NONE;

    m_parent_node = other.m_parent_node;
    other.m_parent_node = nullptr;

    m_transform_locked = other.m_transform_locked;
    other.m_transform_locked = false;

    m_local_transform = other.m_local_transform;
    other.m_local_transform = Transform::identity;

    m_world_transform = other.m_world_transform;
    other.m_world_transform = Transform::identity;

    m_entity_aabb = other.m_entity_aabb;
    other.m_entity_aabb = BoundingBox::Empty();

    m_scene = other.m_scene;
    other.m_scene = GetDefaultScene();

    const ID<Entity> entity = other.m_entity;
    other.m_entity = ID<Entity>::invalid;
    SetEntity(entity);

    m_name = std::move(other.m_name);

    m_child_nodes = std::move(other.m_child_nodes);
    other.m_child_nodes = {};

    m_descendents = std::move(other.m_descendents);
    other.m_descendents = {};

    for (NodeProxy &node : m_child_nodes) {
        AssertThrow(node.IsValid());

        node->m_parent_node = this;
    }

    return *this;
}

Node::~Node()
{
    RemoveAllChildren();
    SetEntity(ID<Entity>::invalid);
}

void Node::SetName(const String &name)
{
    if (name.Empty()) {
        m_name = g_unnamed_node_name;
    } else {
        m_name = name;
    }

#ifdef HYP_EDITOR
    EditorDelegates::GetInstance().OnNodeUpdate(this, Class()->GetProperty(NAME("Name")));
#endif
}

bool Node::IsOrHasParent(const Node *node) const
{
    if (node == nullptr) {
        return false;
    }

    if (node == this) {
        return true;
    }

    if (m_parent_node == nullptr) {
        return false;
    }

    return m_parent_node->IsOrHasParent(node);
}

void Node::SetScene(Scene *scene)
{
    if (!scene) {
        scene = g_engine->GetWorld()->GetDetachedScene(Threads::CurrentThreadID()).Get();
    }

    AssertThrow(scene != nullptr);

    if (m_scene != scene) {
        Scene *previous_scene = m_scene;

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(
            previous_scene != nullptr,
            "Previous scene is null when setting new scene for Node %s - should be set to detached world scene by default",
            m_name.Data()
        );
#endif

        m_scene = scene;

#ifdef HYP_EDITOR
        EditorDelegates::GetInstance().OnNodeUpdate(this, Class()->GetProperty(NAME("Scene")));
#endif

        // Move entity from previous scene to new scene
        if (m_entity.IsValid()) {
            if (previous_scene != nullptr && previous_scene->GetEntityManager() != nullptr) {
                AssertThrow(m_scene->GetEntityManager() != nullptr);
                
                previous_scene->GetEntityManager()->MoveEntity(m_entity, *m_scene->GetEntityManager());
            } else {
                // Entity manager null - exiting engine is likely cause here
                m_entity = ID<Entity>::invalid;

#ifdef HYP_EDITOR
                EditorDelegates::GetInstance().OnNodeUpdate(this, Class()->GetProperty(NAME("Entity")));
#endif
            }
        }
    }

    for (NodeProxy &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        child->SetScene(m_scene);
    }
}

void Node::OnNestedNodeAdded(const NodeProxy &node, bool direct)
{
    if (m_delegates) {
        m_delegates->OnNestedNodeAdded(node, direct);
    }

    m_descendents.PushBack(node);
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node, false);
    }
}

void Node::OnNestedNodeRemoved(const NodeProxy &node, bool direct)
{
    if (m_delegates) {
        m_delegates->OnNestedNodeRemoved(node, direct);
    }

    const auto it = m_descendents.Find(node);

    if (it != m_descendents.End()) {
        m_descendents.Erase(it);
    }

    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeRemoved(node, false);
    }
}

NodeProxy Node::AddChild(const NodeProxy &node)
{
    if (!node.IsValid()) {
        return AddChild(NodeProxy(MakeRefCountedPtr<Node>()));
    }

    if (node.Get() == this || node->GetParent() == this) {
        return node;
    }

    if (node->GetParent() != nullptr) {
        HYP_LOG(Node, LogLevel::WARNING, "Attaching node {} to {} when it already has a parent node ({}). Node will be detached from parent.",
            node->GetName(), GetName(), node->GetParent()->GetName());

        AssertThrowMsg(node->Remove(), "Node %s could not be detached from parent", node->GetName().Data());
    }

    AssertThrowMsg(
        !IsOrHasParent(node.Get()),
        "Attaching node %s to %s would create a circular reference",
        node->GetName().Data(),
        GetName().Data()
    );

    m_child_nodes.PushBack(node);
    
    node->m_parent_node = this;
    node->SetScene(m_scene);

    OnNestedNodeAdded(node, true);

    for (NodeProxy &nested : node->GetDescendents()) {
        OnNestedNodeAdded(nested, false);
    }

    node->UpdateWorldTransform();

    return node;
}

bool Node::RemoveChild(NodeList::Iterator iter)
{
    if (iter == m_child_nodes.end()) {
        return false;
    }

    if (NodeProxy &node = *iter) {
        AssertThrow(node.IsValid());
        AssertThrow(node->GetParent() == this);

        for (NodeProxy &nested : node->GetDescendents()) {
            OnNestedNodeRemoved(nested, false);
        }

        OnNestedNodeRemoved(node, true);

        node->m_parent_node = nullptr;
        node->SetScene(nullptr);
    }

    m_child_nodes.Erase(iter);

    UpdateWorldTransform();

    return true;
}

bool Node::RemoveAt(int index)
{
    if (index < 0) {
        index = int(m_child_nodes.Size()) + index;
    }

    if (index >= m_child_nodes.Size()) {
        return false;
    }

    return RemoveChild(m_child_nodes.Begin() + index);
}

bool Node::Remove()
{
    if (m_parent_node == nullptr) {
        return false;
    }

    return m_parent_node->RemoveChild(m_parent_node->FindChild(this));
}

void Node::RemoveAllChildren()
{
    for (auto it = m_child_nodes.begin(); it != m_child_nodes.end();) {
        if (NodeProxy &node = *it) {
            AssertThrow(node.IsValid());
            AssertThrow(node->GetParent() == this);

            for (NodeProxy &nested : node->GetDescendents()) {
                OnNestedNodeRemoved(nested, false);
            }

            OnNestedNodeRemoved(node, true);

            node->m_parent_node = nullptr;
            node->SetScene(nullptr);
        }

        it = m_child_nodes.Erase(it);
    }

    UpdateWorldTransform();
}

NodeProxy Node::GetChild(int index) const
{
    if (index < 0) {
        index = int(m_child_nodes.Size()) + index;
    }

    if (index >= m_child_nodes.Size()) {
        return NodeProxy::empty;
    }

    return m_child_nodes[index];
}

NodeProxy Node::Select(UTF8StringView selector) const
{
    NodeProxy result;

    if (selector.Size() == 0) {
        return result;
    }

    char ch;

    char buffer[256];
    uint32 buffer_index = 0;
    uint32 selector_index = 0;

    const Node *search_node = this;

    const char *str = selector.Data();

    while ((ch = str[selector_index]) != '\0') {
        const char prev_selector_char = selector_index == 0
            ? '\0'
            : str[selector_index - 1];

        ++selector_index;

        if (ch == '/' && prev_selector_char != '\\') {
            const auto it = search_node->FindChild(buffer);

            if (it == search_node->GetChildren().End()) {
                return NodeProxy::empty;
            }

            search_node = it->Get();
            result = *it;

            if (!search_node) {
                return NodeProxy::empty;
            }

            buffer_index = 0;
        } else if (ch != '\\') {
            buffer[buffer_index] = ch;

            ++buffer_index;

            if (buffer_index == std::size(buffer)) {
                HYP_LOG(Node, LogLevel::WARNING, "Node search string too long, must be within buffer size limit of {}",
                    std::size(buffer));

                return NodeProxy::empty;
            }
        }

        buffer[buffer_index] = '\0';
    }

    // find remaining
    if (buffer_index != 0) {
        const auto it = search_node->FindChild(buffer);

        if (it == search_node->GetChildren().End()) {
            return NodeProxy::empty;
        }

        search_node = it->Get();
        result = *it;
    }

    return result;
}

Node::NodeList::Iterator Node::FindChild(const Node *node)
{
    return m_child_nodes.FindIf([node](const auto &it)
    {
        return it.Get() == node;
    });
}

Node::NodeList::ConstIterator Node::FindChild(const Node *node) const
{
    return m_child_nodes.FindIf([node](const auto &it)
    {
        return it.Get() == node;
    });
}

Node::NodeList::Iterator Node::FindChild(const char *name)
{
    return m_child_nodes.FindIf([name](const auto &it)
    {
        return it.GetName() == name;
    });
}

Node::NodeList::ConstIterator Node::FindChild(const char *name) const
{
    return m_child_nodes.FindIf([name](const auto &it)
    {
        return it.GetName() == name;
    });
}

void Node::LockTransform()
{
    m_transform_locked = true;

    // set entity to static
    if (m_entity.IsValid()) {
        if (const RC<EntityManager> &entity_manager = m_scene->GetEntityManager()) {
            entity_manager->AddTag<EntityTag::STATIC>(m_entity);
            entity_manager->RemoveTag<EntityTag::DYNAMIC>(m_entity);
        }
    }

    for (NodeProxy &child : m_child_nodes) {
        if (!child.IsValid()) {
            continue;
        }

        child->LockTransform();
    }
}

void Node::UnlockTransform()
{
    m_transform_locked = false;

    for (NodeProxy &child : m_child_nodes) {
        if (!child.IsValid()) {
            continue;
        }

        child->UnlockTransform();
    }
}

void Node::SetLocalTransform(const Transform &transform)
{
    if (m_transform_locked) {
        return;
    }

    m_local_transform = transform;
    
    UpdateWorldTransform();
}

Transform Node::GetRelativeTransform(const Transform &parent_transform) const
{
    return parent_transform.GetInverse() * m_world_transform;
}

void Node::SetEntity(ID<Entity> entity)
{
    if (m_entity == entity) {
        return;
    }

    // Remove the NodeLinkComponent from the old entity
    if (m_entity.IsValid() && m_scene->GetEntityManager() != nullptr) {
        m_scene->GetEntityManager()->RemoveComponent<NodeLinkComponent>(m_entity);
    }

    if (entity.IsValid() && m_scene->GetEntityManager() != nullptr) {
        m_entity = entity;

#ifdef HYP_EDITOR
        EditorDelegates::GetInstance().OnNodeUpdate(this, Class()->GetProperty(NAME("Entity")));
#endif

        EntityManager *previous_entity_manager = EntityManager::GetEntityToEntityManagerMap().GetEntityManager(m_entity);

        // need to move the entity between EntityManagers
        if (previous_entity_manager != nullptr && previous_entity_manager != m_scene->GetEntityManager().Get()) {
            previous_entity_manager->MoveEntity(m_entity, *m_scene->GetEntityManager());

#ifdef HYP_DEBUG_MODE
            // Sanity check
            AssertThrow(EntityManager::GetEntityToEntityManagerMap().GetEntityManager(m_entity) == m_scene->GetEntityManager().Get());
#endif
        }

        if (TransformComponent *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            SetWorldTransform(transform_component->transform);
        }

        RefreshEntityTransform();

        // set entity to static by default
        m_scene->GetEntityManager()->AddTag<EntityTag::STATIC>(m_entity);
        m_scene->GetEntityManager()->RemoveTag<EntityTag::DYNAMIC>(m_entity);
        
        // Update / add a NodeLinkComponent to the new entity
        if (NodeLinkComponent *node_link_component = m_scene->GetEntityManager()->TryGetComponent<NodeLinkComponent>(m_entity)) {
            node_link_component->node = WeakRefCountedPtrFromThis();
        } else {
            m_scene->GetEntityManager()->AddComponent<NodeLinkComponent>(m_entity, {
                WeakRefCountedPtrFromThis()
            });
        }

        if (!m_scene->GetEntityManager()->HasComponent<VisibilityStateComponent>(m_entity)) {
            m_scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(m_entity, { });
        }
    } else {
        m_entity = ID<Entity>::invalid;

#ifdef HYP_EDITOR
        EditorDelegates::GetInstance().OnNodeUpdate(this, Class()->GetProperty(NAME("Entity")));
#endif

        SetEntityAABB(BoundingBox::Empty());

        UpdateWorldTransform();
    }
}

void Node::SetEntityAABB(const BoundingBox &aabb)
{
    if (m_entity_aabb == aabb) {
        return;
    }

    m_entity_aabb = aabb;

#ifdef HYP_EDITOR
    EditorDelegates::GetInstance().OnNodeUpdate(this, Class()->GetProperty(NAME("EntityAABB")));
    EditorDelegates::GetInstance().OnNodeUpdate(this, Class()->GetProperty(NAME("LocalAABB")));
    EditorDelegates::GetInstance().OnNodeUpdate(this, Class()->GetProperty(NAME("WorldAABB")));
#endif
}

BoundingBox Node::GetLocalAABBExcludingSelf() const
{
    BoundingBox aabb = BoundingBox::Zero();

    for (const NodeProxy &child : GetChildren()) {
        if (!child.IsValid()) {
            continue;
        }

        if (!(child->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB)) {
            aabb.Extend(child->GetLocalAABB() * child->GetLocalTransform());
        }
    }

    return aabb;
}

BoundingBox Node::GetLocalAABB() const
{
    BoundingBox aabb = m_entity_aabb.IsValid() ? m_entity_aabb : BoundingBox::Zero();

    for (const NodeProxy &child : GetChildren()) {
        if (!child.IsValid()) {
            continue;
        }

        if (!(child->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB)) {
            aabb.Extend(child->GetLocalAABB() * child->GetLocalTransform());
        }
    }

    return aabb;
}

BoundingBox Node::GetWorldAABB() const
{
    BoundingBox aabb = m_entity_aabb.IsValid() ? m_entity_aabb : BoundingBox::Zero();
    aabb *= m_world_transform;

    for (const NodeProxy &child : GetChildren()) {
        if (!child.IsValid()) {
            continue;
        }

        if (!(child->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB)) {
            aabb.Extend(child->GetWorldAABB());
        }
    }

    return aabb;
}

void Node::UpdateWorldTransform()
{
    if (m_transform_locked) {
        return;
    }

    if (m_type == Type::BONE) {
        static_cast<Bone *>(this)->UpdateBoneTransform();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    const Transform transform_before = m_world_transform;

    if (m_parent_node != nullptr && !(m_flags & NodeFlags::IGNORE_PARENT_TRANSFORM)) {
        m_world_transform = m_parent_node->GetWorldTransform() * m_local_transform;
    } else if (m_parent_node != nullptr) {
        m_world_transform = m_local_transform;
        
        if (!(m_flags & NodeFlags::IGNORE_PARENT_TRANSLATION)) {
            m_world_transform.GetTranslation() = (m_local_transform.GetTranslation() + m_parent_node->GetWorldTransform().GetTranslation());
        }

        if (!(m_flags & NodeFlags::IGNORE_PARENT_ROTATION)) {
            m_world_transform.GetRotation() = (m_local_transform.GetRotation() * m_parent_node->GetWorldTransform().GetRotation());
        }

        if (!(m_flags & NodeFlags::IGNORE_PARENT_SCALE)) {
            m_world_transform.GetScale() = (m_local_transform.GetScale() * m_parent_node->GetWorldTransform().GetScale());
        }

        m_world_transform.UpdateMatrix();
    } else {
        m_world_transform = m_local_transform;
    }

    const RC<EntityManager> &entity_manager = m_scene->GetEntityManager();

    if (m_entity.IsValid() && entity_manager != nullptr) {
        if (entity_manager->HasTag<EntityTag::STATIC>(m_entity) || !entity_manager->HasTag<EntityTag::DYNAMIC>(m_entity)) {
            entity_manager->AddTag<EntityTag::DYNAMIC>(m_entity);
            entity_manager->RemoveTag<EntityTag::STATIC>(m_entity);
        }

        if (TransformComponent *transform_component = entity_manager->TryGetComponent<TransformComponent>(m_entity)) {
            transform_component->transform = m_world_transform;
        } else {
            entity_manager->AddComponent<TransformComponent>(m_entity, TransformComponent {
                m_world_transform
            });
        }
    }

    if (m_world_transform == transform_before) {
        return;
    }

    for (NodeProxy &node : m_child_nodes) {
        AssertThrow(node != nullptr);
        node->UpdateWorldTransform();
    }

#ifdef HYP_EDITOR
    EditorDelegates::GetInstance().OnNodeUpdate(this, Node::Class()->GetProperty(NAME("LocalTransform")));
    EditorDelegates::GetInstance().OnNodeUpdate(this, Node::Class()->GetProperty(NAME("WorldTransform")));
    EditorDelegates::GetInstance().OnNodeUpdate(this, Node::Class()->GetProperty(NAME("LocalAABB")));
    EditorDelegates::GetInstance().OnNodeUpdate(this, Node::Class()->GetProperty(NAME("WorldAABB")));
#endif
}

void Node::RefreshEntityTransform()
{
    if (m_entity.IsValid() && m_scene->GetEntityManager() != nullptr) {
        if (BoundingBoxComponent *bounding_box_component = m_scene->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(m_entity)) {
            SetEntityAABB(bounding_box_component->local_aabb);
        } else {
            SetEntityAABB(BoundingBox::Empty());
        }

        if (TransformComponent *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            transform_component->transform = m_world_transform;
        } else {
            m_scene->GetEntityManager()->AddComponent<TransformComponent>(m_entity, TransformComponent {
                m_world_transform
            });
        }
    } else {
        SetEntityAABB(BoundingBox::Empty());
    }
}

uint Node::CalculateDepth() const
{
    uint depth = 0;

    Node *parent = m_parent_node;

    while (parent != nullptr) {
        ++depth;
        parent = parent->GetParent();
    }

    return depth;
}

uint Node::FindSelfIndex() const
{
    if (m_parent_node == nullptr) {
        return ~0u;
    }

    const auto it = m_parent_node->FindChild(this);

    if (it == m_parent_node->GetChildren().End()) {
        return 0;
    }

    return uint(it - m_parent_node->GetChildren().Begin());
}

bool Node::TestRay(const Ray &ray, RayTestResults &out_results) const
{
    const BoundingBox world_aabb = GetWorldAABB();

    const bool has_node_hit = ray.TestAABB(world_aabb);

    bool has_entity_hit = false;

    if (has_node_hit) {
        if (m_entity.IsValid()) {
            has_entity_hit = ray.TestAABB(
                world_aabb,
                m_entity.Value(),
                nullptr,
                out_results
            );
        }

        for (const NodeProxy &child_node : m_child_nodes) {
            if (!child_node.IsValid()) {
                continue;
            }

            if (child_node->TestRay(ray, out_results)) {
                has_entity_hit = true;
            }
        }
    }

    return has_entity_hit;
}

NodeProxy Node::FindChildWithEntity(ID<Entity> entity) const
{
    // breadth-first search
    Queue<const Node *> queue;
    queue.Push(this);

    while (queue.Any()) {
        const Node *parent = queue.Pop();

        for (const NodeProxy &child : parent->GetChildren()) {
            if (!child) {
                continue;
            }

            if (child.GetEntity() == entity) {
                return child;
            }

            queue.Push(child.Get());
        }
    }

    return NodeProxy::empty;
}

NodeProxy Node::FindChildByName(UTF8StringView name) const
{
    // breadth-first search
    Queue<const Node *> queue;
    queue.Push(this);

    while (queue.Any()) {
        const Node *parent = queue.Pop();

        for (const NodeProxy &child : parent->GetChildren()) {
            if (!child) {
                continue;
            }

            if (child.GetName() == name) {
                return child;
            }

            queue.Push(child.Get());
        }
    }

    return NodeProxy::empty;
}

NodeProxy Node::FindChildByUUID(const UUID &uuid) const
{
    // breadth-first search
    Queue<const Node *> queue;
    queue.Push(this);

    while (queue.Any()) {
        const Node *parent = queue.Pop();

        for (const NodeProxy &child : parent->GetChildren()) {
            if (!child) {
                continue;
            }

            if (child->GetUUID() == uuid) {
                return child;
            }

            queue.Push(child.Get());
        }
    }

    return NodeProxy::empty;
}

void Node::AddTag(Name key, const NodeTag &value)
{
    m_tags.Set(key, value);
}

bool Node::RemoveTag(Name key)
{
    return m_tags.Erase(key);
}

const NodeTag &Node::GetTag(Name key) const
{
    static const NodeTag empty_tag = NodeTag();

    const auto it = m_tags.Find(key);

    if (it == m_tags.End()) {
        return empty_tag;
    }

    return it->second;
}

bool Node::HasTag(Name key) const
{
    return m_tags.Contains(key);
}

Scene *Node::GetDefaultScene()
{
    return g_engine->GetWorld()->GetDetachedScene(Threads::CurrentThreadID()).Get();
}

#pragma endregion Node

} // namespace hyperion
