/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <scene/Node.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/animation/Bone.hpp>
#include <system/Debug.hpp>
#include <Engine.hpp>

#include <cstring>

namespace hyperion::v2 {

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
        g_engine->GetWorld()->GetDetachedScene(Threads::CurrentThreadID()).Get()
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
    m_name(name),
    m_parent_node(nullptr),
    m_local_transform(local_transform),
    m_scene(scene),
    m_transform_locked(false),
    m_delegates(new Delegates)
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
      m_local_aabb(other.m_local_aabb),
      m_world_aabb(other.m_world_aabb),
      m_scene(other.m_scene),
      m_transform_locked(other.m_transform_locked),
      m_delegates(std::move(other.m_delegates))
{
    other.m_type = Type::NODE;
    other.m_flags = NODE_FLAG_NONE;
    other.m_parent_node = nullptr;
    other.m_local_transform = Transform::identity;
    other.m_world_transform = Transform::identity;
    other.m_local_aabb = BoundingBox::Empty();
    other.m_world_aabb = BoundingBox::Empty();
    other.m_scene = nullptr;
    other.m_transform_locked = false;

    auto entity = other.m_entity;
    other.m_entity = ID<Entity>::invalid;
    SetEntity(entity);

    m_child_nodes = std::move(other.m_child_nodes);
    other.m_child_nodes = {};

    m_descendents = std::move(other.m_descendents);
    other.m_descendents = {};

    for (auto &node : m_child_nodes) {
        AssertThrow(node != nullptr);

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
    other.m_flags = NODE_FLAG_NONE;

    m_parent_node = other.m_parent_node;
    other.m_parent_node = nullptr;

    m_transform_locked = other.m_transform_locked;
    other.m_transform_locked = false;

    m_local_transform = other.m_local_transform;
    other.m_local_transform = Transform::identity;

    m_world_transform = other.m_world_transform;
    other.m_world_transform = Transform::identity;

    m_local_aabb = other.m_local_aabb;
    other.m_local_aabb = BoundingBox::Empty();

    m_world_aabb = other.m_world_aabb;
    other.m_world_aabb = BoundingBox::Empty();

    m_scene = other.m_scene;
    other.m_scene = nullptr;

    ID<Entity> entity = other.m_entity;
    other.m_entity = ID<Entity>::invalid;
    SetEntity(entity);

    m_name = std::move(other.m_name);

    m_child_nodes = std::move(other.m_child_nodes);
    other.m_child_nodes = {};

    m_descendents = std::move(other.m_descendents);
    other.m_descendents = {};

    for (auto &node : m_child_nodes) {
        AssertThrow(node != nullptr);

        node->m_parent_node = this;
    }

    return *this;
}

Node::~Node()
{
    RemoveAllChildren();
    SetEntity(ID<Entity>::invalid);
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
    if (m_scene == scene) {
        return;
    }

    if (!scene) {
        scene = g_engine->GetWorld()->GetDetachedScene(Threads::CurrentThreadID()).Get();
    }

    AssertThrow(scene != nullptr);

    if (m_scene && m_entity.IsValid()) {
        m_scene->GetEntityManager()->MoveEntity(m_entity, *scene->GetEntityManager().Get());
    }

    m_scene = scene;

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        child->SetScene(scene);
    }
}

void Node::OnNestedNodeAdded(const NodeProxy &node, bool direct)
{
    if (m_delegates) {
        m_delegates->OnNestedNodeAdded.Broadcast(node, direct);
    }

    m_descendents.PushBack(node);
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node, false);
    }
}

void Node::OnNestedNodeRemoved(const NodeProxy &node, bool direct)
{
    if (m_delegates) {
        m_delegates->OnNestedNodeRemoved.Broadcast(node, direct);
    }

    const auto it = m_descendents.Find(node);

    if (it != m_descendents.End()) {
        m_descendents.Erase(it);
    }

    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeRemoved(node, false);
    }
}

