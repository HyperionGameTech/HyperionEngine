/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Utilities/Uuid.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/StringView.hpp>
#include <Core/Utilities/Variant.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/Ray.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Scene/EntityTag.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class Scene;
class Node;
class World;
class Entity;
class EditorDelegates;

HYP_ENUM()
enum class TransformChangeType : uint8
{
    Default = 0,   //!< Default transform change, marks the node as dirty so the transform is saved and the editor is aware of the change when not in simulation mode.
    Simulation = 1 //!< Transform change caused by physics or other simulation (e.g scripts) - should not mark the node as modified.
};

// clang-format off

HYP_ENUM()
enum class NodeFlags : uint32
{
    None = 0x0,                                                                                 //!< @editor=false

    IgnoreParentTranslation = 0x1,                                                              //!< @title="Ignores parent translation"
    IgnoreParentScale = 0x2,                                                                    //!< @title="Ignores parent scaling"
    IgnoreParentRotation = 0x4,                                                                 //!< @title="Ignores parent rotation"
    IgnoreParentTransform = IgnoreParentTranslation | IgnoreParentScale | IgnoreParentRotation, //!< @editor=false

    ExcludeFromParentBounds = 0x8,                                                              //!< @title="Does not affect parent node's bounds"
    ExcludeFromOctree = 0x10,                                                                   //!< @title="Not included in the Scene's octree"

    HideInSceneOutline = 0x1000,                                                                //!< @editor=false

    Mobility = 0xE000,                                                                          //!< @editor=false
    MobilityStatic = 0x2000,                                                                    //!< @editor=false
    MobilityStaticByProxy = 0x4000,                                                             //!< @editor=false
    MobilityDynamic = Mobility & ~(MobilityStatic | MobilityStaticByProxy),                     //!< @editor=false

    Default = MobilityStatic                                                                    //!< @editor=false
};

// clang-format on

HYP_MAKE_ENUM_FLAGS(NodeFlags);

HYP_STRUCT()
struct NodeTag
{
    HYP_STRUCT_BODY(NodeTag);

    using VariantType = Variant<
        int,
        float,
        Vec2f, Vec3f, Vec4f,
        Vec2i, Vec3i, Vec4i,
        Vec2u, Vec3u, Vec4u,
        String,
        Name,
        UUID>;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name;

    HYP_FIELD(Property = "Data", Serialize = true)
    VariantType data;

    NodeTag() = default;

    NodeTag(Name name, const VariantType& data)
        : name(name),
          data(data)
    {
    }

    NodeTag(Name name, VariantType&& data)
        : name(name),
          data(std::move(data))
    {
    }

    template <class T>
    NodeTag(Name name, T&& value)
        : name(name),
          data(std::forward<T>(value))
    {
    }

    NodeTag(const NodeTag& other)
        : name(other.name),
          data(other.data)
    {
    }

    NodeTag& operator=(const NodeTag& other)
    {
        if (this == &other)
        {
            return *this;
        }

        name = other.name;
        data = other.data;

        return *this;
    }

    NodeTag(NodeTag&& other) noexcept
        : name(std::move(other.name)),
          data(std::move(other.data))
    {
    }

    NodeTag& operator=(NodeTag&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        name = std::move(other.name);
        data = std::move(other.data);

        return *this;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return data.IsValid();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !data.IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return data.IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const NodeTag& other) const
    {
        return name == other.name && data == other.data;
    }

    HYP_FORCE_INLINE bool operator!=(const NodeTag& other) const
    {
        return name != other.name || data != other.data;
    }

    ENGINE_API String ToString() const;
};

struct NodeUnlockTransformScope;

HYP_STRUCT()
class NodeTagSet : HashTable<NodeTag, &NodeTag::name>
{
    HYP_STRUCT_BODY(NodeTagSet);

public:
    using Base = HashTable<NodeTag, &NodeTag::name>;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    using Base::Contains;

    NodeTagSet() = default;

    NodeTagSet(const Base& other)
        : Base(other)
    {
    }

    NodeTagSet(Base&& other) noexcept
        : Base(std::move(other))
    {
    }

