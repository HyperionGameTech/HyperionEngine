/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/BakerSubsystem.hpp>
#include <Baking/Baker.hpp>
#include <Baking/BakerMemory.hpp>
#include <Baking/BakerScene.hpp>
#include <Baking/BakeEpoch.hpp>

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
using Cat = BakerSceneCategory;

static void UpdateEpoch(BakerScene& bakerScene, const LightmapVolume& lmv)
{
    uint64 epoch = BakeEpoch::ComputeEpoch(lmv, bakerScene);
    
    bakerScene.SetAssetEpoch<Cat::LightReceiver>(lmv, epoch);
    bakerScene.SetAssetEpoch<Cat::Lightmap>(lmv, epoch);
}

static void UpdateEpoch(BakerScene& bakerScene, const EnvProbe& envProbe)
{
    uint64 epoch = BakeEpoch::ComputeEpoch(envProbe, bakerScene);

    bakerScene.SetAssetEpoch<Cat::LightReceiver>(envProbe, epoch);
}

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

    for (ObjectBakeState& bs : m_bakes)
    {
        Handle<BakerBase>& baker = bs.baker;
        Assert(baker.IsValid());

        baker->Shutdown();
    }

    m_bakes.Clear();
}

void BakerSubsystem::Update(float delta)
{
    HYP_SCOPE;
    
    AssertOnThread(g_simThread);

    size_t bakerIndex = 0;

    if (m_bakes.Any())
    {
        ObjectBakeState& bs = m_bakes[bakerIndex];
        Assert(bs.obj && bs.bakerScene && bs.baker);

        ObjectBase* source = bs.obj;
        BakerScene& bakerScene = *bs.bakerScene;
        BakerBase* baker = bs.baker;

        baker->Update(delta);

        if (baker->IsComplete())
        {
            const bool succeeded = baker->GetState() == BakerState::Complete;

            HYP_LOG(Lightmap, Info, "Baker for source {} completed, succeeded = {}",
                source ? source->Id() : ObjIdBase(), succeeded);

            baker->Shutdown();

            if (succeeded)
            {
                OnBakeCompleted(bakerScene, source);
            }

            // Remove this guy
            m_bakes.PopFront();
        }
        else
        {
            GetWorld()->ProcessViewAsync(baker->GetView());

            ++bakerIndex;
        }
    }

    // keep the others' views alive even if we don't Update() them right now.
    while (bakerIndex < m_bakes.Size())
    {
        ObjectBakeState& bs = m_bakes[bakerIndex];
        Assert(bs.baker);

        GetWorld()->ProcessViewAsync(bs.baker->GetView());

        ++bakerIndex;
    }

    g_bakerArena->Reset();
}

void BakerSubsystem::OnBakeCompleted(Baking::BakerScene& bakerScene, ObjectBase* source)
{
    if (!source)
    {
        return;
    }
    
    if (LightmapVolume* lmv = DynamicCast<LightmapVolume>(source))
    {
        UpdateEpoch(bakerScene, *lmv);

        return;
    }

    if (EnvProbe* envProbe = DynamicCast<EnvProbe>(source))
    {
        UpdateEpoch(bakerScene, *envProbe);

        return;
    }
}

// clang-format off

#define DEF_ENQUEUE_BAKE_SPECIALIZATION(T) \
    template <> Task<void> BakerSubsystem::EnqueueBake(Baking::BakerScene& bakerScene, const Handle<T>& source, uint32 shadingTypesMaskOverride) \
    {   \
        return EnqueueBake_Internal(bakerScene, source, shadingTypesMaskOverride);  \
    }

DEF_ENQUEUE_BAKE_SPECIALIZATION(LightmapVolume);
DEF_ENQUEUE_BAKE_SPECIALIZATION(EnvProbe);
DEF_ENQUEUE_BAKE_SPECIALIZATION(FogVolume);
DEF_ENQUEUE_BAKE_SPECIALIZATION(Light);

#undef DEF_ENQUEUE_BAKE_SPECIALIZATION

// clang-format on

template <class T, class... Args>
Task<void> BakerSubsystem::EnqueueBake_Internal(
    Baking::BakerScene& bakerScene,
    const Handle<T>& source,
    uint32 shadingTypesMaskOverride,
    Args&&... args)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!source)
    {
        return Task<void>();
    }

    HYP_LOG(Lightmap, Info, "EnqueueBake_Internal: bakerSubsystem = {}, world = {}, source = {}",
        (void*)this, (void*)GetWorld(), source->Id());
    
    auto it = m_bakes.FindIf([source](const ObjectBakeState& bs)
    {
        return bs.obj == source.Get();
    });

    if (it != m_bakes.End())
    {
        return Task<void>();
    }

    Handle<BakerBase> baker = MakeHandle<Baker<T>>(
        BakerConfig::FromConfig(), // <---- TODO: Not just the global...... should be per-type! Like BakerConfig<Light> ?
        bakerScene,
        source,
        std::forward<Args>(args)...);

    baker->SetShadingTypesMaskOverride(shadingTypesMaskOverride);
    InitObject(baker);

    Task<void> task;

    auto fulfillPromise = [promise = task.Promise()]()
    {
        promise->Fulfill();
    };

    baker->OnComplete.Bind(fulfillPromise).Detach();
    baker->OnCancelled.Bind(fulfillPromise).Detach();

    baker->Initialize();
    
    GetWorld()->ProcessViewAsync(baker->GetView());

    m_bakes.PushBack(ObjectBakeState { source, &bakerScene, std::move(baker) });

    return task;
}

void BakerSubsystem::CancelBake(ObjectBase* source)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!source)
    {
        return;
    }

    auto it = m_bakes.FindIf([source](const ObjectBakeState& bs)
    {
        return bs.obj == source;
    });

    if (it == m_bakes.End())
    {
        return;
    }

    Handle<BakerBase> baker = std::move(it->baker);
    Assert(baker.IsValid());

    m_bakes.Erase(it);

    baker->RequestCancel();
    baker->Shutdown();
}

float BakerSubsystem::GetBakeProgress(ObjectBase* source) const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!source)
    {
        return 1.0f;
    }

    auto it = m_bakes.FindIf([source](const ObjectBakeState& bs)
    {
        return bs.obj == source;
    });

    if (it == m_bakes.End())
    {
        return 1.0f;
    }

    Assert(it->baker.IsValid());

    return it->baker->GetProgress();
}

#pragma endregion BakerSubsystem

} // namespace Hyperion
