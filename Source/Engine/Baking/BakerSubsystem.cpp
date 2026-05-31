/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/BakerSubsystem.hpp>
#include <Baking/Baker.hpp>

#include <Baking/lightmap_volume/LightmapVolumeBaker.hpp>
#include <Baking/reflection_probe/ReflectionProbeBaker.hpp>
#include <Baking/fog_volume/FogVolumeBaker.hpp>
#include <Baking/shadow_map/ShadowMapBaker.hpp>

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

#include <Core/threading/TaskSystem.hpp>

#include <Core/math/BoundingBox.hpp>

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
        GetWorld()->RemoveView(it.second->GetView());
    }

    m_bakers.Clear();
}

void BakerSubsystem::Update(float delta)
{
    AssertOnThread(g_simThread);

    Array<ObjectBase*> keysToRemove;

    for (auto& it : m_bakers)
    {
        const ObjectBase* key = it.first;
        BakerBase* baker = it.second;

        baker->Update(delta);

        if (baker->IsComplete())
        {
            GetWorld()->RemoveView(baker->GetView());

            keysToRemove.PushBack(it.first);
        }
    }

    for (ObjectBase* obj : keysToRemove)
    {
        m_bakers.Erase(obj);
    }
}

template <>
Task<void> BakerSubsystem::EnqueueBake(const Handle<LightmapVolume>& source)
{
    return EnqueueBake_Internal(source);
}

template <>
Task<void> BakerSubsystem::EnqueueBake(const Handle<ReflectionProbe>& source)
{
    return EnqueueBake_Internal(source);
}

template <>
Task<void> BakerSubsystem::EnqueueBake(const Handle<FogVolume>& source)
{
    return EnqueueBake_Internal(source);
}

template <>
Task<void> BakerSubsystem::EnqueueBake(const Handle<Light>& source)
{
    return EnqueueBake_Internal(source);
}

template <class T, class... Args>
Task<void> BakerSubsystem::EnqueueBake_Internal(const Handle<T>& source, Args&&... args)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!source)
    {
        return Task<void>();
    }

    auto it = m_bakers.Find(source.Get());

    if (it != m_bakers.End())
    {
        return Task<void>();
    }

    Handle<BakerBase> baker = MakeHandle<Baker<T>>(BakerConfig::FromConfig(), source, std::forward<Args>(args)...);
    InitObject(baker);

    Task<void> task;

    baker->OnComplete
        .Bind([promise = task.Promise()]()
            {
                promise->Fulfill();
            })
        .Detach();

    baker->Initialize();

    GetWorld()->AddView(baker->GetView());

    m_bakers.Insert(source.Get(), std::move(baker));

    return task;
}

#pragma endregion BakerSubsystem

} // namespace Hyperion
