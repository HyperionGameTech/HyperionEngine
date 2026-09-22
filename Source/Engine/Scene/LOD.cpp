/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/LOD.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/World.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/View.hpp>

#include <Rendering/Mesh.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Framework/GameState.hpp>

namespace Hyperion {

///pins every mesh to this LOD; -1 selects normally. Set it to 0 to see the scene with LODs switched off.
CVar<int32> g_cvMeshLodForceLod { "Rendering.MeshLod.ForceLod", -1 };

///scales the screen size a mesh is judged by, so > 1 keeps meshes at finer LODs further out
static CVar<float> s_cvScreenSizeScale { "Rendering.MeshLod.ScreenSizeScale", 1.0f };

///how far past a threshold a mesh has to travel before switching back, as a fraction of the threshold
static CVar<float> s_cvHysteresis { "Rendering.MeshLod.Hysteresis", 0.1f };

///added to every automatic pick, on top of the per-mesh bias. Positive is coarser, negative is finer.
static CVar<int32> s_cvGlobalBias { "Rendering.MeshLod.GlobalBias", 0 };

///floor for automatic picks - raise it to stop anything rendering at the finest LODs
static CVar<int32> s_cvMinLod { "Rendering.MeshLod.MinLod", 0 };

///ceiling for automatic picks; -1 means the mesh's own coarsest LOD
static CVar<int32> s_cvMaxLod { "Rendering.MeshLod.MaxLod", -1 };

///packs the overridden entity's id above the LOD it is pinned to
static constexpr uint64 g_meshLodOverrideLodMask = (1ull << MeshLodIndexBits) - 1;
static constexpr uint64 g_noMeshLodOverride = ~0ull;
static AtomicVar<uint64> g_meshLodOverride { g_noMeshLodOverride };

LODViewData::LODViewData(const Mat4f& projectionMatrix, const Vec3f& position, float nearClip)
    : position(position),
      projectionScale(projectionMatrix[1][1]),
      nearClip(nearClip),
      isOrthographic(MathUtil::Abs(projectionMatrix[3][3]) > MathUtil::epsilonF)
{
}

LODViewData::LODViewData(const Camera& camera)
    : LODViewData(camera.GetProjectionMatrix(), camera.GetWorldTranslation(), camera.GetNearClip())
{
}

static uint8 SelectAutomaticMeshLod(const MeshDesc& meshDesc, uint8 numLods, uint8 previousLod, float screenSize)
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

        const float scaledThreshold = threshold * (lodIndex <= previousLod ? 1.0f + hysteresis : 1.0f - hysteresis);

        if (screenSize < scaledThreshold)
        {
            targetLod = lodIndex;
        }
    }

    return targetLod;
}

uint8 SelectMeshLod(const MeshDesc& meshDesc, const MeshLodSelectionParams& params, float screenSize, uint8 previousLod, int32 viewLodBias)
{
    const uint8 numLods = MathUtil::Max(params.numLods, uint8(1));
    const int32 coarsestLod = int32(numLods) - 1;

    // Explicit forces are debugging and authoring tools, so they deliberately ignore the bias and clamp
    // CVars below - asking for a LOD and getting a different one back would make them useless.
    const int32 forcedLodCVar = g_cvMeshLodForceLod.Get();

    if (forcedLodCVar >= 0)
    {
        return uint8(MathUtil::Clamp(forcedLodCVar, 0, coarsestLod));
    }

    if (params.forcedLod != 0)
    {
        return uint8(MathUtil::Clamp(int32(params.forcedLod) - 1, 0, coarsestLod));
    }

    const float screenSizeScale = MathUtil::Max(s_cvScreenSizeScale.Get(), MathUtil::epsilonF);

    const int32 targetLod = int32(SelectAutomaticMeshLod(meshDesc, numLods, previousLod, screenSize * screenSizeScale))
        + params.lodBias
        + viewLodBias
        + s_cvGlobalBias.Get();

    const int32 minLod = MathUtil::Clamp(s_cvMinLod.Get(), 0, coarsestLod);

    // A negative ceiling means the mesh's own coarsest LOD. Clamping the ceiling against the floor keeps a
    // stale pair from inverting the window and selecting nothing sensible.
    const int32 configuredMaxLod = s_cvMaxLod.Get();
    const int32 maxLod = configuredMaxLod < 0
        ? coarsestLod
        : MathUtil::Clamp(configuredMaxLod, minLod, coarsestLod);

    return uint8(MathUtil::Clamp(targetLod, minLod, maxLod));
}

void SetMeshLodOverride(ObjId<Entity> entityId, uint8 lodIndex)
{
    if (!entityId.IsValid())
    {
        ClearMeshLodOverride();

        return;
    }

    AssertDebug(lodIndex < MaxMeshLods, "LOD {} is out of range for a mesh LOD override", lodIndex);

    g_meshLodOverride.Set((uint64(entityId.Value()) << MeshLodIndexBits) | (uint64(lodIndex) & g_meshLodOverrideLodMask), MemoryOrder::RELEASE);
}

void ClearMeshLodOverride()
{
    g_meshLodOverride.Set(g_noMeshLodOverride, MemoryOrder::RELEASE);
}

uint8 GetMeshLodOverride(ObjId<Entity> entityId)
{
    if (!entityId.IsValid())
    {
        return uint8(~0);
    }

    const uint64 packed = g_meshLodOverride.Get(MemoryOrder::ACQUIRE);

    if (packed == g_noMeshLodOverride || (packed >> MeshLodIndexBits) != uint64(entityId.Value()))
    {
        return uint8(~0);
    }

    return uint8(packed & g_meshLodOverrideLodMask);
}

} // namespace Hyperion
