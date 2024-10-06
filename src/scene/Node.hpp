/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_NODE_HPP
#define HYPERION_NODE_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/functional/Delegate.hpp>

#include <core/utilities/UUID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Variant.hpp>

#include <core/object/HypObject.hpp>

#include <core/Name.hpp>

#include <scene/Entity.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <math/Transform.hpp>
#include <math/Ray.hpp>
#include <math/BoundingBox.hpp>

#ifdef HYP_EDITOR
#include <editor/ObservableVar.hpp>
#endif

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

class Engine;
class Scene;

enum NodeFlags : uint32
{
    NONE                        = 0x0,

    IGNORE_PARENT_TRANSLATION   = 0x1,
    IGNORE_PARENT_SCALE         = 0x2,
    IGNORE_PARENT_ROTATION      = 0x4,
    IGNORE_PARENT_TRANSFORM     = IGNORE_PARENT_TRANSLATION | IGNORE_PARENT_SCALE | IGNORE_PARENT_ROTATION,

    EXCLUDE_FROM_PARENT_AABB    = 0x8
};

HYP_MAKE_ENUM_FLAGS(NodeFlags)

HYP_STRUCT()
struct NodeTag
{
    using ValueType = Variant<String, UUID, Name, int32, uint32, float, double, bool>;

    HYP_FIELD()
    ValueType   value;

    NodeTag() = default;

    explicit NodeTag(const String &value)
        : value(value)
    {
    }

    explicit NodeTag(const UUID &value)
        : value(value)
    {
    }

    explicit NodeTag(Name value)
        : value(value)
    {
    }

    explicit NodeTag(int32 value)
        : value(value)
    {
    }

    explicit NodeTag(uint32 value)
        : value(value)
    {
    }

    explicit NodeTag(float value)
        : value(value)
    {
    }

    explicit NodeTag(double value)
        : value(value)
    {
    }

    explicit NodeTag(bool value)
        : value(value)
    {
    }

    NodeTag(const NodeTag &other)
        : value(other.value)
    {
    }

    NodeTag &operator=(const NodeTag &other)
    {
        value = other.value;

        return *this;
    }

    NodeTag(NodeTag &&other) noexcept
        : value(std::move(other.value))
    {
    }

    NodeTag &operator=(NodeTag &&other) noexcept
    {
        value = std::move(other.value);

        return *this;
    }

    HYP_FORCE_INLINE bool operator==(const NodeTag &other) const
    {
        return value == other.value;
    }

    template <typename T>
    HYP_FORCE_INLINE bool operator==(const T &other) const
    {
        return value.GetTypeID() == TypeID::ForType<NormalizedType<T>>()
            && value.Get<NormalizedType<T>>() == other;
    }

    HYP_FORCE_INLINE bool operator!=(const NodeTag &other) const
    {
        return value != other.value;
    }

    template <typename T>
    HYP_FORCE_INLINE bool operator!=(const T &other) const
    {
        return value.GetTypeID() != TypeID::ForType<NormalizedType<T>>()
            || value.Get<NormalizedType<T>>() != other;
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return value.HasValue(); }

    HYP_FORCE_INLINE bool operator!() const
        { return !value.HasValue(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return value.HasValue(); }

    HYP_FORCE_INLINE HashCode GetHashCode() const
        { return value.GetHashCode(); }

    String ToString() const;
};

HYP_CLASS()
class HYP_API Node : public EnableRefCountedPtrFromThis<Node>
{
    friend class Scene;
    friend class Entity;
    friend class NodeProxy;

    HYP_OBJECT_BODY(Node);

public:

#ifdef HYP_EDITOR
    class EditorObservables
    {
    public:
        EditorObservables()                                             = default;

        EditorObservables(const EditorObservables &other)               = delete;
        EditorObservables &operator=(const EditorObservables &other)    = delete;

        EditorObservables(EditorObservables &&other) noexcept
            : m_delegates(std::move(other.m_delegates))
        {
        }

        EditorObservables &operator=(EditorObservables &&other) noexcept
        {
            m_delegates = std::move(other.m_delegates);

            return *this;
        }

        ~EditorObservables() = default;

        Delegate<void, Name, ConstAnyRef> &GetCatchallDelegate()
        {
            return m_catchall;
        }

        Delegate<void, ConstAnyRef> &Get(Name name)
        {
            auto it = m_delegates.Find(name);

            if (it == m_delegates.End()) {
                it = m_delegates.Insert(name, Delegate<void, ConstAnyRef>()).first;
            }

            return it->second;
        }

