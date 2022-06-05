#include "node.h"
#include <system/debug.h>

#include <animation/bone.h>
#include <engine.h>

#include <cstring>

namespace hyperion::v2 {

Node::Node(
    const char *tag,
    const Transform &local_transform
) : Node(
        tag,
        nullptr,
        local_transform
    )
{
}

Node::Node(
    const char *tag,
    Ref<Spatial> &&spatial,
    const Transform &local_transform
) : Node(
        Type::NODE,
        tag,
        std::move(spatial),
        local_transform
    )
{
}

Node::Node(
    Type type,
    const char *tag,
    Ref<Spatial> &&spatial,
    const Transform &local_transform
) : m_type(type),
    m_parent_node(nullptr),
    m_local_transform(local_transform),
    m_scene(nullptr)
{
    SetSpatial(std::move(spatial));

    const size_t len = std::strlen(tag);
    m_tag = new char[len + 1];
    std::strcpy(m_tag, tag);
}

Node::~Node()
{
    delete[] m_tag;
}

void Node::SetTag(const char *tag)
{
    if (tag == m_tag || !std::strcmp(tag, m_tag)) {
        return;
    }

    delete[] m_tag;

    const size_t len = std::strlen(tag);
    m_tag = new char[len + 1];
    std::strcpy(m_tag, tag);
}

void Node::SetScene(Scene *scene)
{
    m_scene = scene;

    for (auto &child : m_child_nodes) {
        if (child == nullptr) {
            continue;
        }

        child->SetScene(scene);
    }
}

void Node::OnNestedNodeAdded(Node *node)
{
    m_descendents.push_back(node);

    for (auto &controller : m_controllers) {
        if (controller.second == nullptr) {
            DebugLog(
                LogType::Warn,
                "Controller was nullptr\n"
            );

            continue;
        }

        controller.second->OnDescendentAdded(node);
    }
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node);
    }
}

void Node::OnNestedNodeRemoved(Node *node)
{
    const auto it = std::find(m_descendents.begin(), m_descendents.end(), node);

    if (it != m_descendents.end()) {
        for (auto &controller : m_controllers) {
            if (controller.second == nullptr) {
                DebugLog(
                    LogType::Warn,
                    "Controller was nullptr\n"
                );

                continue;
            }

            controller.second->OnDescendentRemoved(node);
        }

        m_descendents.erase(it);
    }

    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeRemoved(node);
    }
}

Node *Node::AddChild()
{
    return AddChild(std::make_unique<Node>());
}

Node *Node::AddChild(std::unique_ptr<Node> &&node)
{
    AssertThrow(node != nullptr);
    AssertThrow(node->m_parent_node == nullptr);

    node->m_parent_node = this;
    node->SetScene(m_scene);

    OnNestedNodeAdded(node.get());

    for (Node *nested : node->GetDescendents()) {
        OnNestedNodeAdded(nested);
    }

    m_child_nodes.push_back(std::move(node));

    return m_child_nodes.back().get();
}

bool Node::RemoveChild(NodeList::iterator iter)
{
    if (iter == m_child_nodes.end()) {
        return false;
    }

    if (Node *node = iter->get()) {
        AssertThrow(node->m_parent_node == this);

        for (Node *nested : node->GetDescendents()) {
            OnNestedNodeRemoved(nested);
        }

        OnNestedNodeRemoved(node);

        node->m_parent_node = nullptr;
        node->SetScene(nullptr);
    }

    m_child_nodes.erase(iter);

    return true;
}

bool Node::RemoveChild(size_t index)
{
    if (index >= m_child_nodes.size()) {
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

Node *Node::GetChild(size_t index) const
{
    if (index >= m_child_nodes.size()) {
        return nullptr;
    }

    return m_child_nodes[index].get();
}

Node *Node::Select(const char *selector)
{
    if (selector == nullptr) {
        return nullptr;
    }

    char ch;

    char buffer[256];
    uint32_t buffer_index = 0;
    uint32_t selector_index = 0;

    Node *search_node = this;

    while ((ch = selector[selector_index]) != '\0') {
        const char prev_selector_char = selector_index == 0
            ? '\0'
            : selector[selector_index - 1];

        ++selector_index;

        if (ch == '/' && prev_selector_char != '\\') {
            const auto it = search_node->FindChild(buffer);

            if (it == search_node->GetChildren().end()) {
                return nullptr;
            }

            search_node = it->get();

            if (search_node == nullptr) {
                return nullptr;
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

                return nullptr;
            }
        }

        buffer[buffer_index] = '\0';
    }

    // find remaining
    if (buffer_index != 0) {
        const auto it = search_node->FindChild(buffer);

        if (it == search_node->GetChildren().end()) {
            return nullptr;
        }

        search_node = it->get();
    }

    return search_node;
}

Node::NodeList::iterator Node::FindChild(Node *node)
{
    return std::find_if(
        m_child_nodes.begin(),
        m_child_nodes.end(),
        [node](const auto &it) {
            return it.get() == node;
        }
    );
}

Node::NodeList::iterator Node::FindChild(const char *tag)
{
    return std::find_if(
        m_child_nodes.begin(),
        m_child_nodes.end(),
        [tag](const auto &it) {
            return !std::strcmp(tag, it->GetTag());
        }
    );
}

void Node::SetLocalTransform(const Transform &transform)
{
    m_local_transform = transform;
    
    UpdateWorldTransform();
}

void Node::SetSpatial(Ref<Spatial> &&spatial)
{
    if (m_spatial == spatial) {
        return;
    }

    if (spatial != nullptr) {
        m_spatial = std::move(spatial);
        m_spatial.Init();

        m_local_aabb = m_spatial->GetLocalAabb();
    } else {
        m_local_aabb = BoundingBox();

        m_spatial = nullptr;
    }

    UpdateWorldTransform();
}

void Node::UpdateWorldTransform()
{
    if (m_type == Type::BONE) {
        static_cast<Bone *>(this)->UpdateBoneTransform();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    if (m_parent_node != nullptr) {
        m_world_transform = m_parent_node->GetWorldTransform() * m_local_transform;
    } else {
        m_world_transform = m_local_transform;
    }

    m_world_aabb = m_local_aabb * m_world_transform;

    for (auto &node : m_child_nodes) {
        node->UpdateWorldTransform();

        m_world_aabb.Extend(node->m_world_aabb);
    }

    if (m_spatial != nullptr) {
        m_spatial->SetTransform(m_world_transform);
    }
}

void Node::UpdateInternal(Engine *engine, GameCounter::TickUnit delta)
{
    if (m_spatial != nullptr) {
        m_spatial->Update(engine);
    }

    UpdateControllers(engine, delta);
}

void Node::UpdateControllers(Engine *engine, GameCounter::TickUnit delta)
{
    for (auto &controller : m_controllers) {
        controller.second->OnUpdate(delta);
    }
}

void Node::Update(Engine *engine, GameCounter::TickUnit delta)
{
    UpdateInternal(engine, delta);

    for (auto *node : m_descendents) {
        node->UpdateInternal(engine, delta);
    }
}

} // namespace hyperion::v2
