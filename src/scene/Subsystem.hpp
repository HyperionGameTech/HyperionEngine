/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/HashCode.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

namespace hyperion {

class Scene;
class World;

HYP_CLASS(Abstract)
class HYP_API Subsystem : public ObjectBase
{
    HYP_OBJECT_BODY(Subsystem);

public:
    friend class World;

    Subsystem();
    Subsystem(const Subsystem& other) = delete;
    Subsystem& operator=(const Subsystem& other) = delete;
    Subsystem(Subsystem&& other) = delete;
    Subsystem& operator=(Subsystem&& other) = delete;
    virtual ~Subsystem();

    virtual bool RequiresUpdateOnGameThread() const
    {
        return true;
    }

    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    virtual void OnAddedToWorld() = 0;
    virtual void OnRemovedFromWorld() = 0;
    virtual void PreUpdate(float delta)
    {
    }
    virtual void Update(float delta) = 0;
    virtual void OnSceneAttached(const Handle<Scene>& scene) { };
    virtual void OnSceneDetached(Scene* scene) { };

protected:
    virtual void Init() override
    {
        SetReady(true);
    }

private:
    HYP_FORCE_INLINE void SetWorld(World* world)
    {
        m_world = world;
    }

    World* m_world;
};

} // namespace hyperion
