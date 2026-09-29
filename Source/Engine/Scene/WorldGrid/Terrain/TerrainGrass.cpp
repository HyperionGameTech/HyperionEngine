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

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <cmath>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

namespace {

struct GrassRandom
{
    uint32 state;

    explicit GrassRandom(uint32 seed)
        : state(seed * 747796405u + 2891336453u)
    {
    }

    float Next()
    {
        state = state * 747796405u + 2891336453u;
        uint32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        word = (word >> 22u) ^ word;

        return float(word) * (1.0f / 4294967296.0f);
    }

    float Range(float from, float to)
    {
        return from + (to - from) * Next();
    }
};

// how far a patch tilts from vertical toward the slope it stands on - grass still grows mostly upward
static constexpr float s_slopeAlignment = 0.6f;

static float SampleSplatWeight(const TerrainGrassTileInput& input, uint32 splatLayer, float gridX, float gridZ)
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

    const auto at = [&input, splatLayer](uint32 x, uint32 z)
    {
        return float(input.splatWeights[(size_t(z) * input.cellSize + x) * TerrainNumSplatLayers + splatLayer]) * (1.0f / 255.0f);
    };

    return MathUtil::Lerp(
        MathUtil::Lerp(at(x0, z0), at(x1, z0), fx),
        MathUtil::Lerp(at(x0, z1), at(x1, z1), fx),
        fz);
}

static float SamplePaintWeight(const TerrainGrassTileInput& input, int32 plane, float gridX, float gridZ)
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

    const auto at = [&input, planeWeights](uint32 x, uint32 z)
    {
        return float(planeWeights[size_t(z) * input.cellSize + x]) * (1.0f / 255.0f);
    };

    return MathUtil::Lerp(
        MathUtil::Lerp(at(x0, z0), at(x1, z0), fx),
        MathUtil::Lerp(at(x0, z1), at(x1, z1), fx),
        fz);
}

static float HashToUnit(int32 x, int32 z, uint32 seed)
{
    uint32 state = (uint32(x) * 0x8DA6B343u) ^ (uint32(z) * 0xD8163841u) ^ (seed * 0xCB1AB31Fu);
    state = state * 747796405u + 2891336453u;
    state = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    state = (state >> 22u) ^ state;

    return float(state) * (1.0f / 4294967296.0f);
}

static float ValueNoise(float x, float z, uint32 seed)
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

