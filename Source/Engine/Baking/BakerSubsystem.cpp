/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/BakerSubsystem.hpp>
#include <Baking/Baker.hpp>
#include <Baking/BakerMemory.hpp>

#include <Baking/LightmapVolume/LightmapVolumeBaker.hpp>
#include <Baking/EnvProbe/EnvProbeBaker.hpp>
#include <Baking/FogVolume/FogVolumeBaker.hpp>
#include <Baking/ShadowMap/ShadowMapBaker.hpp>

#include <Rendering/RenderConfig.hpp>

#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/LightmapVolume.hpp>
#include <Scene/Light.hpp>
#include <Scene/World.hpp>
#include <Scene/View.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>

#include <Core/Threading/TaskSystem.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <System/AppContext.hpp>

#include <Framework/EngineDriver.hpp>

#include <BakerSubsystem.generated.inl>

namespace Hyperion {

using namespace Baking;

#pragma region BakerSubsystem

BakerSubsystem::BakerSubsystem()
{
}

void BakerSubsystem::OnAddedToWorld()
{
    AssertOnThread(g_simThread);
}

void BakerSubsystem::OnRemovedFromWorld()
{
    AssertOnThread(g_simThread);

    for (auto& it : m_bakers)
    {
        Handle<BakerBase>& baker = it.second;
        baker->Shutdown();
    }

    m_bakers.Clear();
}

void BakerSubsystem::Update(float delta)
{
    HYP_SCOPE;
    
    AssertOnThread(g_simThread);

    size_t bakerIndex = 0;

    if (m_bakers.Any())
    {
        const ObjectBase* key = m_bakers[bakerIndex].first;
        BakerBase* baker = m_bakers[bakerIndex].second;

        baker->Update(delta);

        if (baker->IsComplete())
        {
            baker->Shutdown();

            // Remove this guy
            m_bakers.PopFront();
        }
        else
        {
            GetWorld()->ProcessViewAsync(baker->GetView());

            ++bakerIndex;
        }
    }

    // keep the others' views alive even if we don't Update() them right now.
    while (bakerIndex < m_bakers.Size())
    {
        const ObjectBase* key = m_bakers[bakerIndex].first;
        BakerBase* baker = m_bakers[bakerIndex].second;

        GetWorld()->ProcessViewAsync(baker->GetView());

        ++bakerIndex;
    }

    g_bakerArena->Reset();
}

template <>
Task<void> BakerSubsystem::EnqueueBake(const Handle<LightmapVolume>& source, uint32 shadingTypesMaskOverride)
{
    return EnqueueBake_Internal(source, shadingTypesMaskOverride);
}

template <>
Task<void> BakerSubsystem::EnqueueBake(const Handle<EnvProbe>& source, uint32 shadingTypesMaskOverride)
{
    return EnqueueBake_Internal(source, shadingTypesMaskOverride);
}

template <>
Task<void> BakerSubsystem::EnqueueBake(const Handle<FogVolume>& source, uint32 shadingTypesMaskOverride)
{
    return EnqueueBake_Internal(source, shadingTypesMaskOverride);
}

template <>
Task<void> BakerSubsystem::EnqueueBake(const Handle<Light>& source, uint32 shadingTypesMaskOverride)
{
    return EnqueueBake_Internal(source, shadingTypesMaskOverride);
}

template <class T, class... Args>
Task<void> BakerSubsystem::EnqueueBake_Internal(const Handle<T>& source, uint32 shadingTypesMaskOverride, Args&&... args)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!source)
    {
        return Task<void>();
    }

    auto it = m_bakers.FindIf([source](const Pair<ObjectBase*, Handle<BakerBase>>& item)
    {
        return item.first == source.Get();
    });

    if (it != m_bakers.End())
    {
        return Task<void>();
    }

    Handle<BakerBase> baker = MakeHandle<Baker<T>>(BakerConfig::FromConfig(), source, std::forward<Args>(args)...);
    baker->SetShadingTypesMaskOverride(shadingTypesMaskOverride);
    InitObject(baker);

    Task<void> task;

    baker->OnComplete
        .Bind([promise = task.Promise()]()
            {
                promise->Fulfill();
            })
        .Detach();

    baker->Initialize();
    
    GetWorld()->ProcessViewAsync(baker->GetView());

    m_bakers.EmplaceBack(source.Get(), std::move(baker));

    return task;
}

#pragma endregion BakerSubsystem

} // namespace Hyperion