        void Trigger(Name name, ConstAnyRef value)
        {
            auto it = m_delegates.Find(name);

            if (it != m_delegates.End()) {
                it->second(value);
            }

            m_catchall(name, value);
        }

        void RemoveAll()
        {
            for (auto &it : m_delegates) {
                it.second.RemoveAll();
            }

            m_catchall.RemoveAll();
        }

    private:
        HashMap<Name, Delegate<void, ConstAnyRef>>  m_delegates;
        Delegate<void, Name, ConstAnyRef>           m_catchall;
    };
#endif

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

    /*! \brief Construct the node, optionally taking in a name string to improve identification.
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

    /*! \brief Get the UUID of the Node. */
    HYP_FORCE_INLINE const UUID &GetUUID() const
        { return m_uuid; }

    /*! \returns The name that was given to the Node on creation. */
    HYP_METHOD(Property="Name", Serialize=true, Editor=true, Label="Name", Description="The name of the node.")
    HYP_FORCE_INLINE const String &GetName() const
        { return m_name; }
        
    /*! \brief Set the name of this Node. Used for nested lookups. */
    HYP_METHOD(Property="Name", Serialize=true, Editor=true, Label="Name", Description="The name of the node.")
    void SetName(const String &name);

    /*! \returns The type of the node. By default, it will just be NODE. */
    HYP_METHOD()
    HYP_FORCE_INLINE Type GetType() const
        { return m_type; }

    /*! \brief Get the flags of the Node.
     *  \see NodeFlagBits
     *  \returns The flags of the Node.
    */
    HYP_FORCE_INLINE NodeFlags GetFlags() const
        { return m_flags; }

    /*! \brief Set the flags of the Node.
     *  \see NodeFlagBits
     *  \param flags The flags to set on the Node. */
    HYP_FORCE_INLINE void SetFlags(NodeFlags flags)
        { m_flags = flags; }

    HYP_METHOD()
    HYP_FORCE_INLINE Node *GetParent() const
        { return m_parent_node; }
    
    bool IsOrHasParent(const Node *node) const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsRoot() const
        { return m_parent_node == nullptr; }

    /*! \returns A pointer to the Scene this Node and its children are attached to. May be null. */
    HYP_METHOD()
    HYP_FORCE_INLINE Scene *GetScene() const
        { return m_scene; }

    /*! \brief Set the Scene this Node and its children are attached to.
     *  \internal Not intended to be used in user code. Use Remove() instead. */
    void SetScene(Scene *scene);

    /*! \brief \returns The underlying entity AABB for this node. */
    HYP_METHOD(Property="EntityAABB", Serialize=true, Editor=true, Label="AABB", Description="The underlying AABB for this node, not considering child nodes or transform")
    HYP_FORCE_INLINE const BoundingBox &GetEntityAABB() const
        { return m_entity_aabb; }

    /*! \brief Set the underlying entity AABB of the Node. Does not update the Entity's BoundingBoxComponent.
    *   \param aabb The entity bounding box to set
    *   \note Calls to RefreshEntityTransform() will override this value. */
    HYP_METHOD(Property="EntityAABB", Serialize=true, Editor=true, Label="AABB", Description="The underlying AABB for this node, not considering child nodes or transform")
    void SetEntityAABB(const BoundingBox &aabb);

    HYP_METHOD(Property="Entity", Serialize=true, Editor=true, Label="Entity", Description="The entity that this node is linked with.")
    HYP_FORCE_INLINE ID<Entity> GetEntity() const
        { return m_entity; }

    HYP_METHOD(Property="Entity", Serialize=true, Editor=true, Label="Entity", Description="The entity that this node is linked with.")
    void SetEntity(ID<Entity> entity);

    /*! \brief Add the Node as a child of this object, taking ownership over the given Node.
     *  \param node The Node to be added as achild of this Node
     *  \returns The added Node */
    HYP_METHOD()
    NodeProxy AddChild(const NodeProxy &node = NodeProxy());

    /*! \brief Remove a child using the given iterator (i.e from FindChild())
     *  \param iter The iterator from this Node's child list
     *  \returns Whether then removal was successful */
    bool RemoveChild(NodeList::Iterator iter);

    /*! \brief Remove a child at the given index
     *  \param index The index of the child element to remove
     *  \returns Whether then removal was successful */
    HYP_METHOD()
    bool RemoveAt(int index);

    /*! \brief Remove this node from the parent Node's list of child Nodes. */
    HYP_METHOD()
    bool Remove();

    HYP_METHOD()
    void RemoveAllChildren();
    
    /*! \brief Get a child Node from this Node's child list at the given index.
     *  \param index The index of the child element to return
     *  \returns The child node at the given index. If the index is out of bounds, nullptr
     *  will be returned. */
    HYP_METHOD()
    NodeProxy GetChild(int index) const;

