/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_HPP
#define HYPERION_ENTITY_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

namespace hyperion {

class World;
class Scene;

HYP_CLASS()
class Entity : public HypObject<Entity>
{
    HYP_OBJECT_BODY(Entity);

public:
    friend class EntityManager;

    HYP_API Entity();
    HYP_API virtual ~Entity() override;

protected:
    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    virtual void OnAddedToWorld(World* world)
    {
        m_world = world;
    }

    virtual void OnRemovedFromWorld(World* world)
    {
        m_world = nullptr;
    }

    virtual void OnAddedToScene(Scene* scene)
    {
        m_scene = scene;
    }

    virtual void OnRemovedFromScene(Scene* scene)
    {
        m_scene = nullptr;
    }

private:
    void Init() override;

    World* m_world;
    Scene* m_scene;
};

} // namespace hyperion

#endif
