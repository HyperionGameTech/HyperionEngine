/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_NODE_HPP
#define HYPERION_NODE_HPP

#include <core/Containers.hpp>
#include <core/containers/String.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/functional/Delegate.hpp>

#include <scene/Entity.hpp>
#include <scene/NodeProxy.hpp>

#include <math/Transform.hpp>
#include <math/Ray.hpp>
#include <math/BoundingBox.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

class Engine;
class Scene;

using NodeFlags = uint32;

enum NodeFlagBits : NodeFlags
{
    NODE_FLAG_NONE                      = 0x0,
    NODE_FLAG_IGNORE_PARENT_TRANSLATION = 0x1,
    NODE_FLAG_IGNORE_PARENT_SCALE       = 0x2,
    NODE_FLAG_IGNORE_PARENT_ROTATION    = 0x4,
    NODE_FLAG_IGNORE_PARENT_TRANSFORM   = NODE_FLAG_IGNORE_PARENT_TRANSLATION | NODE_FLAG_IGNORE_PARENT_SCALE | NODE_FLAG_IGNORE_PARENT_ROTATION,
};

class HYP_API Node : public EnableRefCountedPtrFromThis<Node>
{
    friend class Scene;
    friend class Entity;
    friend class NodeProxy;

public:
    struct Delegates
    {
        Delegate<void, const NodeProxy &, bool /* direct */> OnNestedNodeAdded;
        Delegate<void, const NodeProxy &, bool /* direct */> OnNestedNodeRemoved;
    };

    using NodeList = Array<NodeProxy>;

    enum class Type : uint32
    {
        NODE,
        BONE
    };

    /*! \brief Construct the node, optionally taking in a string tag to improve identification.
     * \param name A String representing the name of the Node.
     * \param local_transform An optional parameter representing the local-space transform of this Node.
     */
    Node(
        const String &name = String::empty,
        const Transform &local_transform = Transform()
    );

    Node(
        const String &name,
        ID<Entity> entity,
        const Transform &local_transform = Transform()
    );

    Node(
        const String &name,
        ID<Entity> entity,
        const Transform &local_transform,
        Scene *scene
    );

    Node(const Node &other)             = delete;
    Node &operator=(const Node &other)  = delete;
    Node(Node &&other) noexcept;
    Node &operator=(Node &&other) noexcept;
    ~Node();

    /*! \returns The string tag that was given to the Node on creation. */
    const String &GetName() const { return m_name; }
    /*! \brief Set the string tag of this Node. Used for nested lookups. */
    void SetName(const String &name) { m_name = name; }

    /*! \returns The type of the node. By default, it will just be NODE. */
    Type GetType() const
        { return m_type; }

    /*! \brief Get the flags of the Node.
     *  \see NodeFlagBits
     *  \returns The flags of the Node.
    */
    NodeFlags GetFlags() const
        { return m_flags; }