    /*! \brief Search for a (potentially nested) node using the syntax `some/child/node`.
     *  Each `/` indicates searching a level deeper, so first a child node with the name "some"
     *  is found, after which a child node with the name "child" is searched for on the previous "some" node,
     *  and finally a node with the name "node" is searched for from the above "child" node.
     *  If any level fails to find a node, nullptr is returned.
     *
     *  The string is case-sensitive.
     *  The '/' can be escaped by using a '\' char. */
    HYP_METHOD()
    NodeProxy Select(UTF8StringView selector) const;

    /*! \brief Get an iterator for the given child Node from this Node's child list
     * \param node The node to find in this Node's child list
     * \returns The resulting iterator */
    NodeList::Iterator FindChild(const Node *node);

    /*! \brief Get an iterator for the given child Node from this Node's child list
     * \param node The node to find in this Node's child list
     * \returns The resulting iterator */
    NodeList::ConstIterator FindChild(const Node *node) const;

    /*! \brief Get an iterator for a node by finding it by its name
     * \param name The string to compare with the child Node's name
     * \returns The resulting iterator */
    NodeList::Iterator FindChild(const char *name);

    /*! \brief Get an iterator for a node by finding it by its name
     * \param name The string to compare with the child Node's name
     * \returns The resulting iterator */
    NodeList::ConstIterator FindChild(const char *name) const;

    /*! \brief Get the child Nodes of this Node.
     *  \returns Array of child Nodes */
    HYP_FORCE_INLINE NodeList &GetChildren()
        { return m_child_nodes; }

    /*! \brief Get the child Nodes of this Node.
     *  \returns Array of child Nodes */
    HYP_FORCE_INLINE const NodeList &GetChildren() const
        { return m_child_nodes; }

    /*! \brief Get all descendent child Nodes from this Node. This vector is pre-calculated,
     * so no calculation happens when calling this method.
     * \returns A vector of raw pointers to descendent Nodes */
    HYP_FORCE_INLINE Array<NodeProxy> &GetDescendents()
        { return m_descendents; }

    /*! \brief Get all descendent child Nodes from this Node. This vector is pre-calculated,
     * so no calculation happens when calling this method.
     * \returns A vector of raw pointers to descendent Nodes */
    HYP_FORCE_INLINE const Array<NodeProxy> &GetDescendents() const
        { return m_descendents; }

    /*! \brief Set the local-space translation, scale, rotation of this Node (not influenced by the parent Node) */
    HYP_METHOD(Property="LocalTransform", Serialize=true, Editor=true, Label="Local-space Transform")
    void SetLocalTransform(const Transform &transform);

    /*! \returns The local-space translation, scale, rotation of this Node. */
    HYP_METHOD(Property="LocalTransform", Serialize=true, Editor=true, Label="Local-space Transform")
    HYP_FORCE_INLINE const Transform &GetLocalTransform() const
        { return m_local_transform; }
    
    /*! \returns The local-space translation of this Node. */
    HYP_METHOD()
    HYP_FORCE_INLINE const Vec3f &GetLocalTranslation() const
        { return m_local_transform.GetTranslation(); }
    
    /*! \brief Set the local-space translation of this Node (not influenced by the parent Node) */
    HYP_METHOD()
    HYP_FORCE_INLINE void SetLocalTranslation(const Vec3f &translation)
        { SetLocalTransform({translation, m_local_transform.GetScale(), m_local_transform.GetRotation()}); }

    /*! \brief Move the Node in local-space by adding the given vector to the current local-space translation.
     * \param translation The vector to translate this Node by
     */
    HYP_METHOD()
    HYP_FORCE_INLINE void Translate(const Vec3f &translation)
        { SetLocalTranslation(m_local_transform.GetTranslation() + translation); }
    
    /*! \returns The local-space scale of this Node. */
    HYP_METHOD()
    HYP_FORCE_INLINE const Vec3f &GetLocalScale() const
        { return m_local_transform.GetScale(); }
    
    /*! \brief Set the local-space scale of this Node (not influenced by the parent Node) */
    HYP_METHOD()
    HYP_FORCE_INLINE void SetLocalScale(const Vec3f &scale)
        { SetLocalTransform({m_local_transform.GetTranslation(), scale, m_local_transform.GetRotation()}); }

    /*! \brief Scale the Node in local-space by multiplying the current local-space scale by the given scale vector.
     * \param scale The vector to scale this Node by */
    HYP_METHOD()
    HYP_FORCE_INLINE void Scale(const Vec3f &scale)
        { SetLocalScale(m_local_transform.GetScale() * scale); }
    
