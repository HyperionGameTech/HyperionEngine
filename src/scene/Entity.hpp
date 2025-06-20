/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_HPP
#define HYPERION_ENTITY_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/containers/Array.hpp>

#include <core/object/HypObject.hpp>

namespace hyperion {

class World;
class Scene;
class Node;
class EntityManager;

HYP_CLASS()
class HYP_API Entity : public HypObject<Entity>
{
    HYP_OBJECT_BODY(Entity);

public:
    friend class EntityManager;
    friend class Node;

    HYP_API Entity();
    HYP_API virtual ~Entity() override;

    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_scene;
    }

    EntityManager* GetEntityManager() const;

    void SetReceivesUpdate(bool receives_update);

    /*! \brief Attaches this Entity to a Node. If the Entity is already attached to a Node, it will be detached first.
     *
     *  \param [in] attach_node The Node to attach the Entity to.
     */
    virtual void Attach(const Handle<Node>& attach_node);

    /*! \brief Detaches this Entity from its current Node, if it is attached to one.
     *
     *  \note This will not remove the Entity from the EntityManager.
     */
    virtual void Detach();

protected:
    virtual void Init() override;

    virtual bool ReceivesUpdate() const
    {
        return false;
    }

    virtual void Update(float delta)
    {
    }

    virtual void OnAttachedToNode(Node* node);
    virtual void OnDetachedFromNode(Node* node);

    virtual void OnAddedToWorld(World* world);
    virtual void OnRemovedFromWorld(World* world);

    virtual void OnAddedToScene(Scene* scene);
    virtual void OnRemovedFromScene(Scene* scene);

    void AttachChild(const Handle<Entity>& child);
    void DetachChild(const Handle<Entity>& child);

private:
    World* m_world;
    Scene* m_scene;
};

} // namespace hyperion

#endif
