/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/Set.hpp>

#include <Core/memory/AnyRef.hpp>

#include <Core/math/Mat4f.hpp>

#include <Scene/Node.hpp>
#include <Scene/EntityTag.hpp>

namespace Hyperion {

class World;
class Scene;
class Node;
class EntityManager;
struct Transform;
struct BoxedValue;

struct EntityInitInfo
{
    bool receivesUpdate : 1 = false;
    bool canEverUpdate : 1 = true;
    uint8 bvhDepth : 3 = 3; // 0 means no BVH, 1 means 1 level deep, etc.

    // Initial tags to add to the Entity when it is created
    Array<EntityTag, InlineAllocator<4>> initialTags;
};

HYP_CLASS(AssetBucket = "Entities")
class ENGINE_API Entity : public Node
{
    HYP_OBJECT_BODY(Entity);

public:
    friend class EntityManager;
    friend class Node;

    Entity();
    explicit Entity(Name name);

    virtual ~Entity() override;

    /*! \brief Clone this entity, including all components.
     *  \returns A handle to the cloned entity. */
    HYP_METHOD()
    virtual Handle<Node> Clone() const override;

    HYP_METHOD()
    HYP_FORCE_INLINE EntityManager* GetEntityManager() const
    {
        return m_entityManager;
    }

    template <class Component, class EntityManagerPtr = EntityManager*>
    Component& GetComponent() const;

    template <class Component, class EntityManagerPtr = EntityManager*>
    Component* TryGetComponent() const;

    template <class Component, class EntityManagerPtr = EntityManager*>
    bool HasComponent() const;

    template <class Component, class T = Component, class EntityManagerPtr = EntityManager*>
    Component& AddComponent(T&& component);

    template <class Component, class EntityManagerPtr = EntityManager*>
    bool RemoveComponent();

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    void AddTag();

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    bool RemoveTag();

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    bool HasTag() const;

    HYP_METHOD()
    bool ReceivesUpdate() const;

    HYP_METHOD()
    void SetReceivesUpdate(bool receivesUpdate);

    virtual void LockTransform() override;
    virtual void UnlockTransform() override;

    virtual void SetLocalBounds(const BoundingBox& aabb) override;

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

protected:
    virtual void Init() override;

    virtual void Update(float delta)
    {
    }

    virtual void OnAttachedToNode(Node* node) override;
    virtual void OnDetachedFromNode(Node* node) override;

    virtual void OnNodeAttached(Node* node) override;
    virtual void OnNodeDetached(Node* node) override;

    virtual void OnAddedToWorld(World* world);
    virtual void OnRemovedFromWorld(World* world);

    virtual void OnAddedToScene(Scene* scene);
    virtual void OnRemovedFromScene(Scene* scene);

    virtual void OnComponentAdded(AnyRef component);
    virtual void OnComponentRemoved(AnyRef component);

    virtual void OnTagAdded(EntityTag tag);
    virtual void OnTagRemoved(EntityTag tag);

    virtual void SetScene(Scene* scene) override;

    virtual void OnTransformUpdated() override;
    virtual void OnMobilityChanged(bool isStatic) override;

    EntityInitInfo m_entityInitInfo;

private:
    void SetEntityManager(const Handle<EntityManager>& entityManager);

    HYP_METHOD(Property = "Tags", NoScriptBindings)
    Array<EntityTag> SerializeTags() const;

    HYP_METHOD(Property = "Tags", NoScriptBindings, LoadOrder = 1001)
    void DeserializeTags(const Array<EntityTag>& tags);

    HYP_METHOD(Property = "Components", NoScriptBindings)
    Array<BoxedValue, DynamicAllocator> SerializeComponents() const;

    HYP_METHOD(Property = "Components", NoScriptBindings, LoadOrder = 1000)
    void DeserializeComponents(const Array<BoxedValue, DynamicAllocator>& components);

    EntityManager* m_entityManager;

    int m_renderProxyVersion;

    // has the transform been updated since the Node's transform has been unlocked?
    bool m_transformChanged : 1;
};

#include <Scene/Entity.inl>

} // namespace Hyperion
