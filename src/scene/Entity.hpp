/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <core/memory/AnyRef.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/RenderProxyable.hpp>

namespace hyperion {

class World;
class Scene;
class Node;
class EntityManager;
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
class HYP_API Entity : public RenderProxyable
{
    HYP_OBJECT_BODY(Entity);

    friend class EntityRenderProxySystem_Mesh;

public:
    friend class EntityManager;
    friend class Node;

    Entity();
    virtual ~Entity() override;

    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_scene;
    }

    HYP_FORCE_INLINE const Matrix4& GetPrevModelMatrix() const
    {
        return m_prevModelMatrix;
    }

    EntityManager* GetEntityManager() const;

    bool ReceivesUpdate() const;
    void SetReceivesUpdate(bool receivesUpdate);

    /*! \brief Attaches this Entity to a Node. If the Entity is already attached to a Node, it will be detached first.
     *
     *  \param [in] attachNode The Node to attach the Entity to.
     */
    HYP_METHOD()
    virtual void AttachTo(const Handle<Node>& attachNode);

    /*! \brief Detaches this Entity from its current Node, if it is attached to one.
     *
     *  \note This will not remove the Entity from the EntityManager.
     */
    HYP_METHOD()
    virtual void Detach();

protected:
    virtual void Init() override;

    virtual void Update(float delta)
    {
    }

    virtual void OnAttachedToNode(Node* node);
    virtual void OnDetachedFromNode(Node* node);

    virtual void OnAddedToWorld(World* world);
    virtual void OnRemovedFromWorld(World* world);

    virtual void OnAddedToScene(Scene* scene);
    virtual void OnRemovedFromScene(Scene* scene);

    virtual void OnComponentAdded(AnyRef component);
    virtual void OnComponentRemoved(AnyRef component);

    virtual void OnTagAdded(EntityTag tag);
    virtual void OnTagRemoved(EntityTag tag);

    void AttachChild(const Handle<Entity>& child);
    void DetachChild(const Handle<Entity>& child);

    EntityInitInfo m_entityInitInfo;

private:
    World* m_world;
    Scene* m_scene;

    Matrix4 m_prevModelMatrix;
};

} // namespace hyperion

