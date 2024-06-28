/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_NODE_PROXY_HPP
#define HYPERION_NODE_PROXY_HPP

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <math/Transform.hpp>
#include <math/Ray.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Scene;
class Node;
class Entity;

class NodeProxy;

struct HYP_API NodeProxyChildren
{
    struct HYP_API IteratorBase
    {
        const Node *node;
        SizeType index;

        IteratorBase &operator++()
        {
            ++index;

            return *this;
        }
    };

    struct HYP_API Iterator : IteratorBase
    {
        using Base = IteratorBase;

        bool operator==(const Iterator &other) const
            { return node == other.node && index == other.index; }

        bool operator!=(const Iterator &other) const
            { return node != other.node || index != other.index; }

        NodeProxy &operator*();
        NodeProxy *operator->();

        Iterator &operator++()
        {
            Base::operator++();

            return *this;
        }
    };

    struct HYP_API ConstIterator : IteratorBase
    {
        using Base = IteratorBase;

        bool operator==(const ConstIterator &other) const
            { return node == other.node && index == other.index; }

        bool operator!=(const ConstIterator &other) const
            { return node != other.node || index != other.index; }

        const NodeProxy &operator*() const;
        const NodeProxy *operator->() const;

        ConstIterator &operator++()
        {
            Base::operator++();

            return *this;
        }
    };

    const Node *node;

    uint Size() const;

    Iterator Begin();
    ConstIterator Begin() const;
    Iterator End();
    ConstIterator End() const;

    Iterator begin() { return Begin(); }
    Iterator end() { return End(); }
    ConstIterator cbegin() const { return Begin(); }
    ConstIterator cend() const { return End(); }
};


/*! \brief The NodeProxy class is a reference-counted wrapper around a Node pointer, allowing
    for safe access to Node data and lessening the likelihood of a missing Node asset causing a full game crash. */
class HYP_API NodeProxy : RC<Node>
{
protected:
    using Base = RC<Node>;

public:
    static const NodeProxy empty;

    /*! \brief Construct an empty NodeProxy. */
    NodeProxy();
    /*! \brief Construct a NodeProxy from a Node pointer. Takes ownership of the Node, so ensure the Node pointer is not used elsewhere. */
    explicit NodeProxy(Node *ptr);
    explicit NodeProxy(const Base &other);
    NodeProxy(const NodeProxy &other);
    NodeProxy &operator=(const NodeProxy &other);
    NodeProxy(NodeProxy &&other) noexcept;
    NodeProxy &operator=(NodeProxy &&other) noexcept;
    ~NodeProxy();