    NodeTagSet(const NodeTagSet& other) = default;
    NodeTagSet& operator=(const NodeTagSet& other) = default;

    NodeTagSet(NodeTagSet&& other) noexcept = default;
    NodeTagSet& operator=(NodeTagSet&& other) noexcept = default;

    HYP_FORCE_INLINE bool Has(StringHash name) const
    {
        return Base::Contains(Name(name));
    }

    HYP_FORCE_INLINE const NodeTag* Get(StringHash name) const
    {
        const auto it = Base::Find(Name(name));

        if (it != Base::End())
        {
            return &(*it);
        }

        return nullptr;
    }

    HYP_FORCE_INLINE NodeTag* Get(Name name)
    {
        return const_cast<NodeTag*>(static_cast<const NodeTagSet*>(this)->Get(name));
    }

    template <class T, class... Args>
    NodeTag* Add(Name name, Args&&... args)
    {
        auto [it, inserted] = Base::Insert(NodeTag { name, NodeTag::VariantType(std::forward<Args>(args)...) });

        if (!inserted)
        {
            // update existing tag
            it->data = NodeTag::VariantType(std::forward<Args>(args)...);
        }

        return &(*it);
    }

    NodeTag* Add(const NodeTag& tag)
    {
        NodeTag* pExistingTag = Get(tag.name);

        if (pExistingTag != nullptr)
        {
            // update existing tag
            pExistingTag->data = tag.data;

            return pExistingTag;
        }

        auto [it, inserted] = Base::Insert(tag);

        return &(*it);
    }

    NodeTag* Add(NodeTag&& tag)
    {
        NodeTag* pExistingTag = Get(tag.name);

        if (pExistingTag != nullptr)
        {
            // update existing tag
            pExistingTag->data = std::move(tag.data);

            return pExistingTag;
        }

        auto [it, inserted] = Base::Insert(std::move(tag));

        return &(*it);
    }

    bool Remove(StringHash name)
    {
        return Base::Erase(Name(name));
    }

    void Clear()
    {
        Base::Clear();
    }

    String ToString() const
    {
        String result = "{ ";

        for (const auto& tag : *this)
        {
            result += tag.ToString() + " ";
        }

        result += "}";

        return result;
    }

    /// Serialization
    HYP_METHOD(Property = "Values", Serialize = true, NoScriptBindings)
    Array<NodeTag, DynamicAllocator> SerializeValues() const
    {
        Array<NodeTag, DynamicAllocator> result;
        result.Reserve(m_size);

        for (const NodeTag& tag : *this)
        {
            result.PushBack(tag);
        }

        return result;
    }

    HYP_METHOD(Property = "Values", Serialize = true, NoScriptBindings)
    void DeserializeValues(const Array<NodeTag, DynamicAllocator>& values)
    {
        Reserve(values.Size());

        for (const NodeTag& tag : values)
        {
            Add(tag);
        }
    }
    /// End Serialization

    HYP_DEF_STL_BEGIN_END(Base::Begin(), Base::End())
};

HYP_CLASS()
class ENGINE_API Node : public ObjectBase
{
    friend class Scene;
    friend class Entity;

    HYP_OBJECT_BODY(Node);

public:
    class DescendantsIterator
    {
    public:
        DescendantsIterator()
            : m_current(nullptr)
        {
        }

        explicit DescendantsIterator(const Node* root)
            : m_current(nullptr)
        {
            if (root != nullptr && root->GetChildren().Any())
            {
                m_stack.Reserve(8);

                // Start with first child of root
                m_stack.PushBack({ root, 0 });
                ++(*this);
            }
        }

        DescendantsIterator& operator++()
        {
            while (m_stack.Any())
            {
                auto& [node, index] = m_stack.Back();

                if (index < node->GetChildren().Size())
                {
                    const Handle<Node>& child = node->GetChildren()[index];
                    ++index;

                    if (child.IsValid())
                    {
                        m_current = child.Get();

                        if (m_current->GetChildren().Any())
                        {
                            m_stack.PushBack({ m_current, 0 });
                        }

                        return *this;
                    }
                }
                else
                {
                    m_stack.PopBack();
                }
            }

            m_current = nullptr;
            return *this;
        }

