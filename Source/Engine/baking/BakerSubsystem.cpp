/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/BakerSubsystem.hpp>
#include <baking/Baker.hpp>

#include <baking/lightmap_volume/LightmapVolumeBaker.hpp>
#include <baking/reflection_probe/ReflectionProbeBaker.hpp>
#include <baking/fog_volume/FogVolumeBaker.hpp>

#include <rendering/RenderConfig.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/FogVolume.hpp>
#include <scene/LightmapVolume.hpp>
#include <scene/World.hpp>
#include <scene/View.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <Core/threading/TaskSystem.hpp>

#include <Core/math/BoundingBox.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>

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

    Handle<BakerBase> baker = MakeHandle<Baker<T>>(LightmapperConfig::FromConfig(), source, std::forward<Args>(args)...);
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