    /*! \returns The local-space rotation of this Node. */
    HYP_METHOD()
    HYP_FORCE_INLINE const Quaternion &GetLocalRotation() const
        { return m_local_transform.GetRotation(); }
    
    /*! \brief Set the local-space rotation of this Node (not influenced by the parent Node) */
    HYP_METHOD()
    HYP_FORCE_INLINE void SetLocalRotation(const Quaternion &rotation)
        { SetLocalTransform(Transform(m_local_transform.GetTranslation(), m_local_transform.GetScale(), rotation)); }

    /*! \brief Rotate the Node by multiplying the current local-space rotation by the given quaternion.
     * \param rotation The quaternion to rotate this Node by */
    HYP_METHOD()
    HYP_FORCE_INLINE void Rotate(const Quaternion &rotation)
        { SetLocalRotation(m_local_transform.GetRotation() * rotation); }

    /*! \brief \returns The world-space translation, scale, rotation of this Node. Influenced by accumulative transformation of all ancestor Nodes. */
    HYP_METHOD(Property="WorldTransform", Serialize=true, Editor=true, Label="World-space Transform")
    HYP_FORCE_INLINE const Transform &GetWorldTransform() const
        { return m_world_transform; }

    /*! \brief Set the world-space translation, scale, rotation of this Node  */
    HYP_METHOD(Property="WorldTransform", Serialize=true, Editor=true, Label="World-space Transform")
    void SetWorldTransform(const Transform &transform)
    {
        if (m_parent_node == nullptr) {
            SetLocalTransform(transform);

            return;
        }

        Transform offset_transform;
        offset_transform.GetTranslation() = (m_flags & NodeFlags::IGNORE_PARENT_TRANSLATION)
            ? transform.GetTranslation()
            : transform.GetTranslation() - m_parent_node->GetWorldTranslation();
        offset_transform.GetScale() = (m_flags & NodeFlags::IGNORE_PARENT_SCALE)
            ? transform.GetScale()
            : transform.GetScale() / m_parent_node->GetWorldScale();
        offset_transform.GetRotation() = (m_flags & NodeFlags::IGNORE_PARENT_ROTATION)
            ? transform.GetRotation()
            : Quaternion(m_parent_node->GetWorldRotation()).Invert() * transform.GetRotation();

        offset_transform.UpdateMatrix();

        SetLocalTransform(offset_transform);
    }
    
    /*! \returns The world-space translation of this Node. */
    HYP_METHOD()
    HYP_FORCE_INLINE const Vec3f &GetWorldTranslation() const
        { return m_world_transform.GetTranslation(); }
    
    /*! \brief Set the world-space translation of this Node by offsetting the local-space translation */
    HYP_METHOD()
    void SetWorldTranslation(const Vec3f &translation)
    {
        if (m_parent_node == nullptr || (m_flags & NodeFlags::IGNORE_PARENT_TRANSLATION)) {
            SetLocalTranslation(translation);

            return;
        }

        SetLocalTranslation(translation - m_parent_node->GetWorldTranslation());
    }
    
    /*! \returns The local-space scale of this Node. */
    HYP_METHOD()
    HYP_FORCE_INLINE const Vec3f &GetWorldScale() const
        { return m_world_transform.GetScale(); }
    
    /*! \brief Set the local-space scale of this Node by offsetting the local-space scale */
    HYP_METHOD()
    void SetWorldScale(const Vec3f &scale)
    {
        if (m_parent_node == nullptr || (m_flags & NodeFlags::IGNORE_PARENT_SCALE)) {
            SetLocalScale(scale);

            return;
        }

        SetLocalScale(scale / m_parent_node->GetWorldScale());
    }
    
    /*! \returns The world-space rotation of this Node. */
    HYP_METHOD()
    HYP_FORCE_INLINE const Quaternion &GetWorldRotation() const
        { return m_world_transform.GetRotation(); }
    
    /*! \brief Set the world-space rotation of this Node by offsetting the local-space rotation */
    HYP_METHOD()
    void SetWorldRotation(const Quaternion &rotation)
    {
        if (m_parent_node == nullptr || (m_flags & NodeFlags::IGNORE_PARENT_ROTATION)) {
            SetLocalRotation(rotation);

            return;
        }

        SetLocalRotation(Quaternion(m_parent_node->GetWorldRotation()).Invert() * rotation);
    }

    /*! \brief Get the relative transform of this Node to the given parent transform.
     * \param parent_transform The parent transform to calculate the relative transform to.
     * \returns The relative transform of this Node to the given parent transform. */
    HYP_METHOD()
    Transform GetRelativeTransform(const Transform &parent_transform) const;