        // Deleted to reduce overhead and consumption of the memory allocation for stack data.
        DescendantsIterator operator++(int) = delete;

        HYP_FORCE_INLINE Node* operator*() const
        {
            return m_current;
        }

        HYP_FORCE_INLINE Node* operator->() const
        {
            return m_current;
        }

        HYP_FORCE_INLINE bool operator==(const DescendantsIterator& other) const
        {
            return m_current == other.m_current;
        }

        HYP_FORCE_INLINE bool operator!=(const DescendantsIterator& other) const
        {
            return m_current != other.m_current;
        }

    private:
        struct StackEntry
        {
            const Node* node;
            size_t index;
        };

        Node* m_current;
        Array<StackEntry, ThreadAllocator> m_stack;
    };

    class DescendantsView
    {
    public:
        explicit DescendantsView(const Node* node)
            : m_node(node)
        {
        }

        DescendantsIterator Begin() const
        {
            return DescendantsIterator(m_node);
        }

        DescendantsIterator End() const
        {
            return DescendantsIterator();
        }

        DescendantsIterator begin() const
        {
            return Begin();
        }
        DescendantsIterator end() const
        {
            return End();
        }

        /*! \brief Get the number of descendant nodes.
         *  \note This method traverses the entire tree to count descendants, so it has O(n) complexity.
         *  \returns The total count of descendant nodes. */
        size_t Size() const
        {
            size_t count = 0;

            for (auto it = Begin(); it != End(); ++it)
            {
                ++count;
            }

            return count;
        }

        /*! \brief Get a descendant node by index.
         *  \param index The index of the descendant to retrieve (0-based).
         *  \note This method traverses the tree up to the specified index, so it has O(index) complexity.
         *        For sequential access, prefer using iterators directly.
         *  \returns A pointer to the node at the specified index, or nullptr if the index is out of bounds. */
        Node* operator[](size_t index) const
        {
            size_t currentIndex = 0;

            for (auto it = Begin(); it != End(); ++it)
            {
                if (currentIndex == index)
                {
                    return *it;
                }

                ++currentIndex;
            }

            return nullptr;
        }

    private:
        const Node* m_node;
    };

    using NodeList = Array<Handle<Node>, SceneAllocator>;

    /*! \brief Construct the node, optionally taking in a name string to improve identification.
     * \param name The name of the Node.
     * \param localTransform An optional parameter representing the local-space transform of this Node.
     */
    explicit Node(Name name = Name::Invalid(), const Transform& localTransform = Transform(), Scene* scene = nullptr);

    Node(const Node& other) = delete;
    Node& operator=(const Node& other) = delete;
    Node(Node&& other) noexcept = delete;
    Node& operator=(Node&& other) noexcept = delete;

    virtual ~Node() override;

    /*! \brief Clone this node, creating a new instance with the same properties.
     *  \returns A handle to the cloned node. */
    HYP_METHOD()
    virtual Handle<Node> Clone() const;

    HYP_METHOD()
    bool HasName() const;

    HYP_METHOD(Property = "Name", Serialize, EditorOrder = 1)
    Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name")
    virtual void SetName(Name name)
    {
        if (name == m_name)
        {
            return;
        }

        m_name = name;

        MarkDirty();
    }

    /*! \brief Get the flags of the Node.
     *  \see NodeFlagBits
     *  \returns The flags of the Node. */
    HYP_METHOD(Property = "NodeFlags", Serialize, EditorOrder = 3)
    HYP_FORCE_INLINE EnumFlags<NodeFlags> GetNodeFlags() const
    {
        return m_nodeFlags;
    }

    /*! \brief Set the flags of the Node.
     *  \see NodeFlagBits
     *  \param flags The flags to set on the Node. */
    HYP_METHOD(Property = "NodeFlags")
    void SetNodeFlags(EnumFlags<NodeFlags> flags);

    HYP_METHOD(Property = "Parent", Transient)
    HYP_FORCE_INLINE Node* GetParent() const
    {
        return m_parentNode;
    }