    /*! \brief Return the Node pointer held by the NodeProxy. */
    HYP_NODISCARD HYP_FORCE_INLINE
    Node *Get() const
        { return Base::Get(); }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    bool Is() const
    {
        static_assert(std::is_base_of_v<Node, T>, "T must be a subclass of Node");

        return Base::Is<T>();
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    T *Cast() const
    {
        if (Base::Is<T>()) {
            return static_cast<T *>(Base::Get());
        }

        return nullptr;
    }

    /*! \brief Checks if the NodeProxy is not empty */
    HYP_NODISCARD HYP_FORCE_INLINE
    bool Any() const
        { return Get() != nullptr; }

    /*! \brief Checks if the NodeProxy is empty */
    HYP_NODISCARD HYP_FORCE_INLINE
    bool Empty() const
        { return Get() == nullptr; }

    /*! \brief Checks if the NodeProxy is in a valid state. (equivalent to Any()) */
    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsValid() const
        { return Any(); }

    /*! \brief Conversion operator to bool. Checks if the NodeProxy is not empty. */
    HYP_NODISCARD HYP_FORCE_INLINE
    explicit operator bool() const
        { return Any(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator!() const
        { return Empty(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator==(const NodeProxy &other) const
        { return Get() == other.Get(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator!=(const NodeProxy &other) const
        { return Get() != other.Get(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return Get() == nullptr; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return Get() != nullptr; }

    HYP_NODISCARD HYP_FORCE_INLINE
    Node *operator->() const
        { return Get(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    Node &operator*() const
        { return *Get(); }

    /*! \brief If the Node is present, returns a child Node at the given index.
        If the index is out of bounds, returns an empty NodeProxy. If no Node is present, returns an empty NodeProxy. */
    HYP_NODISCARD HYP_FORCE_INLINE
    NodeProxy operator[](SizeType index)
        { return GetChild(index); }

    /*! \brief If the Node exists, returns the String name of the Node.
        Otherwise, returns the empty String. */
    const String &GetName() const;

    /*! \brief If the Node exists, sets the name of the Node. */
    void SetName(const String &name);

    /*! \brief If the NodeProxy is not empty, returns the Entity, if any,
        attached to the underlying Node. */
    ID<Entity> GetEntity() const;

    /*! \brief If the Node exists, sets the Entity attached to the Node. */
    void SetEntity(ID<Entity> entity);

    /*! \brief If the Node is present, returns the parent Node of the Node.
        If no Node is present, returns nullptr. */
    Node *GetParent() const;

    /*! \brief If the Node is present, returns a boolean indicating whether the \ref{node} is equal to the the held node or a parent of the held node.
        If no Node is present, returns false. */
    bool IsOrHasParent(const Node *node) const;

    /*! \brief If the NodeProxy is not empty, and index is not out of bounds,
        returns a new NodeProxy, holding a pointer to the child of the held Node at the given index.
        If the index is out of bounds, returns an empty node proxy.
        If no Node is held, returns an empty NodeProxy. */
    NodeProxy GetChild(SizeType index);

    NodeProxyChildren GetChildren()
        { return NodeProxyChildren { const_cast<const Node *>(Base::Get()) }; }

    const NodeProxyChildren GetChildren() const
        { return NodeProxyChildren { Base::Get() }; }

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

    /*! \brief If the Node is present, adds a new child to that Node,
        and returns a new NodeProxy of that added child Node. If the Node
        is not present, returns an empty NodeProxy. */
    NodeProxy AddChild();

    /*! \brief If the Node is present, adds the given child to the Node,
        and returns a new NodeProxy of that added child Node. If the Node
        is not present, returns an empty NodeProxy. */
    NodeProxy AddChild(const NodeProxy &node);

    /*! \brief If the Node is present and attached to a parent Node,
        detaches it from that Node.
        \returns Whether or not the Node was removed from its parent. If no Node is present, returns false. */
    bool Remove();

    /*! \returns The local-space translation, scale, rotation of this Node. */
    const Transform &GetLocalTransform() const;

    void SetLocalTransform(const Transform &transform);

    /*! \returns The local-space translation of this Node. */
    const Vec3f &GetLocalTranslation() const;

    /*! \brief Set the local-space translation of this Node (not influenced by the parent Node) */
    void SetLocalTranslation(const Vec3f &translation);

    /*! \brief Move the Node in local-space by adding the given vector to the current local-space translation.
     * \param translation The vector to translate this Node by
     */
    void Translate(const Vec3f &translation)
        { SetLocalTranslation(GetLocalTranslation() + translation); }

    /*! \returns The local-space scale of this Node. */
    const Vec3f &GetLocalScale() const;

    /*! \brief Set the local-space scale of this Node (not influenced by the parent Node) */
    void SetLocalScale(const Vec3f &scale);

    /*! \brief Scale the Node in local-space by multiplying the current local-space scale by the given scale vector.
     * \param scale The vector to scale this Node by
     */
    void Scale(const Vec3f &scale)
        { SetLocalScale(GetLocalScale() * scale); }

    /*! \returns The local-space rotation of this Node. */
    const Quaternion &GetLocalRotation() const;

    /*! \brief Set the local-space rotation of this Node (not influenced by the parent Node) */
    void SetLocalRotation(const Quaternion &rotation);

    /*! \brief Rotate the Node by multiplying the current local-space rotation by the given quaternion.
     * \param rotation The quaternion to rotate this Node by
     */
    void Rotate(const Quaternion &rotation)
        { SetLocalRotation(GetLocalRotation() * rotation); }

    /*! \brief Rotate the Node by multiplying the current local-space rotation by the given Euler angles.
     * \param angles The angles to rotate this Node by
     */
    void Rotate(const Vec3f &angles)
        { SetLocalRotation(GetLocalRotation() * Quaternion(angles)); }

    /*! \returns The world-space translation, scale, rotation of this Node. Influenced by accumulative transformation of all ancestor Nodes. */
    const Transform &GetWorldTransform() const;

    /*! \returns The world-space translation of this Node. */
    const Vec3f &GetWorldTranslation() const;

    /*! \brief Set the world-space translation of this Node by offsetting the local-space translation */
    void SetWorldTranslation(const Vec3f &translation);

    /*! \returns The local-space scale of this Node. */
    const Vec3f &GetWorldScale() const;

    /*! \brief Set the local-space scale of this Node by offsetting the local-space scale */
    void SetWorldScale(const Vec3f &scale);

    /*! \returns The world-space rotation of this Node. */
    const Quaternion &GetWorldRotation() const;

    /*! \brief Set the world-space rotation of this Node by offsetting the local-space rotation */
    void SetWorldRotation(const Quaternion &rotation);

    /*! \returns The local-space (model) of the node's aabb. Does not include any child nodes' transforms. */
    BoundingBox GetEntityAABB() const;
    
    /*! \brief Set the local-space (model) of the node's aabb. */
    void SetEntityAABB(const BoundingBox &aabb);

    /*! \returns The local-space (model) of the node's aabb, including child nodes with their relative transforms. */
    BoundingBox GetLocalAABB() const;

    /*! \returns The world-space aabb of the node. Includes the transforms of all
     * parent nodes. */
    BoundingBox GetWorldAABB() const;

    /*! \brief If the Node is present, returns true if the Node's transform is locked. */
    bool IsTransformLocked() const;

    /*! \brief If the Node is present, locks the Node's transform, preventing it from being modified. */
    void LockTransform();

    /*! \brief Unlock the Node's transform, allowing it to be modified again. */
    void UnlockTransform();

    /*! \brief Calculate the depth of the Node relative to the root Node. */
    uint CalculateDepth() const;

    /*! \brief Conversion operator to Weak<Node>. */
    HYP_NODISCARD HYP_FORCE_INLINE
    operator Weak<Node>() const
        { return Weak<Node>(static_cast<const RC<Node> &>(*this)); }

    HashCode GetHashCode() const;
};

static_assert(sizeof(NodeProxy) == sizeof(RC<Node>), "NodeProxy should be the same size as RC<Node>, to allow for type punning");

} // namespace hyperion

#endif
