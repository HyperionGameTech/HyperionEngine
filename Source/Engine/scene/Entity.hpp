/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/HashSet.hpp>

#include <Core/memory/AnyRef.hpp>

#include <Core/math/Mat4f.hpp>

#include <scene/Node.hpp>

namespace Hyperion {

class World;
class Scene;
class Node;
class EntityManager;
struct Transform;
struct BoxedValue;
enum class EntityTag : uint64;

struct EntityInitInfo
{
    bool receivesUpdate : 1 = false;
    bool canEverUpdate : 1 = true;
    uint8 bvhDepth : 3 = 3; // 0 means no BVH, 1 means 1 level deep, etc.

    // Initial tags to add to the Entity when it is created
    Array<EntityTag, InlineAllocator<4>> initialTags;
};

HYP_CLASS()
class HYP_API Entity : public Node
{
    HYP_OBJECT_BODY(Entity);

    friend class EntityRenderProxySystem_Mesh;

public:
    friend class EntityManager;
    friend class Node;

    Entity();
    explicit Entity(Name name);

    virtual ~Entity() override;

    HYP_METHOD()
    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    HYP_FORCE_INLINE const Mat4f& GetPrevModelMatrix() const
    {
        return m_prevModelMatrix;
    }

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

    HYP_METHOD(Property = "Components", NoScriptBindings, EditOrder = 1000)
    Array<BoxedValue, DynamicAllocator> SerializeComponents() const;

    HYP_METHOD(Property = "Components", NoScriptBindings, LoadOrder = 1000)
    void DeserializeComponents(const Array<BoxedValue, DynamicAllocator>& components);

    World* m_world;
    EntityManager* m_entityManager;

    Mat4f m_prevModelMatrix;

    int m_renderProxyVersion;

    // has the transform been updated since the Node's transform has been unlocked?
    bool m_transformChanged : 1;
};

#include <scene/Entity.inl>

} // namespace Hyperion
