/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Subsystem.hpp>

#include <Core/containers/LinkedList.hpp>
#include <Core/containers/Map.hpp>

#include <Core/threading/Task.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/math/BoundingBox.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {

class LightmapVolume;
class ReflectionProbe;
class FogVolume;
class Light;
class Scene;

namespace Baking {

class BakerBase;

template <class T>
    concept Bakeable = std::is_same_v<T, LightmapVolume>
        || std::is_same_v<T, ReflectionProbe>
        || std::is_same_v<T, FogVolume>
        || std::is_same_v<T, Light>;
} // namespace Baking

HYP_CLASS()
class ENGINE_API BakerSubsystem final : public Subsystem
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
    Task<void> EnqueueBake(const Handle<T>& source);

private:
    SubsystemUpdatePhase GetUpdatePhase_Internal() const override
    {
        return SubsystemUpdatePhase::AfterVis;
    }

    template <class T, class... Args>
    Task<void> EnqueueBake_Internal(const Handle<T>& source, Args&&... args);

    // Map source to lightmapper instance
    TMap<ObjectBase*, Handle<Baking::BakerBase>> m_bakers;
};

} // namespace Hyperion
