/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainGrass.hpp>
#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>
#include <Scene/WorldGrid/Terrain/GroundCover.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Instancing/InstanceGroup.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineDefaults.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <bit>
#include <cmath>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

namespace TerrainGrass {


namespace {

constexpr float SlopeAlignment = 0.1f;

struct GrassRandom
{
    uint32 state;

    HYP_FORCE_INLINE explicit GrassRandom(uint32 seed)
        : state(seed * 747796405u + 2891336453u)
    {
    }

    HYP_FORCE_INLINE float Next()
    {
        state = state * 747796405u + 2891336453u;
        uint32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        word = (word >> 22u) ^ word;

        return float(word) * (1.0f / 4294967296.0f);
    }

    HYP_FORCE_INLINE float Range(float from, float to)
    {
        return from + (to - from) * Next();
    }
};

float GetInstanceRank(const Vec3f& position)
{
    uint32 state = (std::bit_cast<uint32>(position.x) * 0x8DA6B343u) ^ (std::bit_cast<uint32>(position.z) * 0xD8163841u);
    state = state * 747796405u + 2891336453u;
    state = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    state = (state >> 22u) ^ state;

    // 24 bits so the conversion to float is exact on the GPU too
    return float(state >> 8u) * (1.0f / 16777216.0f);
}

float SampleSplatWeight(const TerrainGrassTileInput& input, uint32 splatLayer, float gridX, float gridZ)
{
    const float maxCoord = float(input.cellSize - 1);

    gridX = MathUtil::Clamp(gridX, 0.0f, maxCoord);
    gridZ = MathUtil::Clamp(gridZ, 0.0f, maxCoord);

    const uint32 x0 = MathUtil::Min(uint32(gridX), input.cellSize - 1);
    const uint32 z0 = MathUtil::Min(uint32(gridZ), input.cellSize - 1);
    const uint32 x1 = MathUtil::Min(x0 + 1, input.cellSize - 1);
    const uint32 z1 = MathUtil::Min(z0 + 1, input.cellSize - 1);

    const float fx = gridX - float(x0);
    const float fz = gridZ - float(z0);

    const auto sampleWeightAt = [&input, splatLayer](uint32 x, uint32 z)
    {
        return float(input.splatWeights[(size_t(z) * input.cellSize + x) * TerrainNumSplatLayers + splatLayer]) * (1.0f / 255.0f);
    };

    return MathUtil::Lerp(
        MathUtil::Lerp(sampleWeightAt(x0, z0), sampleWeightAt(x1, z0), fx),
        MathUtil::Lerp(sampleWeightAt(x0, z1), sampleWeightAt(x1, z1), fx),
        fz);
}

float SamplePaintWeight(const TerrainGrassTileInput& input, int32 plane, float gridX, float gridZ)
{
    const size_t planeSize = size_t(input.cellSize) * input.cellSize;

    if (plane < 0 || input.paintWeights.Size() < planeSize * size_t(plane + 1))
    {
        return 0.0f;
    }

    const float maxCoord = float(input.cellSize - 1);

    gridX = MathUtil::Clamp(gridX, 0.0f, maxCoord);
    gridZ = MathUtil::Clamp(gridZ, 0.0f, maxCoord);

    const uint32 x0 = MathUtil::Min(uint32(gridX), input.cellSize - 1);
    const uint32 z0 = MathUtil::Min(uint32(gridZ), input.cellSize - 1);
    const uint32 x1 = MathUtil::Min(x0 + 1, input.cellSize - 1);
    const uint32 z1 = MathUtil::Min(z0 + 1, input.cellSize - 1);

    const float fx = gridX - float(x0);
    const float fz = gridZ - float(z0);

    const ubyte* planeWeights = input.paintWeights.Data() + planeSize * size_t(plane);

    const auto sampleWeightAt = [&input, planeWeights](uint32 x, uint32 z)
    {
        return float(planeWeights[size_t(z) * input.cellSize + x]) * (1.0f / 255.0f);
    };

    return MathUtil::Lerp(
        MathUtil::Lerp(sampleWeightAt(x0, z0), sampleWeightAt(x1, z0), fx),
        MathUtil::Lerp(sampleWeightAt(x0, z1), sampleWeightAt(x1, z1), fx),
        fz);
}

float HashToUnit(int32 x, int32 z, uint32 seed)
{
    uint32 state = (uint32(x) * 0x8DA6B343u) ^ (uint32(z) * 0xD8163841u) ^ (seed * 0xCB1AB31Fu);
    state = state * 747796405u + 2891336453u;
    state = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    state = (state >> 22u) ^ state;

    return float(state) * (1.0f / 4294967296.0f);
}

float ValueNoise(float x, float z, uint32 seed)
{
    const int32 x0 = int32(MathUtil::Floor(x));
    const int32 z0 = int32(MathUtil::Floor(z));

    const float fx = x - float(x0);
    const float fz = z - float(z0);

    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sz = fz * fz * (3.0f - 2.0f * fz);

    return MathUtil::Lerp(
        MathUtil::Lerp(HashToUnit(x0, z0, seed), HashToUnit(x0 + 1, z0, seed), sx),
        MathUtil::Lerp(HashToUnit(x0, z0 + 1, seed), HashToUnit(x0 + 1, z0 + 1, seed), sx),
        sz);
}

float SampleVisibleSplatWeight(const TerrainGrassTileInput& input, uint32 splatLayer, float gridX, float gridZ, float worldX, float worldZ)
{
    // TERRAIN_SPLAT_SHARPNESS, TERRAIN_SPLAT_NOISE_BREAKUP, TERRAIN_HEIGHT_BLEND_DEPTH
    constexpr float SplatSharpness = 1.35f;
    constexpr float SplatNoiseBreakup = 0.1f;
    constexpr float HeightBlendDepth = 0.1f;

    const float noise = ValueNoise(worldX * 0.035f, worldZ * 0.035f, 0x5A17u) - 0.5f;

    float layerWeight = 0.0f;
    float strongestOtherWeight = -1.0f;

    for (uint32 otherLayer = 0; otherLayer < TerrainNumSplatLayers; otherLayer++)
    {
        const float weight = MathUtil::Clamp((SampleSplatWeight(input, otherLayer, gridX, gridZ) - 0.5f) * SplatSharpness + 0.5f + noise * SplatNoiseBreakup, 0.0f, 1.0f);

        if (otherLayer == splatLayer)
        {
            layerWeight = weight;
        }
        else if (weight > 0.001f)
        {
            strongestOtherWeight = MathUtil::Max(strongestOtherWeight, weight);
        }
    }

    if (layerWeight <= 0.001f)
    {
        return 0.0f;
    }

    if (strongestOtherWeight < 0.0f)
    {
        return 1.0f;
    }

    const float lead = MathUtil::Clamp(layerWeight - strongestOtherWeight + HeightBlendDepth, -1.0f, 1.0f);

    if (lead < 0.0f)
    {
        return 0.5f * (1.0f + lead) * (1.0f + lead);
    }

    return 1.0f - 0.5f * (1.0f - lead) * (1.0f - lead);
}

// weighted pick (Gumbel-max) whose draws come mostly from smooth noise, so each type grows in drifts
uint32 PickCoverType(const TerrainCoverLayerPlan& layer, uint32 layerIndex, float worldX, float worldZ, GrassRandom& random)
{
    if (layer.types.Size() <= 1)
    {
        return 0;
    }

    constexpr float ClumpRandomness = 0.3f;

    const float clumpSize = MathUtil::Max(layer.clumpSize, 0.5f);

    uint32 bestType = 0;
    float bestScore = -MathUtil::Infinity<float>();

    for (uint32 typeIndex = 0; typeIndex < uint32(layer.types.Size()); typeIndex++)
    {
        const float noise = ValueNoise(worldX / clumpSize, worldZ / clumpSize, layerIndex * 131u + typeIndex * 7919u + 17u);
        const float draw = MathUtil::Clamp(MathUtil::Lerp(noise, random.Next(), ClumpRandomness), 0.001f, 0.999f);

        const float score = std::log(MathUtil::Max(layer.types[typeIndex].weight, 1e-6f)) - std::log(-std::log(draw));

        if (score > bestScore)
        {
            bestScore = score;
            bestType = typeIndex;
        }
    }

    return bestType;
}

} // namespace

void GenerateTile(const TerrainGrassTileInput& input, TerrainGrassTileOutput& output)
{
    uint32 numSlots = 0;

    for (const TerrainCoverLayerPlan& layer : input.layers)
    {
        for (const TerrainCoverTypePlan& type : layer.types)
        {
            numSlots += uint32(type.memberMatrices.Size());
        }
    }

    output.slots.Clear();
    output.slots.Resize(numSlots);

    if (input.cellSize < 2
        || input.splatWeights.Size() < size_t(input.cellSize) * input.cellSize * TerrainNumSplatLayers
        || input.cellScale.x <= 0.0f
        || input.cellScale.z <= 0.0f)
    {
        return;
    }

    const auto sampleHeight = [&input](float x, float z)
    {
        return TerrainMeshHelpers::SampleLodSurfaceHeight(input.paddedHeights, input.cellSize, 1, x, z) * input.cellScale.y;
    };

    const auto layerPaintPlane = [&input](uint32 layerIndex) -> int32
    {
        return layerIndex < input.layerPaintPlanes.Size() ? input.layerPaintPlanes[layerIndex] : -1;
    };

    const auto sampleTotalPaint = [&input, &layerPaintPlane](float gridX, float gridZ)
    {
        float totalPaint = 0.0f;

        for (uint32 layerIndex = 0; layerIndex < uint32(input.layers.Size()); layerIndex++)
        {
            if (input.layers[layerIndex].isPainted)
            {
                totalPaint += SamplePaintWeight(input, layerPaintPlane(layerIndex), gridX, gridZ);
            }
        }

        return MathUtil::Min(totalPaint, 1.0f);
    };

    uint32 layerFirstSlot = 0;

    // hoisted to preserve memory.
    Array<uint32, TerrainAllocator> typeFirstSlots;

    for (uint32 layerIndex = 0; layerIndex < uint32(input.layers.Size()); layerIndex++)
    {
        const TerrainCoverLayerPlan& layer = input.layers[layerIndex];

        typeFirstSlots.Resize(0);

        uint32 layerNumSlots = 0;

        for (const TerrainCoverTypePlan& type : layer.types)
        {
            typeFirstSlots.PushBack(layerFirstSlot + layerNumSlots);
            layerNumSlots += uint32(type.memberMatrices.Size());
        }

        const int32 paintPlane = layerPaintPlane(layerIndex);

        if (layer.types.Empty()
            || (layer.isPainted && paintPlane < 0)
            || (!layer.isPainted && layer.splatLayer >= TerrainNumSplatLayers))
        {
            layerFirstSlot += layerNumSlots;

            continue;
        }

        GrassRandom random(input.seed * 2654435761u + layerIndex);

        const float spacingWorld = MathUtil::Max(layer.spacing, 0.1f);
        const Vec2f spacing(spacingWorld / input.cellScale.x, spacingWorld / input.cellScale.z);

        const uint32 numX = uint32(MathUtil::Ceil(float(input.tileMax.x - input.tileMin.x) / spacing.x));
        const uint32 numZ = uint32(MathUtil::Ceil(float(input.tileMax.y - input.tileMin.y) / spacing.y));

        for (uint32 z = 0; z < numZ; z++)
        {
            for (uint32 x = 0; x < numX; x++)
            {
                const float gridX = float(input.tileMin.x) + (float(x) + random.Next()) * spacing.x;
                const float gridZ = float(input.tileMin.y) + (float(z) + random.Next()) * spacing.y;

                const float yaw = random.Range(0.0f, 2.0f * MathUtil::pi<float>);
                const float scaleJitter = random.Range(0.85f, 1.3f);
                const float acceptance = random.Next();

                if (gridX >= float(input.tileMax.x) || gridZ >= float(input.tileMax.y))
                {
                    continue;
                }

                const float worldX = input.cellMin.x + gridX * input.cellScale.x;
                const float worldZ = input.cellMin.z + gridZ * input.cellScale.z;

                const float edgeNoise = (ValueNoise(worldX / 1.7f, worldZ / 1.7f, 0x3C6Fu) - 0.5f) * 0.35f;

                const float coverage = layer.isPainted
                    ? MathUtil::SmoothStep(0.2f, 0.8f, SamplePaintWeight(input, paintPlane, gridX, gridZ) + edgeNoise)
                    : MathUtil::SmoothStep(0.15f, 0.85f, SampleVisibleSplatWeight(input, layer.splatLayer, gridX, gridZ, worldX, worldZ) + edgeNoise)
                        * (1.0f - MathUtil::SmoothStep(0.3f, 0.8f, sampleTotalPaint(gridX, gridZ)));

                if (acceptance >= coverage)
                {
                    continue;
                }

                const Vec3f position(worldX, input.cellMin.y + sampleHeight(gridX, gridZ), worldZ);

                const uint32 typeIndex = PickCoverType(layer, layerIndex, position.x, position.z, random);
                const TerrainCoverTypePlan& type = layer.types[typeIndex];

                const Vec3f groundNormal = Vec3f(
                    (sampleHeight(gridX - 1.0f, gridZ) - sampleHeight(gridX + 1.0f, gridZ)) / (2.0f * input.cellScale.x),
                    1.0f,
                    (sampleHeight(gridX, gridZ - 1.0f) - sampleHeight(gridX, gridZ + 1.0f)) / (2.0f * input.cellScale.z))
                                               .Normalized();

                // a sheared basis: the footprint lies flush on the ground, while the blades rise nearly upright
                const Vec3f up = MathUtil::Lerp(Vec3f::UnitY(), groundNormal, SlopeAlignment).Normalized();
                const Vec3f right = groundNormal.Cross(Vec3f(MathUtil::Cos(yaw), 0.0f, MathUtil::Sin(yaw))).Normalized();
                const Vec3f forward = right.Cross(groundNormal);

                const float heightScale = scaleJitter * MathUtil::Lerp(0.3f, 1.0f, coverage);
                const float widthScale = scaleJitter * MathUtil::Lerp(0.7f, 1.0f, coverage);

                Mat4f patchMatrix;
                Mat4f stretchedPatchMatrix;

                for (uint32 row = 0; row < 3; row++)
                {
                    patchMatrix[row][0] = right[row] * widthScale;
                    patchMatrix[row][1] = up[row] * heightScale;
                    patchMatrix[row][2] = forward[row] * widthScale;
                    patchMatrix[row][3] = position[row];

                    // the vertex shader widens patches with distance, up to this
                    stretchedPatchMatrix[row][0] = patchMatrix[row][0] * TerrainGrassFarStretch;
                    stretchedPatchMatrix[row][1] = patchMatrix[row][1];
                    stretchedPatchMatrix[row][2] = patchMatrix[row][2] * TerrainGrassFarStretch;
                    stretchedPatchMatrix[row][3] = patchMatrix[row][3];
                }

                for (uint32 memberIndex = 0; memberIndex < uint32(type.memberMatrices.Size()); memberIndex++)
                {
                    const Mat4f instanceMatrix = patchMatrix * type.memberMatrices[memberIndex];

                    if (input.isFar && GetInstanceRank(Vec3f(instanceMatrix[0][3], instanceMatrix[1][3], instanceMatrix[2][3])) >= TerrainGrassFarRankCutoff)
                    {
                        continue;
                    }

                    TerrainGrassSlotInstances& slot = output.slots[typeFirstSlots[typeIndex] + memberIndex];

                    slot.transforms.PushBack(instanceMatrix);
                    slot.bounds = slot.bounds.Union(stretchedPatchMatrix * type.memberBounds[memberIndex]);
                }
            }
        }

        layerFirstSlot += layerNumSlots;
    }
}

} // namespace TerrainGrass