    HYP_METHOD()
    bool IsOrHasParent(const Node* node) const;

    HYP_METHOD()
    Node* FindParentWithName(UTF8StringView name) const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsRoot() const
    {
        return m_parentNode == nullptr;
    }

    /*! \returns A pointer to the Scene this Node and its children are attached to. May be null. */
    HYP_METHOD(Property = "Scene", Transient, Editor = false)
    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_scene;
    }

    /*! \brief Set the Scene this Node and its children are attached to. If nullptr is provided and \p moveToDetached is true, the Node will be moved to a detached Scene.
     *  \internal Not intended to be used in user code. Use Remove() instead. */
    HYP_METHOD(Property = "Scene", Transient, Editor = false)
    void SetScene(Scene* scene, bool moveToDetached = true);

    /*! \returns A pointer to the World this Node and its children are attached to. May be null. */
    HYP_METHOD(Property = "World", Transient, Editor = false)
    World* GetWorld() const;

    /*! \brief \returns The local bounds for this node, not considering child nodes or transform. */
    HYP_METHOD(Property = "LocalBounds")
    HYP_FORCE_INLINE const BoundingBox& GetLocalBounds() const
    {
        return m_localBounds;
    }

    /*! \brief Set the bounds of the Node's content. Does not update the Entity's BoundingBoxComponent.
     *   \param aabb The bounding box to set
     *   \note Calls to RefreshEntityTransform() will override this value. */
    HYP_METHOD(Property = "LocalBounds")
    virtual void SetLocalBounds(const BoundingBox& aabb);

    /*! \brief Add the Node as a child of this object, taking ownership over the given Node.
     *  \param node The Node to be added as achild of this Node
     *  \returns The added Node */
    HYP_METHOD()
    Handle<Node> AddChild(const Handle<Node>& node = {});

    /*! \brief Remove a child from this Node's child list.
     *  \param node The child Node to remove from this Node's child list.
     *  \param moveToDetached if true, will move the node to a detached scene, otherwise will set its Scene to nullptr.
     *  \returns Whether then removal was successful */
    bool RemoveChild(const Node* node, bool moveToDetached = true);

    /*! \brief Remove a child at the given index
     *  \param index The index of the child element to remove
     *  \param moveToDetached if true, will move the node to a detached scene, otherwise will set its Scene to nullptr.
     *  \returns Whether then removal was successful */
    HYP_METHOD()
    bool RemoveAt(uint32 index, bool moveToDetached = true);

    /*! \brief Remove this node from the parent Node's list of child Nodes.
     *  \returns Whether the removal was successful. */
    HYP_METHOD()
    bool Remove(bool moveToDetached = true);

    HYP_METHOD()
    void RemoveAllChildren(bool moveToDetached = true);

    HYP_METHOD()
    uint32 NumChildren() const;

    /*! \brief Get a child Node from this Node's child list at the given index.
     *  \param index The index of the child element to return
     *  \returns The child node at the given index. If the index is out of bounds, nullptr
     *  will be returned. */
    HYP_METHOD()
    Handle<Node> GetChild(uint32 index) const;

    /*! \brief Search for a (potentially nested) node using the syntax `some/child/node`.
     *  Each `/` indicates searching a level deeper, so first a child node with the name "some"
     *  is found, after which a child node with the name "child" is searched for on the previous "some" node,
     *  and finally a node with the name "node" is searched for from the above "child" node.
     *  If any level fails to find a node, nullptr is returned.
     *
     *  The string is case-sensitive.
     *  The '/' can be escaped by using a '\' char. */
    HYP_METHOD()
    Handle<Node> Select(ANSIStringView selector) const;

    HYP_METHOD()
    Array<Name> GetDeepPath() const;

    /*! \brief Get an iterator for the given child Node from this Node's child list
     * \param node The node to find in this Node's child list
     * \returns The resulting iterator */
    NodeList::Iterator FindChild(const Node* node);

    /*! \brief Get an iterator for the given child Node from this Node's child list
     * \param node The node to find in this Node's child list
     * \returns The resulting iterator */
    NodeList::ConstIterator FindChild(const Node* node) const;

    /*! \brief Get an iterator for a node by finding it by its name
     * \param name The string to compare with the child Node's name
     * \returns The resulting iterator */
    NodeList::Iterator FindChild(const char* name);

    /*! \brief Get an iterator for a node by finding it by its name
     * \param name The string to compare with the child Node's name
     * \returns The resulting iterator */
    NodeList::ConstIterator FindChild(const char* name) const;

    /*! \brief Get the child Nodes of this Node.
     *  \returns Array of child Nodes */
    HYP_FORCE_INLINE const NodeList& GetChildren() const
    {
        return m_childNodes;
    }

    /*! \brief Get all descendant child Nodes from this Node.
     * \returns A view that can be iterated over to traverse all descendant nodes */
    DescendantsView GetDescendants() const
    {
        return DescendantsView(this);
    }

    /*! \brief Get all descendant child Nodes from this Node as an array.
     * \returns A vector of raw pointers to descendant Nodes */
    Array<Node*> GetDescendantsArray() const;

    /*! \brief Set the local-space translation, scale, rotation of this Node (not influenced by the parent Node) */
    HYP_METHOD()
    void SetLocalTransform(const Transform& transform, TransformChangeType changeType = TransformChangeType::Default);