NodeProxy Node::AddChild()
{
    return AddChild(NodeProxy(new Node()));
}

NodeProxy Node::AddChild(const NodeProxy &node)
{
    if (!node) {
        // could not be added... return empty.
        return NodeProxy::empty;
    }

    AssertThrowMsg(
        node->GetParent() == nullptr,
        "Cannot attach a child node that already has a parent"
    );

    m_child_nodes.PushBack(std::move(node));

    auto &_node = m_child_nodes.Back();
    _node->m_parent_node = this;
    _node->SetScene(m_scene);

    OnNestedNodeAdded(_node, true);

    for (auto &nested : _node->GetDescendents()) {
        OnNestedNodeAdded(nested, false);
    }

    _node->UpdateWorldTransform();

    return _node;
}

bool Node::RemoveChild(NodeList::Iterator iter)
{
    if (iter == m_child_nodes.end()) {
        return false;
    }

    if (auto &node = *iter) {
        AssertThrow(node != nullptr);
        AssertThrow(node->GetParent() == this);

        for (auto &nested : node->GetDescendents()) {
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

bool Node::RemoveChild(SizeType index)
{
    if (index >= m_child_nodes.Size()) {
        return false;
    }

    return RemoveChild(m_child_nodes.begin() + index);
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
        if (auto &node = *it) {
            AssertThrow(node != nullptr);
            AssertThrow(node->GetParent() == this);

            for (auto &nested : node->GetDescendents()) {
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

NodeProxy Node::GetChild(SizeType index) const
{
    if (index >= m_child_nodes.Size()) {
        return NodeProxy::empty;
    }

    return m_child_nodes[index];
}

NodeProxy Node::Select(const char *selector) const
{
    NodeProxy result;

    if (selector == nullptr) {
        return result;
    }

    char ch;

    char buffer[256];
    uint32 buffer_index = 0;
    uint32 selector_index = 0;

    const Node *search_node = this;

    while ((ch = selector[selector_index]) != '\0') {
        const char prev_selector_char = selector_index == 0
            ? '\0'
            : selector[selector_index - 1];

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
                DebugLog(
                    LogType::Warn,
                    "Node search string too long, must be within buffer size limit of %llu\n",
                    std::size(buffer)
                );

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
        m_scene->GetEntityManager()->AddTag<EntityTag::STATIC>(m_entity);
        m_scene->GetEntityManager()->RemoveTag<EntityTag::DYNAMIC>(m_entity);
    }

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        child->LockTransform();
    }
}

void Node::UnlockTransform()
{
    m_transform_locked = false;

    for (auto &child : m_child_nodes) {
        if (!child) {
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

void Node::SetEntity(ID<Entity> entity)
{
    if (m_entity == entity) {
        return;
    }

    // Remove the NodeLinkComponent from the old entity
    if (m_entity.IsValid()) {
        m_scene->GetEntityManager()->RemoveComponent<NodeLinkComponent>(m_entity);
    }

    if (entity.IsValid()) {
        m_entity = entity;

        if (auto *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            SetWorldTransform(transform_component->transform);
        }

        if (auto *bounding_box_component = m_scene->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(m_entity)) {
            m_local_aabb = bounding_box_component->local_aabb;
        } else {
            m_local_aabb = BoundingBox::Empty();
        }

        RefreshEntityTransform();

        // set entity to static by default
        m_scene->GetEntityManager()->AddTag<EntityTag::STATIC>(m_entity);
        m_scene->GetEntityManager()->RemoveTag<EntityTag::DYNAMIC>(m_entity);
        
        // Update / add a NodeLinkComponent to the new entity
        if (auto *node_link_component = m_scene->GetEntityManager()->TryGetComponent<NodeLinkComponent>(m_entity)) {
            node_link_component->node = WeakRefCountedPtrFromThis();
        } else {
            m_scene->GetEntityManager()->AddComponent(m_entity, NodeLinkComponent {
                WeakRefCountedPtrFromThis()
            });
        }
    } else {
        m_local_aabb = BoundingBox::Empty();

        m_entity = ID<Entity>::invalid;

        UpdateWorldTransform();
    }
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

    if (m_parent_node != nullptr && !(m_flags & NODE_FLAG_IGNORE_PARENT_TRANSFORM)) {
        m_world_transform = m_parent_node->GetWorldTransform() * m_local_transform;
    } else if (m_parent_node != nullptr) {
        m_world_transform = m_local_transform;
        
        if (!(m_flags & NODE_FLAG_IGNORE_PARENT_TRANSLATION)) {
            m_world_transform.GetTranslation() = (m_local_transform.GetTranslation() + m_parent_node->GetWorldTransform().GetTranslation());
        }

        if (!(m_flags & NODE_FLAG_IGNORE_PARENT_ROTATION)) {
            m_world_transform.GetRotation() = (m_local_transform.GetRotation() * m_parent_node->GetWorldTransform().GetRotation());
        }

        if (!(m_flags & NODE_FLAG_IGNORE_PARENT_SCALE)) {
            m_world_transform.GetScale() = (m_local_transform.GetScale() * m_parent_node->GetWorldTransform().GetScale());
        }

        m_world_transform.UpdateMatrix();
    } else {
        m_world_transform = m_local_transform;
    }

    if (m_world_transform == transform_before) {
        return;
    }

    m_world_aabb = m_local_aabb * m_world_transform;

    for (auto &node : m_child_nodes) {
        AssertThrow(node != nullptr);
        node->UpdateWorldTransform();

        m_world_aabb.Extend(node->GetWorldAABB());
    }

    if (m_parent_node != nullptr) {
        m_parent_node->m_world_aabb.Extend(m_world_aabb);
    }

    if (m_entity.IsValid()) {
        m_scene->GetEntityManager()->AddTag<EntityTag::DYNAMIC>(m_entity);
        m_scene->GetEntityManager()->RemoveTag<EntityTag::STATIC>(m_entity);

        if (auto *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            transform_component->transform = m_world_transform;
        } else {
            m_scene->GetEntityManager()->AddComponent(m_entity, TransformComponent {
                m_world_transform
            });
        }
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

void Node::RefreshEntityTransform()
{
    m_local_aabb = BoundingBox::Empty();

    if (m_entity.IsValid()) {
        if (auto *bounding_box_component = m_scene->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(m_entity)) {
            m_local_aabb = bounding_box_component->local_aabb;
        } else {
            m_local_aabb = BoundingBox::Empty();
        }

        if (auto *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            transform_component->transform = m_world_transform;
        } else {
            m_scene->GetEntityManager()->AddComponent(m_entity, TransformComponent {
                m_world_transform
            });
        }
    }

    m_world_aabb = m_local_aabb * m_world_transform;
}

bool Node::TestRay(const Ray &ray, RayTestResults &out_results) const
{
    const bool has_node_hit = ray.TestAABB(m_world_aabb);

    bool has_entity_hit = false;

    if (has_node_hit) {
        if (m_entity.IsValid()) {
            has_entity_hit = ray.TestAABB(
                m_world_aabb,
                m_entity.Value(),
                nullptr,
                out_results
            );
        }

        for (auto &child_node : m_child_nodes) {
            if (!child_node) {
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
            if (!child || !child->GetEntity().IsValid()) {
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

NodeProxy Node::FindChildByName(const String &name) const
{
    // breadth-first search
    Queue<const Node *> queue;
    queue.Push(this);

    while (queue.Any()) {
        const Node *parent = queue.Pop();

        for (const NodeProxy &child : parent->GetChildren()) {
            if (!child || !child->GetEntity().IsValid()) {
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

} // namespace hyperion::v2