#pragma region TerrainGroundCoverResources

const Handle<GroundCover>& TerrainGroundCoverResources::GetGroundCover() const
{
    if (!m_groundCover.IsValid())
    {
        return Handle<GroundCover>::empty;
    }

    return TryCast<GroundCover>(m_groundCover.Resolve());
}

Handle<GroundCover> TerrainGroundCoverResources::GetPlantedGroundCover() const
{
    Handle<GroundCover> groundCover = GetGroundCover();

    return groundCover.IsValid() ? groundCover : GetDefaultGroundCover();
}

void TerrainGroundCoverResources::SetGroundCover(const Handle<GroundCover>& groundCover)
{
    m_groundCover = groundCover.IsValid() ? AssetReference(Handle<AssetObject>(groundCover)) : AssetReference();

    Invalidate();
}

void TerrainGroundCoverResources::SetGroundCoverPath(const AssetPath& assetPath)
{
    m_groundCover = AssetReference(assetPath);

    Invalidate();
}

void TerrainGroundCoverResources::Invalidate()
{
    m_isResolved = false;
}

const Array<TerrainCoverLayer, TerrainAllocator>& TerrainGroundCoverResources::GetLayers()
{
    Resolve();

    return m_layers;
}

const Array<TerrainCoverLayerPlan, TerrainAllocator>& TerrainGroundCoverResources::GetPlans()
{
    Resolve();

    return m_plans;
}