#ifdef HYP_EDITOR
    /*! \brief Needed to have changes reflected from editor */
    HYP_METHOD(Property = "LocalTransform", EditorOnly)
    HYP_FORCE_INLINE void SetLocalTransform_EditorOnly(const Transform& transform)
    {
        SetLocalTransform(transform);
    }
#endif // HYP_EDITOR

    /*! \returns The local-space translation, scale, rotation of this Node. */
    HYP_METHOD(Property = "LocalTransform")
    HYP_FORCE_INLINE const Transform& GetLocalTransform() const
    {
        return m_localTransform;
    }

    /*! \returns The local-space translation of this Node. */
    HYP_METHOD(Property = "LocalTranslation", Transient, Editor = false)
    HYP_FORCE_INLINE const Vec3f& GetLocalTranslation() const
    {
        return m_localTransform.GetTranslation();
    }

    /*! \brief Set the local-space translation of this Node (not influenced by the parent Node) */
    HYP_METHOD(Property = "LocalTranslation", Transient)
    HYP_FORCE_INLINE void SetLocalTranslation(const Vec3f& translation, TransformChangeType changeType = TransformChangeType::Default)
    {
        SetLocalTransform(Transform { translation, m_localTransform.GetScale(), m_localTransform.GetRotation() }, changeType);
    }

    /*! \brief Move the Node in local-space by adding the given vector to the current local-space translation.
     * \param translation The vector to translate this Node by
     */
    HYP_METHOD()
    HYP_FORCE_INLINE void Translate(const Vec3f& translation)
    {
        SetLocalTranslation(m_localTransform.GetTranslation() + translation);
    }

    /*! \returns The local-space scale of this Node. */
    HYP_METHOD(Property = "LocalScale", Transient, Editor = false)
    HYP_FORCE_INLINE const Vec3f& GetLocalScale() const
    {
        return m_localTransform.GetScale();
    }

    /*! \brief Set the local-space scale of this Node (not influenced by the parent Node) */
    HYP_METHOD(Property = "LocalScale", Transient, Editor = false)
    HYP_FORCE_INLINE void SetLocalScale(const Vec3f& scale, TransformChangeType changeType = TransformChangeType::Default)
    {
        SetLocalTransform(Transform { m_localTransform.GetTranslation(), scale, m_localTransform.GetRotation() }, changeType);
    }

    /*! \brief Scale the Node in local-space by multiplying the current local-space scale by the given scale vector.
     * \param scale The vector to scale this Node by */
    HYP_METHOD()
    HYP_FORCE_INLINE void Scale(const Vec3f& scale)
    {
        SetLocalScale(m_localTransform.GetScale() * scale);
    }

    /*! \returns The local-space rotation of this Node. */
    HYP_METHOD(Property = "LocalRotation", Transient, Editor = false)
    HYP_FORCE_INLINE const Quat4f& GetLocalRotation() const
    {
        return m_localTransform.GetRotation();
    }

    /*! \brief Set the local-space rotation of this Node (not influenced by the parent Node) */
    HYP_METHOD(Property = "LocalRotation", Transient, Editor = false)
    HYP_FORCE_INLINE void SetLocalRotation(const Quat4f& rotation, TransformChangeType changeType = TransformChangeType::Default)
    {
        SetLocalTransform(Transform { m_localTransform.GetTranslation(), m_localTransform.GetScale(), rotation }, changeType);
    }

    /*! \brief Rotate the Node by multiplying the current local-space rotation by the given quaternion.
     * \param rotation The quaternion to rotate this Node by */
    HYP_METHOD()
    HYP_FORCE_INLINE void Rotate(const Quat4f& rotation)
    {
        SetLocalRotation(m_localTransform.GetRotation() * rotation);
    }

    /*! \brief \returns The world-space matrix */
    HYP_METHOD(Property = "WorldMatrix", Transient, Editor = false)
    const Mat4f& GetWorldMatrix() const
    {
        return m_worldMatrix;
    }

    /*! \returns The world-space translation of this Node. */
    HYP_METHOD(Property = "WorldTranslation", Transient, Editor = false)
    Vec3f GetWorldTranslation() const;

    /*! \brief Set the world-space translation of this Node by offsetting the local-space translation */
    HYP_METHOD(Property = "WorldTranslation", Transient)
    void SetWorldTranslation(const Vec3f& translation, TransformChangeType changeType = TransformChangeType::Default);

    /*! \returns The local-space scale of this Node. */
    HYP_METHOD(Property = "WorldScale", Transient, Editor = false)
    Vec3f GetWorldScale() const;

    /*! \brief Set the local-space scale of this Node by offsetting the local-space scale */
    HYP_METHOD(Property = "WorldScale", Transient, Editor = false)
    void SetWorldScale(const Vec3f& scale, TransformChangeType changeType = TransformChangeType::Default);

    /*! \returns The world-space rotation of this Node. */
    HYP_METHOD(Property = "WorldRotation", Transient, Editor = false)
    Quat4f GetWorldRotation() const;

    /*! \brief Set the world-space rotation of this Node by offsetting the local-space rotation */
    HYP_METHOD(Property = "WorldRotation", Transient, Editor = false)
    void SetWorldRotation(const Quat4f& rotation, TransformChangeType changeType = TransformChangeType::Default);

    /*! \brief Returns whether the Node is locked from being transformed. */
    HYP_METHOD()
    HYP_FORCE_INLINE bool IsTransformLocked() const
    {
        return m_transformLocked;
    }

    /*! \brief Lock the Node from being transformed. */
    virtual void LockTransform();

    /*! \brief Unlock the Node from being transformed. */
    virtual void UnlockTransform();

    HYP_FORCE_INLINE bool IsStatic() const
    {
        return bool(m_nodeFlags & NodeFlags::MobilityStatic);
    }

    void SetIsStatic(bool isStatic);

    HYP_FORCE_INLINE bool IsDynamic() const
    {
        return !IsStatic();
    }

    void SetIsDynamic(bool isDynamic);

    /*! \brief The local-space axis-aligned bounding box of the node, extended to include the bounds of all child nodes (with their local-space transforms applied). */
    HYP_METHOD()
    BoundingBox GetLocalBoundsWithChildren() const;

    /*! \brief The axis-aligned bounding box of the node in world-space.
     *  Includes the bounds of all child nodes and is transformed by ancestor transforms to compose the final world-space AABB. */
    HYP_METHOD(Property = "WorldBounds", EditEnabled = false, Transient)
    BoundingBox GetWorldBounds() const;

    /*! \brief Update the world transform of the Node to reflect changes in the local transform and parent transform.
     *  This will update the TransformComponent of the entity if it exists. */
    HYP_METHOD()
    void UpdateWorldTransform(bool updateChildTransforms = true);

    /*! \brief Calculate the depth of the Node relative to the root Node.
     * \returns The depth of the Node relative to the root Node. If the Node has no parent, 0 is returned. */
    HYP_METHOD()
    uint32 CalculateDepth() const;

    /*! \brief Calculate the index of this Node in its parent's child list.
     * \returns The index of this Node in its parent's child list. If the Node has no parent, -1 is returned. */
    HYP_METHOD()
    uint32 FindSelfIndex() const;

    bool TestRay(const Ray& ray, RayTestResults& outResults, EnumFlags<RayTestFlags> flags = RayTestFlags::TestBVH) const;

    /*! \brief Search child nodes (breadth-first) until a node with the given name is found. */
    HYP_METHOD()
    const Handle<Node>& FindChildByName(StringHash name) const;

    HYP_FORCE_INLINE const NodeTagSet& GetTags() const
    {
        return m_tags;
    }

    /*! \brief Add a tag to this Node. */
    void AddTag(NodeTag&& value);

    /*! \brief Remove a tag from this Node.
     *  \param key The key the tag
     *  \returns Whether the tag with the given key was successfully removed or not */
    bool RemoveTag(StringHash key);

    /*! \brief Get a tag from this Node.
     *  \param key The key the tag
     *  \returns The tag with the given key. If the tag does not exist, an empty NodeTag is returned */
    const NodeTag& GetTag(StringHash key) const;

    /*! \brief Check if this Node has a tag with the given name.
     *  \param key The key the tag
     *  \returns True if the tag exists, false otherwise. */
    bool HasTag(StringHash key) const;

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly)
    void MarkDirty();
