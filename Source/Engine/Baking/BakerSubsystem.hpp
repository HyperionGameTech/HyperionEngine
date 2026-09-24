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

#include <Core/Functional/Delegate.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Defines.hpp>

#include <Baking/BakerMemory.hpp>

namespace Hyperion {

class LightmapVolume;
class EnvProbe;
class FogVolume;
class Light;
class Scene;

namespace Baking {

class BakerBase;
struct BakeLayer;

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

    /// Queue up a task to start baking lightmaps or other baked data for the given object
    template <Baking::Bakeable T>
    Task<void> EnqueueBake(Baking::BakeLayer& bakeLayer, const Handle<T>& source, uint32 shadingTypesMaskOverride = 0);

    /// Cancel an in-progress bake for the given source, if one exists - sim thread only
    void CancelBake(ObjectBase* source);

    /// Get the progress in the range [0, 1] of a bake task for the source */
    float GetBakeProgress(ObjectBase* source) const;

private:
    struct ObjectBakeState
    {
        Handle<ObjectBase> obj;
        Baking::BakeLayer* bakeLayer;
        Handle<Baking::BakerBase> baker;

        // bakers gather their scene state when they start, after earlier bakes in the queue have written theirs
        bool started = false;
    };

    SubsystemUpdatePhase GetUpdatePhase_Internal() const override
    {
        return SubsystemUpdatePhase::AfterVis;
    }

    void OnBakeCompleted(Baking::BakeLayer& bakeLayer, ObjectBase* source);

    template <class T, class... Args>
    Task<void> EnqueueBake_Internal(
        Baking::BakeLayer& bakeLayer,
        const Handle<T>& source,
        uint32 shadingTypesMaskOverride,
        Args&&... args);

    Array<ObjectBakeState, Baking::BakerAllocator> m_bakes;
};

} // namespace Hyperion