    /*! \brief Returns whether the Node is locked from being transformed. */
    HYP_METHOD()
    HYP_FORCE_INLINE bool IsTransformLocked() const
        { return m_transform_locked; }

    /*! \brief Lock the Node from being transformed. */
    void LockTransform();

    /*! \brief Unlock the Node from being transformed. */
    void UnlockTransform();

    /*! \brief Get the local-space (model) aabb of the node, excluding the entity's aabb.
     *  \returns The local-space (model) of the node's aabb, excluding the entity's aabb. */
    HYP_METHOD()
    BoundingBox GetLocalAABBExcludingSelf() const;

    /*! \brief Get the local-space (model) aabb of the node.
     *  \returns The local-space (model) of the node's aabb. */
    HYP_METHOD()
    BoundingBox GetLocalAABB() const;

    /*! \brief \returns The world-space aabb of the node. Includes the transforms of all
     * parent nodes. */
    HYP_METHOD()
    BoundingBox GetWorldAABB() const;

    /*! \brief Update the world transform of the Node to reflect changes in the local transform and parent transform.
     *  This will update the TransformComponent of the entity if it exists. */
    HYP_METHOD()
    void UpdateWorldTransform();

    /*! \brief Calculate the depth of the Node relative to the root Node.
     * \returns The depth of the Node relative to the root Node. If the Node has no parent, 0 is returned. */
    HYP_METHOD()
    uint CalculateDepth() const;

    /*! \brief Calculate the index of this Node in its parent's child list.
     * \returns The index of this Node in its parent's child list. If the Node has no parent, -1 is returned. */
    HYP_METHOD()
    uint FindSelfIndex() const;

    bool TestRay(const Ray &ray, RayTestResults &out_results) const;

    /*! \brief Search child nodes (breadth-first) until a node with an Entity with the given ID is found. */
    HYP_METHOD()
    NodeProxy FindChildWithEntity(ID<Entity> entity) const;

    /*! \brief Search child nodes (breadth-first) until a node with the given name is found. */
    HYP_METHOD()
    NodeProxy FindChildByName(UTF8StringView name) const;

    /*! \brief Search child nodes (breadth-first) until a node with the given UUID is found. */
    HYP_METHOD()
    NodeProxy FindChildByUUID(const UUID &uuid) const;

    /*! \brief Get the delegates for this Node. */
    HYP_FORCE_INLINE Delegates *GetDelegates() const
        { return m_delegates.Get(); }

    /*! \brief Get all tags of this Node. */
    HYP_FORCE_INLINE const FlatMap<Name, NodeTag> &GetTags() const
        { return m_tags; }

    /*! \brief Add a tag to this Node. */
    void AddTag(Name key, const NodeTag &value);

    /*! \brief Remove a tag from this Node. */
    bool RemoveTag(Name key);

    /*! \brief Get a tag from this Node.
     *  \returns The tag with the given name. If the tag does not exist, an empty NodeTag is returned */
    const NodeTag &GetTag(Name key) const;

    /*! \brief Check if this Node has a tag with the given name.
     *  \returns True if the tag exists, false otherwise. */
    bool HasTag(Name key) const;

    /*! \brief Get a NodeProxy for this Node. Increments the reference count of the Node's underlying reference count. */
    HYP_FORCE_INLINE NodeProxy GetProxy()
        { return NodeProxy(this); }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_type);
        hc.Add(m_name);
        hc.Add(m_local_transform);
        hc.Add(m_world_transform);
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

    static Scene *GetDefaultScene();

    /*! \brief Refresh the transform of the entity attached to this Node. This will update the entity AABB to match,
     *  and will update the TransformComponent of the entity if it exists. */
    void RefreshEntityTransform();

    void OnNestedNodeAdded(const NodeProxy &node, bool direct);
    void OnNestedNodeRemoved(const NodeProxy &node, bool direct);

    Type                        m_type = Type::NODE;
    EnumFlags<NodeFlags>        m_flags = NodeFlags::NONE;
    String                      m_name;
    Node                        *m_parent_node;
    NodeList                    m_child_nodes;
    Transform                   m_local_transform;
    Transform                   m_world_transform;
    BoundingBox                 m_entity_aabb;

    ID<Entity>                  m_entity;

    Array<NodeProxy>            m_descendents;

    Scene                       *m_scene;

    bool                        m_transform_locked;

    UniquePtr<Delegates>        m_delegates;

    FlatMap<Name, NodeTag>      m_tags;

    UUID                        m_uuid;
};

} // namespace hyperion

#endif