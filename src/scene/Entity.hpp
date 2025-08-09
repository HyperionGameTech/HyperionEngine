/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <core/memory/AnyRef.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Matrix4.hpp>

#include <scene/Node.hpp>

namespace hyperion {

class World;
class Scene;
class Node;
class EntityManager;
struct Transform;
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
    virtual ~Entity() override;

    HYP_FORCE_INLINE const Matrix4& GetPrevModelMatrix() const
    {
        return m_prevModelMatrix;
    }

    EntityManager* GetEntityManager() const;

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

    bool ReceivesUpdate() const;
    void SetReceivesUpdate(bool receivesUpdate);

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

    virtual void OnAddedToWorld(World* world);
    virtual void OnRemovedFromWorld(World* world);

    virtual void OnAddedToScene(Scene* scene);
    virtual void OnRemovedFromScene(Scene* scene);

    virtual void OnComponentAdded(AnyRef component);
    virtual void OnComponentRemoved(AnyRef component);

    virtual void OnTagAdded(EntityTag tag);
    virtual void OnTagRemoved(EntityTag tag);

    virtual void OnTransformUpdated(const Transform& transform);

    EntityInitInfo m_entityInitInfo;

private:
    Matrix4 m_prevModelMatrix;

    int m_renderProxyVersion;
};

#include <scene/Entity.inl>

} // namespace hyperion