// weighted pick (Gumbel-max) whose draws come mostly from smooth noise, so each type grows in drifts
static uint32 PickCoverType(const TerrainCoverLayerPlan& layer, uint32 layerIndex, float worldX, float worldZ, GrassRandom& random)
{
    if (layer.types.Size() <= 1)
    {
        return 0;
    }

    static constexpr float s_clumpRandomness = 0.3f;

    const float clumpSize = MathUtil::Max(layer.clumpSize, 0.5f);

    uint32 bestType = 0;
    float bestScore = -MathUtil::Infinity<float>();

    for (uint32 typeIndex = 0; typeIndex < uint32(layer.types.Size()); typeIndex++)
    {
        const float noise = ValueNoise(worldX / clumpSize, worldZ / clumpSize, layerIndex * 131u + typeIndex * 7919u + 17u);
        const float draw = MathUtil::Clamp(MathUtil::Lerp(noise, random.Next(), s_clumpRandomness), 0.001f, 0.999f);

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

namespace TerrainGrass {

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

    for (uint32 layerIndex = 0; layerIndex < uint32(input.layers.Size()); layerIndex++)
    {
        const TerrainCoverLayerPlan& layer = input.layers[layerIndex];

        Array<uint32> typeFirstSlots;
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

        const float spacingWorld = MathUtil::Max(layer.spacing, 0.1f) * input.stretch;
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

                // weights are soft at the edges of an area; patches thin out and shrink across that band
                const float coverage = layer.isPainted
                    ? MathUtil::SmoothStep(0.3f, 0.8f, SamplePaintWeight(input, paintPlane, gridX, gridZ))
                    : MathUtil::SmoothStep(0.3f, 0.8f, SampleSplatWeight(input, layer.splatLayer, gridX, gridZ))
                        * (1.0f - MathUtil::SmoothStep(0.3f, 0.8f, sampleTotalPaint(gridX, gridZ)));

                if (acceptance >= coverage)
                {
                    continue;
                }

                const float height = sampleHeight(gridX, gridZ);

                const Vec3f position(
                    input.cellMin.x + gridX * input.cellScale.x,
                    input.cellMin.y + height,
                    input.cellMin.z + gridZ * input.cellScale.z);

                const uint32 typeIndex = PickCoverType(layer, layerIndex, position.x, position.z, random);
                const TerrainCoverTypePlan& type = layer.types[typeIndex];

                const Vec3f groundNormal = Vec3f(
                    (sampleHeight(gridX - 1.0f, gridZ) - sampleHeight(gridX + 1.0f, gridZ)) / (2.0f * input.cellScale.x),
                    1.0f,
                    (sampleHeight(gridX, gridZ - 1.0f) - sampleHeight(gridX, gridZ + 1.0f)) / (2.0f * input.cellScale.z))
                                               .Normalized();

                // built as a basis rather than composed from quaternions: yaw about the tilted up axis
                const Vec3f up = MathUtil::Lerp(Vec3f::UnitY(), groundNormal, s_slopeAlignment).Normalized();
                const Vec3f right = up.Cross(Vec3f(MathUtil::Cos(yaw), 0.0f, MathUtil::Sin(yaw))).Normalized();
                const Vec3f forward = right.Cross(up);

                const float scale = scaleJitter * MathUtil::Lerp(0.65f, 1.0f, coverage);

                // stretched patches grow some height too, or far clumps flatten into wide blobs
                const float stretchHeight = MathUtil::Sqrt(input.stretch);
                const Vec3f axes[3] = { right * (scale * input.stretch), up * (scale * stretchHeight), forward * (scale * input.stretch) };

                Mat4f patchMatrix;

                for (uint32 row = 0; row < 3; row++)
                {
                    patchMatrix[row][0] = axes[0][row];
                    patchMatrix[row][1] = axes[1][row];
                    patchMatrix[row][2] = axes[2][row];
                    patchMatrix[row][3] = position[row];
                }

                for (uint32 memberIndex = 0; memberIndex < uint32(type.memberMatrices.Size()); memberIndex++)
                {
                    TerrainGrassSlotInstances& slot = output.slots[typeFirstSlots[typeIndex] + memberIndex];

                    slot.transforms.PushBack(patchMatrix * type.memberMatrices[memberIndex]);
                    slot.bounds = slot.bounds.Union(patchMatrix * type.memberBounds[memberIndex]);
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

    return DynamicCast<GroundCover>(m_groundCover.Resolve());
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

const Array<TerrainCoverLayer>& TerrainGroundCoverResources::GetLayers()
{
    Resolve();

    return m_layers;
}

const Array<TerrainCoverLayerPlan>& TerrainGroundCoverResources::GetPlans()
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

const Handle<Material>& TerrainGroundCoverResources::GetNoShadowMaterial(const Handle<Material>& material)
{
    for (uint32 index = 0; index < uint32(m_noShadowSources.Size()); index++)
    {
        if (m_noShadowSources[index] == material)
        {
            return m_noShadowMaterials[index];
        }
    }

    Handle<Material> noShadowMaterial = material->Clone();
    noShadowMaterial->SetName(NAME_FMT("{}_NoShadows", material->GetName()));

    MaterialAttributes attributes = noShadowMaterial->GetAttributes();
    attributes.flags |= MAF_DISABLE_SHADOW_CASTING;
    noShadowMaterial->SetAttributes(attributes);

    InitObject(noShadowMaterial);

    m_noShadowSources.PushBack(material);
    m_noShadowMaterials.PushBack(std::move(noShadowMaterial));

    return m_noShadowMaterials.Back();
}

void TerrainGroundCoverResources::Resolve()
{
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

    // clumps are tufts with gaps between them, so they're planted overlapping to close those
    static constexpr float s_spacingPerFootprint = 0.47f;

    const Handle<GroundCover>& groundCover = GetGroundCover();

    if (!groundCover.IsValid())
    {
        groundCover = GetDefaultGroundCover();
    }

    if (groundCover.IsValid())
    {
        for (const GroundCoverLayer& coverLayer : groundCover->layers)
        {
            const bool isPainted = coverLayer.source == GroundCoverSource::Painted;

            if (isPainted && !coverLayer.name.IsValid())
            {
                HYP_LOG(WorldGrid, Warning, "Ground cover '{}' has a painted layer with no name - it can't be painted, skipping it", groundCover->GetName());

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
                    type.members.PushBack(TerrainCoverMember { member.mesh, member.material, GetNoShadowMaterial(member.material) });

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

            plan.spacing = (weightedFootprint / totalWeight) * s_spacingPerFootprint / MathUtil::Sqrt(MathUtil::Max(coverLayer.density, 0.01f));

            m_layers.PushBack(std::move(layer));
            m_plans.PushBack(std::move(plan));
        }

        HYP_LOG(WorldGrid, Info, "Ground cover '{}' resolved: {} layers, {} meshes", groundCover->GetName(), m_layers.Size(), m_numSlots);
    }
}

Handle<GroundCover> TerrainGroundCoverResources::GetDefaultGroundCover()
{
    static const Name s_defaultGroundCoverName = NAME("DefaultGroundCover");

    const Handle<AssetRegistry>& engineRegistry = GetEngineAssetRegistry();

    if (!engineRegistry.IsValid())
    {
        return Handle<GroundCover>::empty;
    }

    Handle<GroundCover> groundCover = DynamicCast<GroundCover>(engineRegistry->GetAsset(AssetBuckets::Terrain, s_defaultGroundCoverName));

    if (!groundCover.IsValid())
    {
        HYP_LOG_ONCE(WorldGrid, Warning, "Engine asset DefaultGroundCover is missing - terrain without a GroundCover plants nothing. Run buildshapes --parts=groundcover");
    }

    return groundCover;
}

#pragma endregion TerrainGroundCoverResources

} // namespace Hyperion
