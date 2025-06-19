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
class EntityManager;

HYP_CLASS()
class HYP_API Entity : public HypObject<Entity>
{
    HYP_OBJECT_BODY(Entity);

public:
    friend class EntityManager;

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

protected:
    virtual void Init() override;

    virtual void OnAddedToWorld(World* world);
    virtual void OnRemovedFromWorld(World* world);
    
    virtual void OnAddedToScene(Scene* scene);
    virtual void OnRemovedFromScene(Scene* scene);

    void AttachChild(const Handle<Entity>& child);
    void DetachChild(const Handle<Entity>& child);

    bool HasParent(const Entity* parent) const;

private:
    World* m_world;
    Scene* m_scene;
    Entity* m_parent;
    Array<Handle<Entity>> m_children;
};

} // namespace hyperion

#endif