    /*! \brief Set the flags of the Node.
     *  \see NodeFlagBits
     *  \param flags The flags to set on the Node.
    */
    void SetFlags(NodeFlags flags)
        { m_flags = flags; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Node *GetParent() const
        { return m_parent_node; }

    [[nodiscard]]
    bool IsOrHasParent(const Node *node) const;

    /*! \returns A pointer to the Scene this Node and its children are attached to. May be null. */
    Scene *GetScene() const { return m_scene; }

    /*! \brief Set the Scene this Node and its children are attached to. */
    void SetScene(Scene *scene);

    ID<Entity> GetEntity() const { return m_entity; }
    void SetEntity(ID<Entity> entity);

    /*! \brief Add a new child Node to this object
     * \returns The added Node
     */
    NodeProxy AddChild();

    /*! \brief Add the Node as a child of this object, taking ownership over the given Node.
     * \param node The Node to be added as achild of this Node
     * \returns The added Node
     */
    NodeProxy AddChild(const NodeProxy &node);

    /*! \brief Remove a child using the given iterator (i.e from FindChild())
     * \param iter The iterator from this Node's child list
     * \returns Whether then removal was successful
     */
    bool RemoveChild(NodeList::Iterator iter);

    /*! \brief Remove a child at the given index
     * \param index The index of the child element to remove
     * \returns Whether then removal was successful
     */
    bool RemoveChild(SizeType index);

    /*! \brief Remove this node from the parent Node's list of child Nodes. */
    bool Remove();

    void RemoveAllChildren();
    
    /*! \brief Get a child Node from this Node's child list at the given index.
     * \param index The index of the child element to return
     * \returns The child node at the given index. If the index is out of bounds, nullptr
     * will be returned.
     */
    NodeProxy GetChild(SizeType index) const;

    /*! \brief Search for a (potentially nested) node using the syntax `some/child/node`.
     * Each `/` indicates searching a level deeper, so first a child node with the tag "some"
     * is found, after which a child node with the tag "child" is searched for on the previous "some" node,
     * and finally a node with the tag "node" is searched for from the above "child" node.
     * If any level fails to find a node, nullptr is returned.
     *
     * The string is case-sensitive.
     * The '/' can be escaped by using a '\' char.
     */
    NodeProxy Select(const char *selector) const;

    /*! \brief Get an iterator for the given child Node from this Node's child list
     * \param node The node to find in this Node's child list
     * \returns The resulting iterator
     */
    NodeList::Iterator FindChild(const Node *node);

    /*! \brief Get an iterator for the given child Node from this Node's child list
     * \param node The node to find in this Node's child list
     * \returns The resulting iterator
     */
    NodeList::ConstIterator FindChild(const Node *node) const;

    /*! \brief Get an iterator for a node by finding it by its string tag
     * \param name The string tag to compare with the child Node's string tag
     * \returns The resulting iterator
     */
    NodeList::Iterator FindChild(const char *name);

    /*! \brief Get an iterator for a node by finding it by its string tag
     * \param name The string tag to compare with the child Node's string tag
     * \returns The resulting iterator
     */
    NodeList::ConstIterator FindChild(const char *name) const;

    /*! \brief Get the child Nodes of this Node.
     *  \returns Array of child Nodes
     */
    NodeList &GetChildren()
        { return m_child_nodes; }

    /*! \brief Get the child Nodes of this Node.
     *  \returns Array of child Nodes
     */
    const NodeList &GetChildren() const
        { return m_child_nodes; }

    /*! \brief Get all descendent child Nodes from this Node. This vector is pre-calculated,
     * so no calculation happens when calling this method.
     * \returns A vector of raw pointers to descendent Nodes
     */
    Array<NodeProxy> &GetDescendents()
        { return m_descendents; }

    /*! \brief Get all descendent child Nodes from this Node. This vector is pre-calculated,
     * so no calculation happens when calling this method.
     * \returns A vector of raw pointers to descendent Nodes
     */
    const Array<NodeProxy> &GetDescendents() const
        { return m_descendents; }

    /*! \brief Set the local-space translation, scale, rotation of this Node (not influenced by the parent Node) */
    void SetLocalTransform(const Transform &);

    /*! \returns The local-space translation, scale, rotation of this Node. */
    const Transform &GetLocalTransform() const
        { return m_local_transform; }
    
    /*! \returns The local-space translation of this Node. */
    const Vec3f &GetLocalTranslation() const
        { return m_local_transform.GetTranslation(); }
    
    /*! \brief Set the local-space translation of this Node (not influenced by the parent Node) */
    void SetLocalTranslation(const Vec3f &translation)
        { SetLocalTransform({translation, m_local_transform.GetScale(), m_local_transform.GetRotation()}); }

    /*! \brief Move the Node in local-space by adding the given vector to the current local-space translation.
     * \param translation The vector to translate this Node by
     */
    void Translate(const Vec3f &translation)
        { SetLocalTranslation(m_local_transform.GetTranslation() + translation); }
    
    /*! \returns The local-space scale of this Node. */
    const Vec3f &GetLocalScale() const
        { return m_local_transform.GetScale(); }
    
    /*! \brief Set the local-space scale of this Node (not influenced by the parent Node) */
    void SetLocalScale(const Vec3f &scale)
        { SetLocalTransform({m_local_transform.GetTranslation(), scale, m_local_transform.GetRotation()}); }

    /*! \brief Scale the Node in local-space by multiplying the current local-space scale by the given scale vector.
     * \param scale The vector to scale this Node by
     */
    void Scale(const Vec3f &scale)
        { SetLocalScale(m_local_transform.GetScale() * scale); }
    
    /*! \returns The local-space rotation of this Node. */
    const Quaternion &GetLocalRotation() const
        { return m_local_transform.GetRotation(); }
    
    /*! \brief Set the local-space rotation of this Node (not influenced by the parent Node) */
    void SetLocalRotation(const Quaternion &rotation)
        { SetLocalTransform(Transform(m_local_transform.GetTranslation(), m_local_transform.GetScale(), rotation)); }

    /*! \brief Rotate the Node by multiplying the current local-space rotation by the given quaternion.
     * \param rotation The quaternion to rotate this Node by
     */
    void Rotate(const Quaternion &rotation)
        { SetLocalRotation(m_local_transform.GetRotation() * rotation); }

    /*! \brief \returns The world-space translation, scale, rotation of this Node. Influenced by accumulative transformation of all ancestor Nodes. */
    const Transform &GetWorldTransform() const { return m_world_transform; }

    /*! \brief Set the local-space translation, scale, rotation of this Node  */
    void SetWorldTransform(const Transform &transform)
    {
        if (m_parent_node == nullptr) {
            SetLocalTransform(transform);

            return;
        }

        Transform offset_transform;
        offset_transform.GetTranslation() = (m_flags & NODE_FLAG_IGNORE_PARENT_TRANSLATION)
            ? transform.GetTranslation() : transform.GetTranslation() - m_parent_node->GetWorldTranslation();
        offset_transform.GetScale() = (m_flags & NODE_FLAG_IGNORE_PARENT_SCALE)
            ? transform.GetScale() : transform.GetScale() / m_parent_node->GetWorldScale();
        offset_transform.GetRotation() = (m_flags & NODE_FLAG_IGNORE_PARENT_ROTATION)
            ? transform.GetRotation() : Quaternion(m_parent_node->GetWorldRotation()).Invert() * transform.GetRotation();

        offset_transform.UpdateMatrix();

        SetLocalTransform(offset_transform);
    }
    
    /*! \returns The world-space translation of this Node. */
    const Vec3f &GetWorldTranslation() const
        { return m_world_transform.GetTranslation(); }
    
    /*! \brief Set the world-space translation of this Node by offsetting the local-space translation */
    void SetWorldTranslation(const Vec3f &translation)
    {
        if (m_parent_node == nullptr || (m_flags & NODE_FLAG_IGNORE_PARENT_TRANSLATION)) {
            SetLocalTranslation(translation);

            return;
        }

        SetLocalTranslation(translation - m_parent_node->GetWorldTranslation());
    }
    
    /*! \returns The local-space scale of this Node. */
    const Vec3f &GetWorldScale() const
        { return m_world_transform.GetScale(); }
    
    /*! \brief Set the local-space scale of this Node by offsetting the local-space scale */
    void SetWorldScale(const Vec3f &scale)
    {
        if (m_parent_node == nullptr || (m_flags & NODE_FLAG_IGNORE_PARENT_SCALE)) {
            SetLocalScale(scale);

            return;
        }

        SetLocalScale(scale / m_parent_node->GetWorldScale());
    }
    
    /*! \returns The world-space rotation of this Node. */
    const Quaternion &GetWorldRotation() const
        { return m_world_transform.GetRotation(); }
    
    /*! \brief Set the world-space rotation of this Node by offsetting the local-space rotation */
    void SetWorldRotation(const Quaternion &rotation)
    {
        if (m_parent_node == nullptr || (m_flags & NODE_FLAG_IGNORE_PARENT_ROTATION)) {
            SetLocalRotation(rotation);

            return;
        }

        SetLocalRotation(Quaternion(m_parent_node->GetWorldRotation()).Invert() * rotation);
    }

    /*! \brief \returns The local-space (model) of the node's aabb. Only includes
     * the Entity's aabb
     */
    const BoundingBox &GetLocalAABB() const
        { return m_local_aabb; }

    /*! \brief Set the local-space AABB of the Node. Used for marshaling data */
    void SetLocalAABB(const BoundingBox &aabb)
        { m_local_aabb = aabb; UpdateWorldTransform(); }

    /*! \brief \returns The world-space aabb of the node. Includes the transforms of all
     * parent nodes.
     */
    const BoundingBox &GetWorldAABB() const
        { return m_world_aabb; }

    /*! \brief Set the world-space AABB of the Node. Used for marshaling data */
    void SetWorldAABB(const BoundingBox &aabb)
        { m_world_aabb = aabb; UpdateWorldTransform(); }

    void UpdateWorldTransform();

    void RefreshEntityTransform();

    /*! \brief Calculate the depth of the Node relative to the root Node.
     * \returns The depth of the Node relative to the root Node. If the Node has no parent, 0 is returned. */
    uint CalculateDepth() const;

    /*! \brief Calculate the index of this Node in its parent's child list.
     * \returns The index of this Node in its parent's child list. If the Node has no parent, -1 is returned.
     */
    uint FindSelfIndex() const;

    bool TestRay(const Ray &ray, RayTestResults &out_results) const;

    /*! \brief Search child nodes (breadth-first) until a node with an Entity with the given ID is found. */
    NodeProxy FindChildWithEntity(ID<Entity>) const;
    /*! \brief Search child nodes (breadth-first) until a node with the given name is found. */
    NodeProxy FindChildByName(const String &) const;

    /*! \brief Returns whether the Node is locked from being transformed. */
    bool IsTransformLocked() const
        { return m_transform_locked; }

    /*! \brief Lock the Node from being transformed. */
    void LockTransform();

    /*! \brief Unlock the Node from being transformed. */
    void UnlockTransform();

    Delegates *GetDelegates() const
        { return m_delegates.Get(); }

    /*! \brief Get a NodeProxy for this Node. Increments the reference count of the Node's underlying reference count. */
    NodeProxy GetProxy()
        { return NodeProxy(this); }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_type);
        hc.Add(m_name);
        hc.Add(m_local_transform);
        hc.Add(m_world_transform);
        hc.Add(m_local_aabb);
        hc.Add(m_world_aabb);
        hc.Add(m_entity);

        for (auto &child : m_child_nodes) {
            hc.Add(child);
        }

        return hc;
    }

protected:
    Node(
        Type type,
        const String &name,
        ID<Entity> entity,
        const Transform &local_transform = Transform(),
        Scene *scene = nullptr
    );

    void OnNestedNodeAdded(const NodeProxy &node, bool direct);
    void OnNestedNodeRemoved(const NodeProxy &node, bool direct);

    Type                    m_type = Type::NODE;
    NodeFlags               m_flags = NODE_FLAG_NONE;
    String                  m_name;
    Node                    *m_parent_node;
    NodeList                m_child_nodes;
    Transform               m_local_transform;
    Transform               m_world_transform;
    BoundingBox             m_local_aabb;
    BoundingBox             m_world_aabb;

    ID<Entity>              m_entity;

    Array<NodeProxy>        m_descendents;

    Scene                   *m_scene;

    bool                    m_transform_locked;

    UniquePtr<Delegates>    m_delegates;
};

} // namespace hyperion

#endif