Array<Name> TerrainGroundCoverResources::GetPaintedLayerNames()
{
    Resolve();

    Array<Name> names;

    for (const TerrainCoverLayerPlan& plan : m_plans)
    {
        if (plan.isPainted)
        {
            names.PushBack(plan.name);
        }
    }

    return names;
}

uint32 TerrainGroundCoverResources::GetNumSlots()
{
    Resolve();

    return m_numSlots;
}

uint32 TerrainGroundCoverResources::GetVersion()
{
    Resolve();

    return m_version;
}

void TerrainGroundCoverResources::Refresh()
{
    AssertOnThread(g_simThread);

    if (!m_isResolved)
    {
        return;
    }

    const Handle<GroundCover> groundCover = GetGroundCover();

    if (groundCover.IsValid() && groundCover->GetContentHashCode().Value() != m_contentHashCode)
    {
        Invalidate();
    }
}

const TerrainGroundCoverResources::CoverMaterial& TerrainGroundCoverResources::GetCoverMaterial(
    const Handle<Material>& source,
    const Vec4f& groundCoverParameters,
    Array<CoverMaterial, TerrainAllocator>& inOutPreviousMaterials)
{
    const auto matches = [&source, &groundCoverParameters](const CoverMaterial& coverMaterial)
    {
        return coverMaterial.source == source
            && coverMaterial.groundCoverParameters == groundCoverParameters;
    };

    if (const auto it = m_coverMaterials.FindIf(matches); it != m_coverMaterials.End())
    {
        return *it;
    }

    if (const auto it = inOutPreviousMaterials.FindIf(matches); it != inOutPreviousMaterials.End())
    {
        m_coverMaterials.PushBack(std::move(*it));
        inOutPreviousMaterials.Erase(it);

        return m_coverMaterials.Back();
    }

    CoverMaterial coverMaterial;
    coverMaterial.source = source;
    coverMaterial.groundCoverParameters = groundCoverParameters;

    MaterialParameters parameters = source->GetParameters();
    parameters.colorVariation = groundCoverParameters.x;
    parameters.groundNormalBlend = groundCoverParameters.y;
    parameters.baseOcclusion = groundCoverParameters.z;
    parameters.baseOcclusionHeight = groundCoverParameters.w;

    // a prefab already authored as ground cover is planted as is
    if (parameters == source->GetParameters())
    {
        coverMaterial.material = source;
    }
    else
    {
        coverMaterial.material = source->Clone();
        coverMaterial.material->SetName(NAME_FMT("{}_GroundCover", source->GetName()));
        coverMaterial.material->SetParameters(parameters);

        InitObject(coverMaterial.material);
    }

    coverMaterial.materialNoShadows = coverMaterial.material->Clone();
    coverMaterial.materialNoShadows->SetName(NAME_FMT("{}_NoShadows", coverMaterial.material->GetName()));

    MaterialAttributes attributes = coverMaterial.materialNoShadows->GetAttributes();
    attributes.flags |= MaterialAttributeFlags::DisableShadowCasting;
    coverMaterial.materialNoShadows->SetAttributes(attributes);

    InitObject(coverMaterial.materialNoShadows);

    m_coverMaterials.PushBack(std::move(coverMaterial));

    return m_coverMaterials.Back();
}

