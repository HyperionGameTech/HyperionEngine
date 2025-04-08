/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Node.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <scene/animation/Bone.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/BVHComponent.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#endif

#include <Engine.hpp>

#include <cstring>

namespace hyperion {

#pragma region NodeTag

#pragma endregion NodeTag

#pragma region Node

const String Node::s_unnamed_node_name = "<unnamed>";

// @NOTE: In some places we have a m_scene->GetEntityManager() != nullptr check,
// this only happens in the case that the scene in question is destructing and
// this Node is held on a component that the EntityManager has.
// In practice it only really shows up on UI objects where UIObject holds a reference to a Node.

Node::Node(
    const String &name,
    const Transform &local_transform
) : Node(
        name,
        Handle<Entity>::empty,
        local_transform
    )
{
}

Node::Node(
    const String &name,
    const Handle<Entity> &entity,
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
    const Handle<Entity> &entity,
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
    const Handle<Entity> &entity,
    const Transform &local_transform,
    Scene *scene
) : m_type(type),
    m_name(name.Empty() ? s_unnamed_node_name : name),
    m_parent_node(nullptr),
    m_local_transform(local_transform),
    m_scene(scene != nullptr ? scene : GetDefaultScene()),
    m_transform_locked(false),
    m_transform_changed(false),
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
      m_transform_changed(other.m_transform_changed),
      m_delegates(std::move(other.m_delegates))
{
    other.m_type = Type::NODE;
    other.m_flags = NodeFlags::NONE;
    other.m_name = s_unnamed_node_name;
    other.m_parent_node = nullptr;
    other.m_local_transform = Transform::identity;
    other.m_world_transform = Transform::identity;
    other.m_entity_aabb = BoundingBox::Empty();
    other.m_scene = GetDefaultScene();
    other.m_transform_locked = false;
    other.m_transform_changed = false;

    Handle<Entity> entity = other.m_entity;
    other.m_entity = Handle<Entity>::empty;
    SetEntity(entity);

    m_child_nodes = std::move(other.m_child_nodes);
    m_descendants = std::move(other.m_descendants);

    for (NodeProxy &node : m_child_nodes) {
        AssertThrow(node.IsValid());

        node->m_parent_node = this;
    }
}

Node &Node::operator=(Node &&other) noexcept
{
    RemoveAllChildren();

    SetEntity(Handle<Entity>::empty);
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

    m_transform_changed = other.m_transform_changed;
    other.m_transform_changed = false;

    m_local_transform = other.m_local_transform;
    other.m_local_transform = Transform::identity;

    m_world_transform = other.m_world_transform;
    other.m_world_transform = Transform::identity;

    m_entity_aabb = other.m_entity_aabb;
    other.m_entity_aabb = BoundingBox::Empty();

    m_scene = other.m_scene;
    other.m_scene = GetDefaultScene();

    Handle<Entity> entity = other.m_entity;
    other.m_entity = Handle<Entity>::empty;
    SetEntity(entity);

    m_name = std::move(other.m_name);

    m_child_nodes = std::move(other.m_child_nodes);
    m_descendants = std::move(other.m_descendants);

    for (NodeProxy &node : m_child_nodes) {
        AssertThrow(node.IsValid());

        node->m_parent_node = this;
    }

    return *this;
}

Node::~Node()
{
    RemoveAllChildren();
    SetEntity(Handle<Entity>::empty);
}

void Node::SetName(const String &name)
{
    if (name.Empty()) {
        m_name = s_unnamed_node_name;
    } else {
        m_name = name;
    }

#ifdef HYP_EDITOR
    GetEditorDelegates([this](EditorDelegates *editor_delegates)
    {
        editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Name")));
    });
#endif
}

bool Node::HasName() const
{
    return m_name.Any() && m_name != s_unnamed_node_name;
}

void Node::SetFlags(EnumFlags<NodeFlags> flags)
{
    if (m_flags == flags) {
        return;
    }

    if ((flags & NodeFlags::BUILD_BVH) && !(m_flags & NodeFlags::BUILD_BVH)) {
        EnsureEntityHasBVHComponent();
    }

    m_flags = flags;

#ifdef HYP_EDITOR
    GetEditorDelegates([this](EditorDelegates *editor_delegates)
    {
        editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Flags")));
    });
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

Node *Node::FindParentWithName(UTF8StringView name) const
{
    if (m_name == name) {
        return const_cast<Node *>(this);
    }

    if (m_parent_node == nullptr) {
        return nullptr;
    }

    return m_parent_node->FindParentWithName(name);
}

void Node::SetScene(Scene *scene)
{
    if (!scene) {
        scene = g_engine->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadID()).Get();
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
        GetEditorDelegates([this](EditorDelegates *editor_delegates)
        {
            editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Scene")));
        });
#endif

        // Move entity from previous scene to new scene
        if (m_entity.IsValid()) {
            if (previous_scene != nullptr && previous_scene->GetEntityManager() != nullptr) {
                AssertThrow(m_scene->GetEntityManager() != nullptr);
                
                previous_scene->GetEntityManager()->MoveEntity(m_entity, *m_scene->GetEntityManager());
            } else {
                // Entity manager null - exiting engine is likely cause here

                // Unset the entity
                m_entity.Reset();

#ifdef HYP_EDITOR
                GetEditorDelegates([this](EditorDelegates *editor_delegates)
                {
                    editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Entity")));
                });
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

World *Node::GetWorld() const
{
    return m_scene != nullptr
        ? m_scene->GetWorld()
        : g_engine->GetDefaultWorld().Get();
}

void Node::OnNestedNodeAdded(Node *node, bool direct)
{
    m_descendants.PushBack(node);
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node, false);
    }
}

void Node::OnNestedNodeRemoved(Node *node, bool direct)
{
    const auto it = m_descendants.Find(node);

    if (it != m_descendants.End()) {
        m_descendants.Erase(it);
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
        HYP_LOG(Node, Warning, "Attaching node {} to {} when it already has a parent node ({}). Node will be detached from parent.",
            node->GetName(), GetName(), node->GetParent()->GetName());

        node->Remove();
    }

    AssertThrowMsg(
        !IsOrHasParent(node.Get()),
        "Attaching node %s to %s would create a circular reference",
        node->GetName().Data(),
        GetName().Data()
    );

    m_child_nodes.PushBack(node);

    bool was_transform_locked = false;

    if (node->m_transform_locked) {
        was_transform_locked = true;

        node->UnlockTransform();
    }
    
    node->m_parent_node = this;
    node->SetScene(m_scene);
    node->UpdateWorldTransform();

    if (was_transform_locked) {
        node->LockTransform();
    }

    Node *current_parent = this;

    while (current_parent != nullptr && current_parent->m_delegates != nullptr) {
        current_parent->m_delegates->OnChildAdded(node, /* direct */ current_parent == this);

        current_parent = current_parent->m_parent_node;
    }

    OnNestedNodeAdded(node, true);

    for (Node *nested : node->GetDescendants()) {
        OnNestedNodeAdded(nested, false);
    }

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

        bool was_transform_locked = false;

        if (node->m_transform_locked) {
            was_transform_locked = true;

            node->UnlockTransform();
        }

        node->m_parent_node = nullptr;
        node->SetScene(nullptr);
        node->UpdateWorldTransform();

        if (was_transform_locked) {
            node->LockTransform();
        }

        Node *current_parent = this;

        while (current_parent != nullptr && current_parent->m_delegates != nullptr) {
            current_parent->m_delegates->OnChildRemoved(node, /* direct */ current_parent == this);

            current_parent = current_parent->m_parent_node;
        }

        for (Node *nested : node->GetDescendants()) {
            OnNestedNodeRemoved(nested, false);
        }

        OnNestedNodeRemoved(node, true);
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

void Node::Remove()
{
    if (!m_parent_node) {
        SetScene(nullptr);

        return;
    }

    auto it = m_parent_node->FindChild(this);
    AssertDebug(it != m_parent_node->GetChildren().End());

    m_parent_node->RemoveChild(it);
}

void Node::RemoveAllChildren()
{
    for (auto it = m_child_nodes.begin(); it != m_child_nodes.end();) {
        if (NodeProxy &node = *it) {
            AssertThrow(node.IsValid());
            AssertThrow(node->GetParent() == this);

            Node *current_parent = this;

            while (current_parent != nullptr && current_parent->m_delegates != nullptr) {
                current_parent->m_delegates->OnChildRemoved(node, /* direct */ current_parent == this);

                current_parent = current_parent->m_parent_node;
            }

            for (Node *nested : node->GetDescendants()) {
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
                HYP_LOG(Node, Warning, "Node search string too long, must be within buffer size limit of {}",
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

        m_transform_changed = false;
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

    if (m_local_transform == transform) {
        return;
    }

    m_local_transform = transform;
    
    UpdateWorldTransform();
}

Transform Node::GetRelativeTransform(const Transform &parent_transform) const
{
    return parent_transform.GetInverse() * m_world_transform;
}

void Node::SetEntity(const Handle<Entity> &entity)
{
    if (m_entity == entity) {
        return;
    }

    // Remove the NodeLinkComponent from the old entity
    if (m_entity.IsValid() && m_scene != nullptr && m_scene->GetEntityManager() != nullptr) {
        // For destructor call.
        // Ensure that we remove the NodeLinkComponent from the entity on the correct thread.
        if (Threads::IsOnThread(m_scene->GetEntityManager()->GetOwnerThreadID())) {
            m_scene->GetEntityManager()->RemoveComponent<NodeLinkComponent>(m_entity);

            // Move entity to detached scene
            m_scene->GetEntityManager()->MoveEntity(m_entity, *GetDefaultScene()->GetEntityManager());
        } else {
            Task<void> task = Threads::GetThread(m_scene->GetOwnerThreadID())->GetScheduler().Enqueue(
                HYP_STATIC_MESSAGE("Remove NodeLinkComponent from entity"),
                [entity = m_entity, scene = m_scene->HandleFromThis()]()
                {
                    scene->GetEntityManager()->RemoveComponent<NodeLinkComponent>(entity);

                    // Move entity to detached scene
                    scene->GetEntityManager()->MoveEntity(entity, *GetDefaultScene()->GetEntityManager());
                }
            );

            task.Await();
        }
    }

    if (entity.IsValid() && m_scene != nullptr && m_scene->GetEntityManager() != nullptr) {
        m_entity = entity;

        if (m_flags & NodeFlags::BUILD_BVH) {
            EnsureEntityHasBVHComponent();
        }

#ifdef HYP_EDITOR
        GetEditorDelegates([this](EditorDelegates *editor_delegates)
        {
            editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Entity")));
        });
#endif

        EntityManager *previous_entity_manager = EntityManager::GetEntityToEntityManagerMap().GetEntityManager(m_entity);
        AssertThrow(previous_entity_manager != nullptr);

        // need to move the entity between EntityManagers
        if (previous_entity_manager != nullptr && previous_entity_manager != m_scene->GetEntityManager().Get()) {
            previous_entity_manager->MoveEntity(m_entity, *m_scene->GetEntityManager());

#ifdef HYP_DEBUG_MODE
            // Sanity check
            AssertThrow(EntityManager::GetEntityToEntityManagerMap().GetEntityManager(m_entity) == m_scene->GetEntityManager().Get());
#endif
        }

        // If a TransformComponent already exists on the Entity, allow it to keep its current transform by moving the Node
        // to match it, as long as we're not locked
        // If transform is locked, the Entity's TransformComponent will be synced with the Node's current transform
        if (TransformComponent *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
            if (!IsTransformLocked()) {
                SetWorldTransform(transform_component->transform);
            }
        }

        RefreshEntityTransform();

        // set entity to static by default
        m_scene->GetEntityManager()->AddTag<EntityTag::STATIC>(m_entity);
        m_scene->GetEntityManager()->RemoveTag<EntityTag::DYNAMIC>(m_entity);

        // set transform_changed to false until entity is set to DYNAMIC
        m_transform_changed = false;
        
        // Update / add a NodeLinkComponent to the new entity
        if (NodeLinkComponent *node_link_component = m_scene->GetEntityManager()->TryGetComponent<NodeLinkComponent>(m_entity)) {
            node_link_component->node = WeakRefCountedPtrFromThis();
        } else {
            m_scene->GetEntityManager()->AddComponent<NodeLinkComponent>(m_entity, { WeakRefCountedPtrFromThis() });
        }

        if (!m_scene->GetEntityManager()->HasComponent<VisibilityStateComponent>(m_entity)) {
            m_scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(m_entity, { });
        }
    } else {
        m_entity = Handle<Entity>::empty;

        m_transform_changed = false;

#ifdef HYP_EDITOR
        GetEditorDelegates([this](EditorDelegates *editor_delegates)
        {
            editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("Entity")));
        });
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
    GetEditorDelegates([this](EditorDelegates *editor_delegates)
    {
        editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("EntityAABB")));
        editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("LocalAABB")));
        editor_delegates->OnNodeUpdate(this, Class()->GetProperty(NAME("WorldAABB")));
    });
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
            aabb = aabb.Union(child->GetLocalTransform() * child->GetLocalAABB());
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
            aabb = aabb.Union(child->GetLocalTransform() * child->GetLocalAABB());
        }
    }

    return aabb;
}

BoundingBox Node::GetWorldAABB() const
{
    BoundingBox aabb = m_world_transform * (m_entity_aabb.IsValid() ? m_entity_aabb : BoundingBox::Zero());

    for (const NodeProxy &child : GetChildren()) {
        if (!child.IsValid()) {
            continue;
        }

        if (!(child->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB)) {
            aabb = aabb.Union(child->GetWorldAABB());
        }
    }

    return aabb;
}

void Node::UpdateWorldTransform(bool update_child_transforms)
{
    if (m_transform_locked) {
        return;
    }

    if (m_type == Type::BONE) {
        static_cast<Bone *>(this)->UpdateBoneTransform();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    const Transform transform_before = m_world_transform;

    Transform world_transform = m_local_transform;

    if (m_parent_node != nullptr) {
        world_transform = m_parent_node->GetWorldTransform() * m_local_transform;
        
        if (m_flags & NodeFlags::IGNORE_PARENT_TRANSFORM) {
            if (m_flags & NodeFlags::IGNORE_PARENT_TRANSLATION) {
                world_transform.GetTranslation() = m_local_transform.GetTranslation();
            }
    
            if (m_flags & NodeFlags::IGNORE_PARENT_ROTATION) {
                world_transform.GetRotation() = m_local_transform.GetRotation();
            }
    
            if (m_flags & NodeFlags::IGNORE_PARENT_SCALE) {
                world_transform.GetScale() = m_local_transform.GetScale();
            }
        }
    }

    m_world_transform = world_transform;

    if (m_world_transform == transform_before) {
        return;
    }

    if (m_entity.IsValid()) {
        const RC<EntityManager> &entity_manager = m_scene->GetEntityManager();

        if (!m_transform_changed) {
            // Set to dynamic
            if (entity_manager != nullptr) {
                entity_manager->AddTag<EntityTag::DYNAMIC>(m_entity);
                entity_manager->RemoveTag<EntityTag::STATIC>(m_entity);
            }

            m_transform_changed = true;
        }

        if (entity_manager != nullptr) {
            if (TransformComponent *transform_component = entity_manager->TryGetComponent<TransformComponent>(m_entity)) {
                transform_component->transform = m_world_transform;
            } else {
                entity_manager->AddComponent<TransformComponent>(m_entity, TransformComponent { m_world_transform });
            }

            entity_manager->AddTags<
                EntityTag::UPDATE_AABB,
                EntityTag::UPDATE_LIGHT_TRANSFORM,
                EntityTag::UPDATE_CAMERA_TRANSFORM,
                EntityTag::UPDATE_ENV_GRID_TRANSFORM
            >(m_entity);
        }
    }

    if (update_child_transforms) {
        for (NodeProxy &node : m_child_nodes) {
            node->UpdateWorldTransform(true);
        }
    }

#ifdef HYP_EDITOR
    GetEditorDelegates([this](EditorDelegates *editor_delegates)
    {
        editor_delegates->OnNodeUpdate(this, Node::Class()->GetProperty(NAME("LocalTransform")));
        editor_delegates->OnNodeUpdate(this, Node::Class()->GetProperty(NAME("WorldTransform")));
        editor_delegates->OnNodeUpdate(this, Node::Class()->GetProperty(NAME("LocalAABB")));
        editor_delegates->OnNodeUpdate(this, Node::Class()->GetProperty(NAME("WorldAABB")));
    });
#endif
}

void Node::RefreshEntityTransform()
{
    if (m_entity.IsValid() && m_scene != nullptr && m_scene->GetEntityManager() != nullptr) {
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

        m_scene->GetEntityManager()->AddTags<
            EntityTag::UPDATE_AABB,
            EntityTag::UPDATE_LIGHT_TRANSFORM,
            EntityTag::UPDATE_CAMERA_TRANSFORM,
            EntityTag::UPDATE_ENV_GRID_TRANSFORM
        >(m_entity);
    } else {
        SetEntityAABB(BoundingBox::Empty());
    }
}

void Node::EnsureEntityHasBVHComponent()
{
    if (!m_entity.IsValid()) {
        return;
    }

    if (!m_scene) {
        return;
    }

    if (!m_scene->GetEntityManager()) {
        return;
    }

    if (m_scene->GetEntityManager()->HasComponent<BVHComponent>(m_entity)) {
        return;
    }

    m_scene->GetEntityManager()->AddComponent<BVHComponent>(m_entity, BVHComponent { });
}

uint32 Node::CalculateDepth() const
{
    uint32 depth = 0;

    Node *parent = m_parent_node;

    while (parent != nullptr) {
        ++depth;
        parent = parent->GetParent();
    }

    return depth;
}

uint32 Node::FindSelfIndex() const
{
    if (m_parent_node == nullptr) {
        return ~0u;
    }

    const auto it = m_parent_node->FindChild(this);

    if (it == m_parent_node->GetChildren().End()) {
        return 0;
    }

    return uint32(it - m_parent_node->GetChildren().Begin());
}

bool Node::TestRay(const Ray &ray, RayTestResults &out_results, bool use_bvh) const
{
    const BoundingBox world_aabb = GetWorldAABB();

    bool has_entity_hit = false;

    if (ray.TestAABB(world_aabb)) {
        if (m_entity.IsValid()) {
            BVHNode *bvh = nullptr;
            Matrix4 model_matrix = Matrix4::Identity();

            if (use_bvh && m_scene && m_scene->GetEntityManager()) {
                if (BVHComponent *bvh_component = m_scene->GetEntityManager()->TryGetComponent<BVHComponent>(m_entity.GetID())) {
                    bvh = &bvh_component->bvh;
                }
                
                if (TransformComponent *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity.GetID())) {
                    model_matrix = transform_component->transform.GetMatrix();
                }
            }

            if (bvh) {
                const Ray local_space_ray = model_matrix.Inverted() * ray;

                RayTestResults local_bvh_results = bvh->TestRay(local_space_ray);

                if (local_bvh_results.Any()) {
                    const Matrix4 normal_matrix = model_matrix.Transposed().Inverted();

                    RayTestResults bvh_results;

                    for (RayHit hit : local_bvh_results) {
                        hit.id = m_entity.GetID().Value();
                        hit.user_data = nullptr;
                        
                        Vec4f transformed_normal = normal_matrix * Vec4f(hit.normal, 0.0f);
                        hit.normal = transformed_normal.GetXYZ().Normalized();

                        Vec4f transformed_position = model_matrix * Vec4f(hit.hitpoint, 1.0f);
                        transformed_position /= transformed_position.w;

                        hit.hitpoint = transformed_position.GetXYZ();

                        hit.distance = (hit.hitpoint - ray.position).Length();

                        bvh_results.AddHit(hit);
                    }

                    out_results.Merge(std::move(bvh_results));

                    has_entity_hit = true;
                }
            } else {
                has_entity_hit = ray.TestAABB(world_aabb, m_entity.GetID().Value(), nullptr, out_results);
            }
        }

        for (const NodeProxy &child_node : m_child_nodes) {
            if (!child_node.IsValid()) {
                continue;
            }

            if (child_node->TestRay(ray, out_results, use_bvh)) {
                has_entity_hit = true;
            }
        }
    }

    return has_entity_hit;
}

NodeProxy Node::FindChildWithEntity(ID<Entity> entity_id) const
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

