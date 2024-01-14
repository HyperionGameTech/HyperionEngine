#include "Node.hpp"
#include <system/Debug.hpp>

#include <scene/animation/Bone.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
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
        local_transform
    )
{
}

Node::Node(
    Type type,
    const String &name,
    ID<Entity> entity,
    const Transform &local_transform
) : m_type(type),
    m_name(name),
    m_parent_node(nullptr),
    m_local_transform(local_transform),
    m_scene(nullptr)
{
    SetEntity(entity);

    // Set the scene to the world's detached scene.
    SetScene(g_engine->GetWorld()->GetDetachedScene().Get());
}

Node::Node(Node &&other) noexcept
    : m_type(other.m_type),
      m_name(std::move(other.m_name)),
      m_parent_node(other.m_parent_node),
      m_local_transform(other.m_local_transform),
      m_world_transform(other.m_world_transform),
      m_local_aabb(other.m_local_aabb),
      m_world_aabb(other.m_world_aabb),
      m_scene(other.m_scene)
{
    other.m_type = Type::NODE;
    other.m_parent_node = nullptr;
    other.m_local_transform = Transform::identity;
    other.m_world_transform = Transform::identity;
    other.m_local_aabb = BoundingBox::empty;
    other.m_world_aabb = BoundingBox::empty;
    other.m_scene = nullptr;

    m_entity = std::move(other.m_entity);

    // if (m_entity) {
    //     m_entity->SetIsAttachedToNode(&other, false);
    //     m_entity->SetIsAttachedToNode(this, true);
    // }

    m_child_nodes = std::move(other.m_child_nodes);
    other.m_child_nodes = {};

    m_descendents = std::move(other.m_descendents);
    other.m_descendents = {};

    for (auto &node : m_child_nodes) {
        AssertThrow(node.Get() != nullptr);

        node.Get()->m_parent_node = this;
    }
}

Node &Node::operator=(Node &&other) noexcept
{
    RemoveAllChildren();

    SetEntity(ID<Entity>::invalid);
    SetScene(nullptr);

    m_type = other.m_type;
    other.m_type = Type::NODE;

    m_parent_node = other.m_parent_node;
    other.m_parent_node = nullptr;

    m_local_transform = other.m_local_transform;
    other.m_local_transform = Transform::identity;

    m_world_transform = other.m_world_transform;
    other.m_world_transform = Transform::identity;

    m_local_aabb = other.m_local_aabb;
    other.m_local_aabb = BoundingBox::empty;

    m_world_aabb = other.m_world_aabb;
    other.m_world_aabb = BoundingBox::empty;

    m_scene = other.m_scene;
    other.m_scene = nullptr;

    m_entity = std::move(other.m_entity);

    if (m_entity) {
        // m_entity->SetIsAttachedToNode(&other, false);
        // m_entity->SetIsAttachedToNode(this, true);
    }

    m_name = std::move(other.m_name);

    m_child_nodes = std::move(other.m_child_nodes);
    other.m_child_nodes = {};

    m_descendents = std::move(other.m_descendents);
    other.m_descendents = {};

    for (auto &node : m_child_nodes) {
        AssertThrow(node.Get() != nullptr);

        node.Get()->m_parent_node = this;
    }

    return *this;
}

Node::~Node()
{
    RemoveAllChildren();
    SetEntity(ID<Entity>::invalid);
}

void Node::SetScene(Scene *scene)
{
    if (m_scene == scene) {
        return;
    }

    if (!scene) {
        scene = g_engine->GetWorld()->GetDetachedScene().Get();
    }

    AssertThrow(scene != nullptr);

    if (m_scene && m_entity.IsValid()) {
        // m_scene->RemoveEntityInternal(m_entity);
        m_scene->GetEntityManager()->MoveEntity(m_entity, *scene->GetEntityManager().Get());
    }

    m_scene = scene;

    if (m_scene && m_entity) {
        // m_scene->AddEntityInternal(Handle<Entity>(m_entity));
    }

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        child.Get()->SetScene(scene);
    }
}

void Node::OnNestedNodeAdded(const NodeProxy &node)
{
    m_descendents.PushBack(node);
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node);
    }
}

void Node::OnNestedNodeRemoved(const NodeProxy &node)
{
    const auto it = m_descendents.Find(node);

    if (it != m_descendents.End()) {
        m_descendents.Erase(it);
    }

    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeRemoved(node);
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

    AssertThrow(node.Get()->m_parent_node == nullptr);

    m_child_nodes.PushBack(std::move(node));

    auto &_node = m_child_nodes.Back();
    _node.Get()->m_parent_node = this;
    _node.Get()->SetScene(m_scene);

    OnNestedNodeAdded(_node);

    for (auto &nested : _node.Get()->GetDescendents()) {
        OnNestedNodeAdded(nested);
    }

    _node.Get()->UpdateWorldTransform();

    return _node;
}

