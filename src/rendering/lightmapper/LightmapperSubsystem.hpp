/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAPPER_SUBSYSTEM_HPP
#define HYPERION_LIGHTMAPPER_SUBSYSTEM_HPP

#include <scene/Subsystem.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Task.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <GameCounter.hpp>

namespace hyperion {

class Lightmapper;
class Scene;

HYP_CLASS()
class HYP_API LightmapperSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(LightmapperSubsystem);

public:
    LightmapperSubsystem();
    virtual ~LightmapperSubsystem() override = default;

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;
    virtual void Update(float delta) override;

    Task<void>* GenerateLightmaps(const Handle<Scene>& scene, const BoundingBox& aabb);

private:
    HashMap<Id<Scene>, UniquePtr<Lightmapper>> m_lightmappers;
    LinkedList<Task<void>> m_tasks;
};

} // namespace hyperion

#endif