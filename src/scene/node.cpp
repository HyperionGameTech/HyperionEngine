#include "Node.hpp"
#include <system/Debug.hpp>

#include <animation/Bone.hpp>
#include <Engine.hpp>

#include <cstring>

namespace hyperion::v2 {

Node::Node(
    const char *name,
    const Transform &local_transform
) : Node(
        name,
        nullptr,
        local_transform
    )
{
}

Node::Node(
    const char *name,
    Ref<Spatial> &&spatial,
    const Transform &local_transform
) : Node(
        Type::NODE,
        name,
        std::move(spatial),
        local_transform
    )
{
}

Node::Node(
    Type type,
    const char *name,
    Ref<Spatial> &&spatial,
    const Transform &local_transform
) : m_type(type),
    m_parent_node(nullptr),
    m_local_transform(local_transform),
    m_scene(nullptr)
{
    SetSpatial(std::move(spatial));

    const size_t len = std::strlen(name);
    m_name = new char[len + 1];
    std::strcpy(m_name, name);
}

Node::~Node()
{
    SetSpatial(nullptr);

    delete[] m_name;
}

void Node::SetName(const char *name)
{
    if (name == m_name || !std::strcmp(name, m_name)) {
        return;
    }

    delete[] m_name;

    const size_t len = std::strlen(name);
    m_name = new char[len + 1];
    std::strcpy(m_name, name);
}

void Node::SetScene(Scene *scene)
{
    if (m_scene != nullptr && m_spatial != nullptr) {
        m_scene->RemoveSpatial(m_spatial);
    }

    m_scene = scene;

    if (m_scene != nullptr && m_spatial != nullptr) {
        m_scene->AddSpatial(m_spatial.IncRef());
    }

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
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node);
    }
}

void Node::OnNestedNodeRemoved(Node *node)
{
    const auto it = std::find(m_descendents.begin(), m_descendents.end(), node);

    if (it != m_descendents.end()) {
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

    auto *child = m_child_nodes.back().get();

    child->UpdateWorldTransform();

    return child;
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

    UpdateWorldTransform();

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
    UInt32 buffer_index = 0;
    UInt32 selector_index = 0;

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

Node::NodeList::iterator Node::FindChild(const char *name)
{
    return std::find_if(
        m_child_nodes.begin(),
        m_child_nodes.end(),
        [name](const auto &it) {
            return !std::strcmp(name, it->GetName());
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

    if (m_spatial != nullptr) {
        if (m_scene != nullptr) {
            m_scene->RemoveSpatial(m_spatial);
        }

        m_spatial->SetParent(nullptr);
    }

    if (spatial != nullptr) {
        m_spatial = std::move(spatial);

        if (m_scene != nullptr) {
            m_scene->AddSpatial(m_spatial.IncRef());
        }

        m_spatial->SetParent(this);
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

    if (m_parent_node != nullptr) {
        m_parent_node->m_world_aabb.Extend(m_world_aabb);
    }

    if (m_spatial != nullptr) {
        m_spatial->SetTransform(m_world_transform);
    }
}

bool Node::TestRay(const Ray &ray, RayTestResults &out_results) const
{
    const bool has_node_hit = ray.TestAabb(m_world_aabb);

    bool has_entity_hit = false;

    if (has_node_hit) {
        if (m_spatial != nullptr) {
            has_entity_hit = ray.TestAabb(
                m_spatial->GetWorldAabb(),
                m_spatial->GetId().value,
                m_spatial.ptr,
                out_results
            );
        }

        for (auto &child_node : m_child_nodes) {
            if (child_node == nullptr) {
                continue;
            }

            if (child_node->TestRay(ray, out_results)) {
                has_entity_hit = true;
            }
        }
    }

    return has_entity_hit;
}

void Node::RequestPipelineChanges()
{
    
}

} // namespace hyperion::v2