bool Node::RemoveChild(NodeList::Iterator iter)
{
    if (iter == m_child_nodes.end()) {
        return false;
    }

    if (auto &node = *iter) {
        AssertThrow(node.Get() != nullptr);
        AssertThrow(node.Get()->m_parent_node == this);

        for (auto &nested : node.Get()->GetDescendents()) {
            OnNestedNodeRemoved(nested);
        }

        OnNestedNodeRemoved(node);

        node.Get()->m_parent_node = nullptr;
        node.Get()->SetScene(nullptr);
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
            AssertThrow(node.Get() != nullptr);
            AssertThrow(node.Get()->m_parent_node == this);

            for (auto &nested : node.Get()->GetDescendents()) {
                OnNestedNodeRemoved(nested);
            }

            OnNestedNodeRemoved(node);

            node.Get()->m_parent_node = nullptr;
            node.Get()->SetScene(nullptr);
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
    UInt32 buffer_index = 0;
    UInt32 selector_index = 0;

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
    return m_child_nodes.FindIf([node](const auto &it) {
        return it.Get() == node;
    });
}

Node::NodeList::ConstIterator Node::FindChild(const Node *node) const
{
    return m_child_nodes.FindIf([node](const auto &it) {
        return it.Get() == node;
    });
}

Node::NodeList::Iterator Node::FindChild(const char *name)
{
    return m_child_nodes.FindIf([name](const auto &it) {
        return it.GetName() == name;
    });
}

Node::NodeList::ConstIterator Node::FindChild(const char *name) const
{
    return m_child_nodes.FindIf([name](const auto &it) {
        return it.GetName() == name;
    });
}

void Node::SetLocalTransform(const Transform &transform)
{
    m_local_transform = transform;
    
    UpdateWorldTransform();
}

void Node::SetEntity(ID<Entity> entity)
{
    if (m_entity == entity) {
        return;
    }

    if (m_entity.IsValid()) {
        if (m_scene != nullptr) {
            // m_scene->RemoveEntityInternal(m_entity);
        }

        // m_entity->SetIsAttachedToNode(this, false);
    }

    if (entity.IsValid()) {
        m_entity = entity;
        // InitObject(m_entity);

        if (m_scene != nullptr) {
            // m_scene->AddEntityInternal(Handle<Entity>(m_entity));
        }

        // m_entity->SetIsAttachedToNode(this, true);

        Transform entity_transform;

        if (auto *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            entity_transform = transform_component->transform;
        }

        if (auto *bounding_box_component = m_scene->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(m_entity)) {
            m_local_aabb = bounding_box_component->local_aabb;
        } else {
            m_local_aabb = BoundingBox::empty;
        }

        // m_local_aabb = m_entity->GetLocalAABB();

        SetWorldTransform(entity_transform);
    } else {
        m_local_aabb = BoundingBox::empty;

        m_entity = ID<Entity>::invalid;

        UpdateWorldTransform();
    }
}

void Node::UpdateWorldTransform()
{
    if (m_type == Type::BONE) {
        static_cast<Bone *>(this)->UpdateBoneTransform();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    const Transform transform_before = m_world_transform;

    if (m_parent_node != nullptr) {
        m_world_transform = m_parent_node->GetWorldTransform() * m_local_transform;
    } else {
        m_world_transform = m_local_transform;
    }

    if (m_world_transform == transform_before) {
        return;
    }

    m_world_aabb = m_local_aabb * m_world_transform;

    for (auto &node : m_child_nodes) {
        AssertThrow(node.Get() != nullptr);
        node.Get()->UpdateWorldTransform();

        m_world_aabb.Extend(node.Get()->GetWorldAABB());
    }

    if (m_parent_node != nullptr) {
        m_parent_node->m_world_aabb.Extend(m_world_aabb);
    }

    if (m_entity.IsValid()) {
        if (auto *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            transform_component->transform = m_world_transform;
        } else {
            m_scene->GetEntityManager()->AddComponent(m_entity, TransformComponent {
                m_world_transform
            });
        }

        // m_entity->SetTransform(m_world_transform);
    }
}

void Node::RefreshEntityTransform()
{
    m_local_aabb = BoundingBox::empty;

    if (m_entity.IsValid()) {
        if (auto *bounding_box_component = m_scene->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(m_entity)) {
            m_local_aabb = bounding_box_component->local_aabb;
        } else {
            m_local_aabb = BoundingBox::empty;
        }

        if (auto *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            transform_component->transform = m_world_transform;
        } else {
            m_scene->GetEntityManager()->AddComponent(m_entity, TransformComponent {
                m_world_transform
            });
        }

        // m_local_aabb = m_entity->GetLocalAABB();

        // m_entity->SetTransform(m_world_transform);
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

            if (child_node.Get()->TestRay(ray, out_results)) {
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
            if (!child || !child.Get()->GetEntity()) {
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
            if (!child || !child.Get()->GetEntity()) {
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
