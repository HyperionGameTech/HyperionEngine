/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/MeshLodSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>
#include <Scene/World.hpp>
#include <Scene/LOD.hpp>

#include <Scene/Components/TerrainPatchComponent.hpp>

#include <Rendering/Mesh.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineGlobals.hpp>

#include <MeshLodSystem.generated.inl>

namespace Hyperion {

CVar<int32> g_cvMeshLodForceLod { "Rendering.MeshLod.ForceLod", -1 };
static CVar<float> s_cvScreenSizeScale { "Rendering.MeshLod.ScreenSizeScale", 1.0f };
static CVar<float> s_cvHysteresis { "Rendering.MeshLod.Hysteresis", 0.1f };

static uint8 SelectMeshLod(const MeshDesc& meshDesc, uint8 numLods, uint8 currentLod, float screenSize)
{
    const float hysteresis = MathUtil::Clamp(s_cvHysteresis.Get(), 0.0f, 0.5f);

    uint8 targetLod = 0;
    float previousScreenSize = MathUtil::MaxSafeValue<float>();

    for (uint8 lodIndex = 1; lodIndex < numLods; lodIndex++)
    {
        const float authoredScreenSize = meshDesc.lods[lodIndex].screenSize;
        if (authoredScreenSize <= 0.0f)
        {
            continue;
        }

        const float threshold = MathUtil::Min(authoredScreenSize, previousScreenSize);

        previousScreenSize = threshold;

        const float scaledThreshold = threshold * (lodIndex <= currentLod ? 1.0f + hysteresis : 1.0f - hysteresis);

        if (screenSize < scaledThreshold)
        {
            targetLod = lodIndex;
        }
    }

    return targetLod;
}

void MeshLodSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (EngineGlobals::IsHeadless() || !GetWorld())
    {
        return;
    }

    Array<LODViewData, SceneTempAllocator> viewDatas;
    GetWorld()->CollectLODViewDatas(viewDatas);

    const int32 forcedLodCVar = g_cvMeshLodForceLod.Get();
    const float screenSizeScale = MathUtil::Max(s_cvScreenSizeScale.Get(), MathUtil::epsilonF);

    for (Scene* scene : scenes)
    {
        for (auto [entity, meshComponent, boundingBoxComponent] :
            scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!meshComponent.mesh)
            {
                continue;
            }

            const uint32 lodDataVersion = meshComponent.mesh->GetLodDataVersion();
            const uint8 numLods = meshComponent.mesh->GetMeshDesc().GetNumLods();

            bool needsProxyUpdate = false;

            if (meshComponent.lodDataVersion != lodDataVersion)
            {
                meshComponent.lodDataVersion = lodDataVersion;

                const uint8 clampedLod = MathUtil::Min(meshComponent.lodIndex, uint8(MathUtil::Max(numLods, 1) - 1));

                if (meshComponent.lodIndex != clampedLod)
                {
                    meshComponent.lodIndex = clampedLod;

                    needsProxyUpdate = true;
                }
            }

            // instanced entities share one LOD for the whole batch, whose bounds say nothing about any one instance
            const bool canSelectLod = numLods > 1
                && meshComponent.numInstances == 0
                && !entity->HasTag<EntityTag::MeshLodPinned>()
                && !entity->TryGetComponent<TerrainPatchComponent>();

            if (!canSelectLod)
            {
                if (numLods <= 1 && meshComponent.lodIndex != 0)
                {
                    meshComponent.lodIndex = 0;

                    needsProxyUpdate = true;
                }

                if (needsProxyUpdate)
                {
                    entity->SetNeedsRenderProxyUpdate();
                }

                continue;
            }

            const BoundingSphere boundingSphere { boundingBoxComponent.worldAabb };

            float screenSize = 0.0f;

            for (const LODViewData& viewData : viewDatas)
            {
                screenSize = MathUtil::Max(screenSize, viewData.ComputeScreenSize(boundingSphere));
            }

            meshComponent.screenSize = screenSize;

            int32 targetLod = SelectMeshLod(meshComponent.mesh->GetMeshDesc(), numLods, meshComponent.lodIndex, screenSize * screenSizeScale);

            targetLod += meshComponent.lodBias;

            if (meshComponent.forcedLod != 0)
            {
                targetLod = int32(meshComponent.forcedLod) - 1;
            }

            if (forcedLodCVar >= 0)
            {
                targetLod = forcedLodCVar;
            }

            const uint8 clampedTargetLod = uint8(MathUtil::Clamp(targetLod, 0, int32(numLods) - 1));

            if (meshComponent.lodIndex != clampedTargetLod)
            {
                meshComponent.lodIndex = clampedTargetLod;

                needsProxyUpdate = true;
            }

            if (needsProxyUpdate)
            {
                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

} // namespace Hyperion
