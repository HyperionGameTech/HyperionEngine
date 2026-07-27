/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Subsystem.hpp>

#include <Core/Containers/List.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Threading/Task.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {

class LightmapVolume;
class EnvProbe;
class FogVolume;
class Light;
class Scene;

namespace Baking {

class BakerBase;

template <class T>
    concept Bakeable = std::is_same_v<T, LightmapVolume>
        || std::is_same_v<T, EnvProbe>
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
     *   If a lightmap generation task is already in progress for the given volume, the existing task will be returned instead.
     *   \param shadingTypesMaskOverride If nonzero, restricts the bake to this subset of shading types instead of the
     *   baker's default mask (e.g. baking bent normals only, without recomputing irradiance/radiance). */
    template <Baking::Bakeable T>
    Task<void> EnqueueBake(const Handle<T>& source, uint32 shadingTypesMaskOverride = 0);

private:
    SubsystemUpdatePhase GetUpdatePhase_Internal() const override
    {
        return SubsystemUpdatePhase::AfterVis;
    }

    template <class T, class... Args>
    Task<void> EnqueueBake_Internal(const Handle<T>& source, uint32 shadingTypesMaskOverride, Args&&... args);

    // Map source to lightmapper instance
    Map<ObjectBase*, Handle<Baking::BakerBase>> m_bakers;
};

} // namespace Hyperion
