#ifndef HYPERION_V2_NODE_PROXY_HPP
#define HYPERION_V2_NODE_PROXY_HPP

#include <core/Containers.hpp>
#include <core/HandleID.hpp>
#include <scene/Entity.hpp>
#include <math/Transform.hpp>
#include <math/Ray.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Engine;
class Scene;
class Node;

class NodeProxy;

struct NodeProxyChildren
{
    struct IteratorBase
    {
        const Node *node;
        SizeType index;

        IteratorBase &operator++()
        {
            ++index;

            return *this;
        }
    };

    struct Iterator : IteratorBase
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

    struct ConstIterator : IteratorBase
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

    SizeType Size() const;

    Iterator Begin();
    ConstIterator Begin() const;
    Iterator End();
    ConstIterator End() const;

    Iterator begin() { return Begin(); }
    Iterator end() { return End(); }
    ConstIterator cbegin() const { return Begin(); }
    ConstIterator cend() const { return End(); }
};

class NodeProxy : AtomicRefCountedPtr<Node>
{
protected:
    using Base = AtomicRefCountedPtr<Node>;

public:
    static const NodeProxy empty;

    NodeProxy();
    /*! \brief Takes ownership of the Node ptr */
    explicit NodeProxy(Node *ptr);
    explicit NodeProxy(const Base &other);
    NodeProxy(const NodeProxy &other);
    NodeProxy &operator=(const NodeProxy &other);
    NodeProxy(NodeProxy &&other) noexcept;
    NodeProxy &operator=(NodeProxy &&other) noexcept;
    ~NodeProxy();

    HYP_FORCE_INLINE Node *Get() { return Base::Get(); }
    HYP_FORCE_INLINE const Node *Get() const { return Base::Get(); }

    HYP_FORCE_INLINE bool Any() const { return Get() != nullptr; }
    HYP_FORCE_INLINE bool Empty() const { return Get() == nullptr; }

    /*! \brief Conversion operator to bool, to use in if-statements */
    explicit operator bool() const { return Any(); }

    bool operator!() const { return Empty(); }

    bool operator==(const NodeProxy &other) const
        { return Get() == other.Get(); }

    bool operator!=(const NodeProxy &other) const
        { return Get() != other.Get(); }

    NodeProxy operator[](SizeType index)
        { return GetChild(index); }

    /*! \brief If the Node exists, returns the String name of the Node.
        Otherwise, returns the empty String. */
    const String &GetName() const;

    /*! \brief If the Node exists, sets the name of the Node. */
    void SetName(const String &name);

    /* \brief If the NodeProxy is not empty, returns the Entity, if any,
        attached to the underlying Node. */
    const Handle<Entity> &GetEntity() const;

    /*! \brief If the Node exists, sets the Entity attached to the Node. */
    void SetEntity(const Handle<Entity> &entity);

    /*! \brief If the Node exists, sets the Entity attached to the Node. */
    void SetEntity(Handle<Entity> &&entity);

    /*! \brief If the NodeProxy is not empty, and index is not out of bounds,
        returns a new NodeProxy, holding a pointer to the child of the held Node at the given index.
        If the index is out of bounds, returns an empty node proxy.
        If no Node is held, returns an empty NodeProxy. */
    NodeProxy GetChild(SizeType index);

    NodeProxyChildren GetChildren() { return NodeProxyChildren { const_cast<const Node *>(Base::Get()) }; }
    const NodeProxyChildren GetChildren() const { return NodeProxyChildren { Base::Get() }; }

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

    /*! @returns The local-space translation, scale, rotation of this Node. */
    const Transform &GetLocalTransform() const;

    /*! @returns The local-space translation of this Node. */
    const Vector3 &GetLocalTranslation() const;

    /*! \brief Set the local-space translation of this Node (not influenced by the parent Node) */
    void SetLocalTranslation(const Vector3 &translation);
    
    /*! \brief Move the Node in local-space by adding the given vector to the current local-space translation.
     * @param translation The vector to translate this Node by
     */
    void Translate(const Vector3 &translation)
        { SetLocalTranslation(GetLocalTranslation() + translation); }

    /*! @returns The local-space scale of this Node. */
    const Vector3 &GetLocalScale() const;

    /*! \brief Set the local-space scale of this Node (not influenced by the parent Node) */
    void SetLocalScale(const Vector3 &scale);

    /*! \brief Scale the Node in local-space by multiplying the current local-space scale by the given scale vector.
     * @param scale The vector to scale this Node by
     */
    void Scale(const Vector3 &scale)
        { SetLocalScale(GetLocalScale() * scale); }
    
    /*! @returns The local-space rotation of this Node. */
    const Quaternion &GetLocalRotation() const;

    /*! \brief Set the local-space rotation of this Node (not influenced by the parent Node) */
    void SetLocalRotation(const Quaternion &rotation);

    /*! \brief Rotate the Node by multiplying the current local-space rotation by the given quaternion.
     * @param rotation The quaternion to rotate this Node by
     */
    void Rotate(const Quaternion &rotation)
        { SetLocalRotation(GetLocalRotation() * rotation); }

    /*! @returns The world-space translation, scale, rotation of this Node. Influenced by accumulative transformation of all ancestor Nodes. */
    const Transform &GetWorldTransform() const;

    /*! @returns The world-space translation of this Node. */
    const Vector3 &GetWorldTranslation() const;

    /*! \brief Set the world-space translation of this Node by offsetting the local-space translation */
    void SetWorldTranslation(const Vector3 &translation);

    /*! @returns The local-space scale of this Node. */
    const Vector3 &GetWorldScale() const;

    /*! \brief Set the local-space scale of this Node by offsetting the local-space scale */
    void SetWorldScale(const Vector3 &scale);

    /*! @returns The world-space rotation of this Node. */
    const Quaternion &GetWorldRotation() const;

    /*! \brief Set the world-space rotation of this Node by offsetting the local-space rotation */
    void SetWorldRotation(const Quaternion &rotation);

    /*! @returns The local-space (model) of the node's aabb. Only includes
     * the Entity's aabb.
     */
    const BoundingBox &GetLocalAABB() const;

    /*! @returns The world-space aabb of the node. Includes the transforms of all
     * parent nodes.
     */
    const BoundingBox &GetWorldAABB() const;

    HashCode GetHashCode() const;

private:
    // void ReleaseNode();

    // Node *Get();
};

} // namespace hyperion::v2

#endif