void TerrainGroundCoverResources::Resolve()
{
    // clumps are tufts with gaps between them, so they're planted overlapping to close those
    constexpr float SpacingPerFootprint = 0.47f;

    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_isResolved)
    {
        return;
    }

    m_isResolved = true;
    m_version++;

    m_layers.Clear();
    m_plans.Clear();
    m_numSlots = 0;
    m_contentHashCode = 0;

    // only the materials still planted are kept
    Array<CoverMaterial, TerrainAllocator> previousCoverMaterials = std::move(m_coverMaterials);
    m_coverMaterials.Clear();

    if (const Handle<GroundCover>& ownGroundCover = GetGroundCover(); ownGroundCover.IsValid())
    {
        m_contentHashCode = ownGroundCover->GetContentHashCode().Value();
    }

    Handle<GroundCover> groundCover = GetPlantedGroundCover();

    if (groundCover.IsValid())
    {
        for (const GroundCoverLayer& coverLayer : groundCover->layers)
        {
            const bool isPainted = coverLayer.source == GroundCoverSource::Painted;

            if (isPainted && !coverLayer.name.IsValid())
            {
                HYP_LOG(WorldGrid, Warning, "Ground cover '{}' has a painted layer with no name",
                    groundCover->GetName());

                continue;
            }

            TerrainCoverLayer layer;
            TerrainCoverLayerPlan plan;

            plan.name = coverLayer.name;
            plan.isPainted = isPainted;
            plan.splatLayer = MathUtil::Min(coverLayer.splatLayer, TerrainNumSplatLayers - 1);
            plan.clumpSize = MathUtil::Max(coverLayer.clumpSize, 0.5f);

            float weightedFootprint = 0.0f;
            float totalWeight = 0.0f;

            for (const GroundCoverType& coverType : coverLayer.types)
            {
                if (!coverType.prefab.IsValid())
                {
                    HYP_LOG(WorldGrid, Warning, "Ground cover '{}' has a type with no prefab, skipping it", groundCover->GetName());

                    continue;
                }

                if (coverType.weight <= 0.0f)
                {
                    continue;
                }

                Array<InstanceGroupMember> members;
                InstanceGroup::CollectMembers(*coverType.prefab, members);

                if (members.Empty())
                {
                    HYP_LOG(WorldGrid, Warning, "Ground cover '{}': prefab '{}' has no static meshes to plant", groundCover->GetName(), coverType.prefab->GetName());

                    continue;
                }

                TerrainCoverType type;
                type.firstSlot = m_numSlots;

                TerrainCoverTypePlan typePlan;
                typePlan.weight = coverType.weight;

                BoundingBox typeBounds = BoundingBox::Empty();

                for (const InstanceGroupMember& member : members)
                {
                    const MaterialParameters& sourceParameters = member.material->GetParameters();

                    const Vec4f groundCoverParameters(
                        coverType.colorVariation,
                        coverType.groundNormalBlend,
                        coverType.baseOcclusion,
                        MathUtil::Max(sourceParameters.baseOcclusionHeight, member.mesh->GetAABB().max.y * 0.5f));

                    const CoverMaterial& coverMaterial = GetCoverMaterial(member.material, groundCoverParameters, previousCoverMaterials);

                    type.members.PushBack(TerrainCoverMember { member.mesh, coverMaterial.material, coverMaterial.materialNoShadows });

                    typePlan.memberMatrices.PushBack(member.matrix);
                    typePlan.memberBounds.PushBack(member.bounds);

                    typeBounds = typeBounds.Union(member.bounds);
                }

                m_numSlots += uint32(members.Size());

                const Vec3f extent = typeBounds.GetExtent();

                weightedFootprint += MathUtil::Max(extent.x, extent.z) * coverType.weight;
                totalWeight += coverType.weight;

                layer.types.PushBack(std::move(type));
                plan.types.PushBack(std::move(typePlan));
            }

            if (layer.types.Empty())
            {
                continue;
            }

            plan.spacing = (weightedFootprint / totalWeight) * SpacingPerFootprint / MathUtil::Sqrt(MathUtil::Max(coverLayer.density, 0.01f));
            plan.coverage = MathUtil::Clamp(coverLayer.density, 0.0f, 1.0f);

            m_layers.PushBack(std::move(layer));
            m_plans.PushBack(std::move(plan));
        }

        HYP_LOG(WorldGrid, Info, "Ground cover '{}' resolved: {} layers, {} meshes", groundCover->GetName(), m_layers.Size(), m_numSlots);
    }
}

Handle<GroundCover> TerrainGroundCoverResources::GetDefaultGroundCover()
{
    Handle<GroundCover> groundCover = EngineDefaults::LoadAsset<GroundCover>(EngineDefaults::Get().terrain.groundCover);

    if (!groundCover.IsValid())
    {
        HYP_LOG_ONCE(WorldGrid, Warning, "Engine asset DefaultGroundCover is missing - terrain without a GroundCover plants nothing. Run buildshapes --parts=groundcover");
    }

    return groundCover;
}

#pragma endregion TerrainGroundCoverResources

} // namespace Hyperion