            if (child.GetEntity().GetID() == entity_id) {
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

void Node::AddTag(NodeTag &&value)
{
    HYP_SCOPE;

    m_tags.Set(std::move(value));
}

bool Node::RemoveTag(WeakName key)
{
    HYP_SCOPE;

    auto it = m_tags.FindAs(key);

    if (it == m_tags.End()) {
        return false;
    }

    m_tags.Erase(it);

    return true;
}

const NodeTag &Node::GetTag(WeakName key) const
{
    HYP_SCOPE;

    static const NodeTag empty_tag = NodeTag();

    const auto it = m_tags.FindAs(key);

    if (it == m_tags.End()) {
        return empty_tag;
    }

    return *it;
}

bool Node::HasTag(WeakName key) const
{
    HYP_SCOPE;

    return m_tags.FindAs(key) != m_tags.End();
}

Scene *Node::GetDefaultScene()
{
    return g_engine->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadID()).Get();
}

#ifdef HYP_EDITOR

EditorDelegates *Node::GetEditorDelegates()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    if (EditorSubsystem *editor_subsystem = g_engine->GetDefaultWorld()->GetSubsystem<EditorSubsystem>()) {
        return editor_subsystem->GetEditorDelegates();
    }

    return nullptr;
}

template <class Function>
void Node::GetEditorDelegates(Function &&func)
{
    if (Threads::IsOnThread(g_game_thread)) {
        if (EditorSubsystem *editor_subsystem = g_engine->GetDefaultWorld()->GetSubsystem<EditorSubsystem>()) {
            func(editor_subsystem->GetEditorDelegates());
        }
    } else {
        Threads::GetThread(g_game_thread)->GetScheduler().Enqueue([weak_this = WeakRefCountedPtrFromThis(), func = std::forward<Function>(func)]()
        {
            if (weak_this.Lock()) {
                if (EditorSubsystem *editor_subsystem = g_engine->GetDefaultWorld()->GetSubsystem<EditorSubsystem>()) {
                    func(editor_subsystem->GetEditorDelegates());
                }
            }
        }, TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

#endif

#pragma endregion Node

} // namespace hyperion
