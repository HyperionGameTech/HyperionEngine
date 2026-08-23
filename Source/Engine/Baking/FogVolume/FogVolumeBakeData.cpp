/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/FogVolume/FogVolumeBakeData.hpp>

#include <Scene/FogVolume.hpp>
#include <Scene/Light.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Util/VoxelOctree.hpp>

#include <Util/NoiseFactory.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace Baking {

static constexpr float OccSdfTargetVoxelSize = 0.25f;
static constexpr uint32 OccSdfTargetExtent = 128;

static void GenerateNoiseBitmap(typename BakeData<FogVolume>::NoiseBitmap& noiseBitmap)
{
    class FogVolumeNoiseCombinator : public NoiseCombinator
    {
    public:
        FogVolumeNoiseCombinator()
        {
            Use<SimplexNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, 0.5f, 0.0f, Vec3f(20.0f));
            Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::ADDITIVE, 0.3f, 0.0f, Vec3f(70.0f));
            Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, 0.4f, 0.0f, Vec3f(80.0f));
            Use<WorleyNoiseGenerator>(3, NoiseCombinator::Mode::SUBTRACTIVE, 0.1f, 0.0f, Vec3f(80.0f));
        }
    };

    FogVolumeNoiseCombinator noiseCombinator;

    for (uint32 z = 0; z < noiseBitmap.GetDepth(); z++)
    {
        for (uint32 y = 0; y < noiseBitmap.GetHeight(); y++)
        {
            for (uint32 x = 0; x < noiseBitmap.GetWidth(); x++)
            {
                const float noiseValue = noiseCombinator.GetNoise(
                    Vec3f(
                        float(x) / float(noiseBitmap.GetWidth()),
                        float(y) / float(noiseBitmap.GetHeight()),
                        float(z) / float(noiseBitmap.GetDepth())));

                noiseBitmap.GetPixelReference(x, y, z).SetComponentFloat(0, noiseValue);
            }
        }
    }
}

Result BakeData<FogVolume>::Build()
{
    Assert(m_fogVolume != nullptr);

    const BoundingBox localBounds = m_fogVolume->GetLocalBounds();
    const Vec3f localBoundsExtent = localBounds.GetExtent();
    const float maxExtent = localBoundsExtent.Max();

    if (maxExtent < MathUtil::epsilonF)
    {
        dimensions = Vec3u::One();
    }
    else
    {
        const float scale = float(FogVolume::MaxVolumeTextureExtent) / maxExtent;
        dimensions = Vec3u(MathUtil::Max(Vec3f(1.0f), MathUtil::Ceil(localBoundsExtent * scale)));
    }

    if (dimensions.Volume() == 0)
    {
        dimensions = Vec3u::One();
    }

    {
        m_lights.Clear();
        m_envProbes.Clear();

        bool foundSun = false;

        World* world = m_fogVolume->GetWorld();
        Assert(world != nullptr);

        for (Scene* scene : world->GetScenes())
        {
            if (!(scene->GetSceneFlags() & SceneFlags::FOREGROUND))
            {
                continue;
            }

            EntityManager* mgr = scene->GetEntityManager();

            if (!mgr)
            {
                continue;
            }

            for (auto [light] : mgr->GetEntitySet<EntityType<Light>>())
            {
                if (light->GetLightType() == LightType::Directional)
                {
                    if (!foundSun)
                    {
                        m_sunDirection = StaticCast<DirectionalLight>(light)->GetDirection();
                        foundSun = true;
                    }

                    continue;
                }

                m_lights.PushBack(MakeStrongRef(light));
            }

            for (auto [envProbe] : mgr->GetEntitySet<EntityType<EnvProbe>>())
            {
                if (!envProbe->IsAmbientProbe())
                {
                    continue;
                }

                m_envProbes.PushBack(MakeStrongRef(envProbe));
            }
        }

        if (!foundSun)
        {
            HYP_LOG(Editor, Warning, "No directional lights found in scene for fog volume");
        }
    }

    m_volumeBitmap = VolumeBitmap(dimensions.x, dimensions.y, dimensions.z);

    const Vec3f extentWS = m_fogVolume->GetWorldBounds().GetExtent();
    const Vec3f texelSizeWS = extentWS / Vec3f(dimensions);

    texels.Resize(dimensions.Volume());

    BoundingBox voxelOctreeAabb = m_fogVolume->GetWorldBounds();

    if (!voxelOctreeAabb.IsValid() || !voxelOctreeAabb.IsFinite() || voxelOctreeAabb.IsZero())
    {
        return HYP_MAKE_ERROR(Error, "Invalid fog volume AABB for voxel octree build");
    }

    VoxelOctreeParams octreeParams;
    octreeParams.aabb = voxelOctreeAabb;
    octreeParams.allowResize = false;
    octreeParams.maxDepth = 10;

    m_voxelOctree = MakeUniqueWithAllocator<VoxelOctree, BakerAllocator>();

    auto buildResult = m_voxelOctree->Build(octreeParams, *m_fogVolume->GetEntityManager());

    if (buildResult.HasError())
    {
        return buildResult.GetError();
    }

    Vec3u occDimensions;

    if (maxExtent < MathUtil::epsilonF)
    {
        occDimensions = Vec3u::One();
    }
    else
    {
        static constexpr float OccScale = 1.0f / OccSdfTargetVoxelSize;

        occDimensions = Vec3u(MathUtil::Min(
            Vec3f(float(OccSdfTargetExtent)),
            MathUtil::Max(Vec3f(1.0f), MathUtil::Ceil(extentWS * OccScale))));
    }

    m_occSdfBitmap = OccSdfBitmap(
        occDimensions.x, occDimensions.y, occDimensions.z);

    const Vec3f occTexelHalfSizeWS = extentWS * (Vec3f(0.5f) / Vec3f(occDimensions));
    const float occSdfMaxDistance = extentWS.Length() * 4.0f;

    for (uint32 z = 0; z < occDimensions.z; z++)
    {
        for (uint32 y = 0; y < occDimensions.y; y++)
        {
            for (uint32 x = 0; x < occDimensions.x; x++)
            {
                const Vec3f posWS = voxelOctreeAabb.GetMin()
                    + (extentWS * (Vec3f(float(x), float(y), float(z)) / Vec3f(occDimensions)))
                    + occTexelHalfSizeWS;

                const double sdf = m_voxelOctree->GetSignedDistanceAtPoint(posWS);

                const float clampedSdf = MathUtil::Min(float(sdf), occSdfMaxDistance);

                m_occSdfBitmap.GetPixelReference(x, y, z).SetComponentFloat(0, clampedSdf);
            }
        }
    }

    m_noiseBitmap = NoiseBitmap(
        FogVolume::MaxNoiseTextureExtent,
        FogVolume::MaxNoiseTextureExtent,
        FogVolume::MaxNoiseTextureExtent);

    GenerateNoiseBitmap(m_noiseBitmap);

    return {};
}

} // namespace Baking

} // namespace Hyperion