#else  // !HYP_EDITOR
    static constexpr NoOpFunction<void> MarkDirty;
#endif // HYP_EDITOR

    HYP_FIELD()
    static ScriptableDelegate<void, Node*, bool /* direct */> OnChildAdded;

    HYP_FIELD()
    static ScriptableDelegate<void, Node*, bool /* direct */> OnChildRemoved;

    HYP_FIELD()
    static ScriptableDelegate<void, Node*> TransformUpdated;

protected:
    virtual void Init() override;

    virtual void OnAttachedToNode(Node* node);
    virtual void OnDetachedFromNode(Node* node);

    virtual void OnNodeAttached(Node* node);
    virtual void OnNodeDetached(Node* node);

    virtual void OnTransformUpdated();
    virtual void OnMobilityChanged(bool isStatic);

    virtual void SetScene_Internal(Scene* scene, bool moveToDetached);

    HYP_METHOD(Property = "Children", NoScriptBindings, Serialize)
    void SetChildren(const NodeList& children); // use setter so we can manage parent pointers

    HYP_FIELD(Property = "Name", Serialize)
    Name m_name;

    HYP_FIELD(Property = "NodeFlags", Serialize)
    EnumFlags<NodeFlags> m_nodeFlags;

    HYP_FIELD(Property = "Parent", Transient, Editor = false)
    Node* m_parentNode;

    HYP_FIELD(Property = "Children", LoadOrder = -1, Editor = false, Serialize)
    NodeList m_childNodes;

    HYP_FIELD(Property = "LocalTransform", Serialize, Label = "Local-space Transform")
    Transform m_localTransform;

    HYP_FIELD(Transient, Editor = false)
    Mat4f m_worldMatrix;

    HYP_FIELD(Property = "LocalBounds", Serialize, Label = "Bounding Box", Description = "The bounds for the content of this node. Does not take into account child nodes or transform.")
    BoundingBox m_localBounds;

    HYP_FIELD(Property = "Scene", Transient, Editor = false)
    Scene* m_scene;

    bool m_transformLocked : 1;

    HYP_FIELD(Property = "NodeTags", Serialize)
    NodeTagSet m_tags;
};

struct NodeUnlockTransformScope
{
    NodeUnlockTransformScope(Node& node)
        : node(node),
          locked(node.IsTransformLocked())
    {
        if (locked)
        {
            node.UnlockTransform();
        }
    }

    ~NodeUnlockTransformScope()
    {
        if (locked)
        {
            node.LockTransform();
        }
    }

    Node& node;
    bool locked;
};

} // namespace Hyperion
