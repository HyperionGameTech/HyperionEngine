/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Subsystem.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Task.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/reflection/Handle.hpp>
#include <core/Defines.hpp>

namespace hyperion {

class LightmapperBase;
class LightmapVolume;
class EnvProbe;
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

    /*! \brief Queue up a task to generate lightmaps for the given LightmapVolume.
     *   The returned Task can be used to track the completion state of the lightmap generation job.
     *   If a lightmap generation task is already in progress for the given volume, the existing task will be returned instead. */
    Task<void>* GenerateLightmaps(const Handle<LightmapVolume>& volume);

    /*! \brief Enqueues a task to generate a lightmap for the given EnvProbe.
     *   The returned Task can be used to track the completion state of the lightmap generation job
     *   If a lightmap generation task is already in progress for the given volume, the existing task will be returned instead. */
    Task<void>* GenerateLightmaps(const Handle<EnvProbe>& envProbe);

private:
    template <class T, class... Args>
    Task<void>* GenerateLightmaps_Internal(const Handle<T>& source, Args&&... args);

    // Map source to lightmapper instance
    HashMap<HypObjectBase*, Handle<LightmapperBase>> m_lightmappers;
    HashMap<HypObjectBase*, Task<void>*> m_activeTasks;
    LinkedList<Task<void>> m_tasks;
};

} // namespace hyperion
