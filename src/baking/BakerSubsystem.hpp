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

namespace Hyperion {

class LightmapVolume;
class ReflectionProbe;
class FogVolume;
class Scene;

namespace Baking {

class BakerBase;

template <class T>
concept Bakeable = std::is_same_v<T, LightmapVolume> || std::is_same_v<T, ReflectionProbe> || std::is_same_v<T, FogVolume>;

} // namespace Baking

HYP_CLASS()
class HYP_API BakerSubsystem final : public Subsystem
{
    HYP_OBJECT_BODY(BakerSubsystem);

public:
    BakerSubsystem();
    virtual ~BakerSubsystem() override = default;

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;
    virtual void Update(float delta) override;

    /*! \brief Queue up a task to start baking lightmaps or other baked data for the given object.
     *   The returned Task can be used to track the completion state of the lightmap generation job.
     *   If a lightmap generation task is already in progress for the given volume, the existing task will be returned instead. */
    template <Baking::Bakeable T>
    Task<void>* EnqueueBake(const Handle<T>& source);

private:
    SubsystemUpdatePhase GetUpdatePhase_Internal() const override
    {
        return SubsystemUpdatePhase::AfterVis;
    }

    template <class T, class... Args>
    Task<void>* EnqueueBake_Internal(const Handle<T>& source, Args&&... args);

    // Map source to lightmapper instance
    HashMap<ObjectBase*, Handle<Baking::BakerBase>> m_bakers;
    HashMap<ObjectBase*, Task<void>*> m_activeTasks;
    LinkedList<Task<void>> m_tasks;
};

} // namespace Hyperion
