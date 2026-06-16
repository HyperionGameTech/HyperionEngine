/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Scene/ProbeVolume.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/World.hpp>

#include <Scene/Components/LightmapElementComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

#include <Rendering/RenderProxy.hpp>

#include <ProbeVolume.generated.inl>

#include <algorithm>
#include <cmath>

namespace Hyperion {

#pragma region Tetrahedron face key (used for adjacency)

struct SortedFace
{
    uint32 v[3];

    SortedFace(uint32 a, uint32 b, uint32 c)
    {
        v[0] = a;
        v[1] = b;
        v[2] = c;

        if (v[0] > v[1])
            std::swap(v[0], v[1]);
        if (v[1] > v[2])
            std::swap(v[1], v[2]);
        if (v[0] > v[1])
            std::swap(v[0], v[1]);
    }

    bool operator==(const SortedFace& other) const
    {
        return v[0] == other.v[0]
            && v[1] == other.v[1]
            && v[2] == other.v[2];
    }
};

#pragma endregion

#pragma region ProbeVolume

ProbeVolume::ProbeVolume()
    : VolumeBase()
{
}

ProbeVolume::ProbeVolume(Name name, const BoundingBox& localBounds)
    : VolumeBase(name, localBounds)
{
}

ProbeVolume::ProbeVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
}

ProbeVolume::~ProbeVolume()
{
    m_probes.Clear();
    m_probes.Refit();

    m_tetrahedra.Clear();
    m_tetrahedra.Refit();
}

void ProbeVolume::OnAddedToWorld(World* world)
{
    m_probes.Clear();
    m_probes.Reserve(m_gridCount.Volume());

    for (Node* node : GetChildren())
    {
        if (node->IsA<IrradianceProbe>())
        {
            IrradianceProbe* probe = StaticCast<IrradianceProbe>(node);

#if HYP_EDITOR
            probe->useVolumeEditTool = false;
#endif // HYP_EDITOR

            m_probes.PushBack(probe);
        }
    }

    RebuildRuntimeData();
}

void ProbeVolume::OnRemovedFromWorld(World* world)
{
    m_probes.Clear();
    m_probes.Refit();

    {
        TUniqueLock lock(m_cachedState.mutex);

        m_cachedState.entityCellCache.Clear();
        m_cachedState.lastTetHint = 0;
    }
}

void ProbeVolume::OnTransformUpdated()
{
    VolumeBase::OnTransformUpdated();

    for (IrradianceProbe* probe : m_probes)
    {
        RefreshProbe(*probe);
    }
}

void ProbeVolume::UpdateRenderProxy(RenderProxyProbeVolume* proxy)
{
    *proxy = {};
    proxy->probeVolume = MakeWeakRef(this);
    proxy->bufferData = {};
}

void ProbeVolume::RebuildRuntimeData()
{
    if (m_tetrahedra.Empty() || m_probes.Empty())
    {
        return;
    }

    for (Tetrahedron& tet : m_tetrahedra)
    {
        const Vec3f p0 = m_probes[tet.probeIndices[0]]->GetWorldTranslation();
        const Vec3f e1 = m_probes[tet.probeIndices[1]]->GetWorldTranslation() - p0;
        const Vec3f e2 = m_probes[tet.probeIndices[2]]->GetWorldTranslation() - p0;
        const Vec3f e3 = m_probes[tet.probeIndices[3]]->GetWorldTranslation() - p0;

        const float edgeData[9] = {
            e1.x, e2.x, e3.x,
            e1.y, e2.y, e3.y,
            e1.z, e2.z, e3.z
        };

        tet.invEdgeMatrix = Mat3f(edgeData).Inverse();

        tet.neighbours[0] = -1;
        tet.neighbours[1] = -1;
        tet.neighbours[2] = -1;
        tet.neighbours[3] = -1;
    }

    const uint32 numTets = m_tetrahedra.Size();

    for (uint32 i = 0; i < numTets; i++)
    {
        Tetrahedron& tet = m_tetrahedra[i];

        for (uint32 fi = 0; fi < 4; fi++)
        {
            if (tet.neighbours[fi] != -1)
            {
                continue;
            }

            uint32 fv[3];
            uint32 k = 0;

            for (uint32 j = 0; j < 4; j++)
            {
                if (j != fi)
                {
                    fv[k++] = tet.probeIndices[j];
                }
            }

            const SortedFace key(fv[0], fv[1], fv[2]);

            for (uint32 j = i + 1; j < numTets; j++)
            {
                if (i == j)
                {
                    continue;
                }

                Tetrahedron& other = m_tetrahedra[j];

                for (uint32 ofi = 0; ofi < 4; ofi++)
                {
                    uint32 ofv[3];
                    uint32 ok = 0;

                    for (uint32 oj = 0; oj < 4; oj++)
                    {
                        if (oj != ofi)
                        {
                            ofv[ok++] = other.probeIndices[oj];
                        }
                    }

                    if (SortedFace(ofv[0], ofv[1], ofv[2]) == key)
                    {
                        tet.neighbours[fi] = static_cast<int32>(j);
                        other.neighbours[ofi] = static_cast<int32>(i);

                        break;
                    }
                }

                if (tet.neighbours[fi] != -1)
                {
                    break;
                }
            }
        }
    }

    {
        TUniqueLock lock(m_cachedState.mutex);

        m_cachedState.entityCellCache.Clear();
        m_cachedState.lastTetHint = 0;
    }
}

void ProbeVolume::RemoveStaleCacheEntries()
{
    TUniqueLock lock(m_cachedState.mutex);

    for (auto it = m_cachedState.entityCellCache.Begin(); it != m_cachedState.entityCellCache.End();)
    {
        Handle<Entity> entity = it->first.Lock();
        if (!entity.IsValid())
        {
            it = m_cachedState.entityCellCache.Erase(it);
            continue;
        }

        ++it;
    }
}

/// Light Probe Interpolation Using Tetrahedral Tessellations, Robert Cupisz
/// https://gdcvault.com/play/1015312/Light-Probe-Interpolation-Using-Tetrahedral
///
/// Returns true if an enclosing tetrahedron was found, with barycentric
/// weights in outWeights. Uses per-entity cell caching for temporal
/// coherence and adjacency walking for fast traversal.
bool ProbeVolume::FindEnclosingTetrahedron(
    const Vec3f& position,
    int32& inOutTetIndex,
    Vec4f& outWeights) const
{
    const int32 lastTetHint = inOutTetIndex;

    auto computeWeights = [this](int32 tetIdx, const Vec3f& pos) -> Vec4f
    {
        const Tetrahedron& tet = m_tetrahedra[tetIdx];
        const Vec3f d = pos - m_probes[tet.probeIndices[0]]->GetWorldTranslation();

        Vec4f w;
        w[1] = tet.invEdgeMatrix[0][0] * d.x + tet.invEdgeMatrix[0][1] * d.y + tet.invEdgeMatrix[0][2] * d.z;
        w[2] = tet.invEdgeMatrix[1][0] * d.x + tet.invEdgeMatrix[1][1] * d.y + tet.invEdgeMatrix[1][2] * d.z;
        w[3] = tet.invEdgeMatrix[2][0] * d.x + tet.invEdgeMatrix[2][1] * d.y + tet.invEdgeMatrix[2][2] * d.z;
        w[0] = 1.0f - w[1] - w[2] - w[3];

        return w;
    };

    auto cellContains = [](const Vec4f& w) -> bool
    {
        const float eps = -1e-5f;
        return w[0] >= eps && w[1] >= eps && w[2] >= eps && w[3] >= eps;
    };

    const int32 numTets = static_cast<int32>(m_tetrahedra.Size());
    if (numTets == 0)
    {
        return false;
    }

    // Try O(1) grid-cell lookup first
    if (m_gridCount.x > 1 && m_gridCount.y > 1 && m_gridCount.z > 1)
    {
        const BoundingBox worldBounds = GetWorldBounds();
        const Vec3f extent = worldBounds.GetExtent();

        const Vec3f halfCellSize = (extent / Vec3f(m_gridCount)) * 0.5f;

        // The actual space covered by the tetrahedral mesh
        const Vec3f meshMin = worldBounds.min + halfCellSize;
        const Vec3f meshMax = worldBounds.max - halfCellSize;
        const Vec3f meshSpan = meshMax - meshMin;

        if (meshSpan.x > 0.0f && meshSpan.y > 0.0f && meshSpan.z > 0.0f)
        {
            const Vec3f relativePos = position - meshMin;

            const int32 numCellsX = m_gridCount.x - 1;
            const int32 numCellsY = m_gridCount.y - 1;
            const int32 numCellsZ = m_gridCount.z - 1;

            const int32 gx = MathUtil::Clamp(static_cast<int32>((relativePos.x / meshSpan.x) * numCellsX), 0, numCellsX - 1);
            const int32 gy = MathUtil::Clamp(static_cast<int32>((relativePos.y / meshSpan.y) * numCellsY), 0, numCellsY - 1);
            const int32 gz = MathUtil::Clamp(static_cast<int32>((relativePos.z / meshSpan.z) * numCellsZ), 0, numCellsZ - 1);

            const int32 cellLinear = (gx * numCellsY * numCellsZ) + (gy * numCellsZ) + gz;

            const int32 tetStart = cellLinear * 6;
            const int32 tetEnd = MathUtil::Min(tetStart + 6, numTets);

            for (int32 i = tetStart; i < tetEnd; i++)
            {
                const Vec4f w = computeWeights(i, position);
                if (cellContains(w))
                {
                    inOutTetIndex = i;
                    outWeights = w;
                    return true;
                }
            }
        }
    }

    // Fallback: adjacency walking from cached cell or global hint
    int32 current = -1;

    if (inOutTetIndex >= 0 && inOutTetIndex < numTets)
    {
        current = inOutTetIndex;
    }
    else if (lastTetHint >= 0 && lastTetHint < numTets)
    {
        current = lastTetHint;
    }
    else
    {
        current = 0;
    }

    int32 previous = -1;
    const int32 maxSteps = numTets + 1;

    for (int32 step = 0; step < maxSteps; step++)
    {
        const Vec4f w = computeWeights(current, position);

        if (cellContains(w))
        {
            inOutTetIndex = current;
            outWeights = w;

            return true;
        }

        int32 worstFace = 0;
        float worstVal = w[0];

        if (w[1] < worstVal)
        {
            worstVal = w[1];
            worstFace = 1;
        }

        if (w[2] < worstVal)
        {
            worstVal = w[2];
            worstFace = 2;
        }

        if (w[3] < worstVal)
        {
            worstVal = w[3];
            worstFace = 3;
        }

        const int32 next = m_tetrahedra[current].neighbours[worstFace];

        if (next == -1)
        {
            // We hit the outer hull of the tetrahedral mesh
            inOutTetIndex = current;

            // Clamp negative weights to 0 to prevent inverted/negative light multipliers
            outWeights.x = MathUtil::Max(w.x, 0.0f);
            outWeights.y = MathUtil::Max(w.y, 0.0f);
            outWeights.z = MathUtil::Max(w.z, 0.0f);
            outWeights.w = MathUtil::Max(w.w, 0.0f);

            // Normalize the weights so they sum to exactly 1.0
            const float weightSum = outWeights.x + outWeights.y + outWeights.z + outWeights.w;
            if (weightSum > 0.00001f)
            {
                outWeights /= weightSum;
            }
            else
            {
                // never hit partically, unless all probes were at the same position.
                outWeights = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
            }

            return true;
        }

        if (next == previous)
        {
            inOutTetIndex = current;
            outWeights = w;
            return true;
        }

        previous = current;
        current = next;
    }

    inOutTetIndex = current;
    outWeights = computeWeights(current, position);

    return true;
}

EvaluateSphericalHarmonicsResult ProbeVolume::EvaluateSphericalHarmonics(
    const Entity& inEntity, SphericalHarmonicsData& out) const
{
    Vec3f position = inEntity.GetWorldTranslation();

    // Clamp sample position to volume bounds for cheap extrapolation.
    // See: Cupisz, "Light Probe Interpolation Using Tetrahedral Tessellations", GDC 2012.
    {
        const BoundingBox worldBounds = GetWorldBounds();
        const Vec3f extent = worldBounds.GetExtent();

        // Calculate the half-cell inset where the outermost probes actually sit
        const Vec3f halfCellSize = (extent / Vec3f(m_gridSize)) * 0.5f;

        const Vec3f min = worldBounds.min + halfCellSize;
        const Vec3f max = worldBounds.max - halfCellSize;

        position.x = MathUtil::Clamp(position.x, min.x, max.x);
        position.y = MathUtil::Clamp(position.y, min.y, max.y);
        position.z = MathUtil::Clamp(position.z, min.z, max.z);
    }

    int32 tetHint = -1;

    CachedState& cachedState = m_cachedState;

    // Check per-entity cell cache first
    {
        TSharedLock lock(cachedState.mutex);

        const auto cacheIt = cachedState.entityCellCache.FindAs(inEntity.Id());
        if (cacheIt != cachedState.entityCellCache.End())
        {
            tetHint = cacheIt->second;
        }
    }

    int32 tetIdx = tetHint;
    Vec4f weights;

    if (!FindEnclosingTetrahedron(position, tetIdx, weights))
    {
        return EvaluateSphericalHarmonicsResult::Failure_OutsideOfVolume;
    }

    // Update caches
    {
        TUniqueLock lock(cachedState.mutex);

        cachedState.entityCellCache[MakeWeakRef(&inEntity)] = tetIdx;
        cachedState.lastTetHint = tetIdx;
    }

    const Tetrahedron& tet = m_tetrahedra[tetIdx];

    const SphericalHarmonicsData& shA = m_probes[tet.probeIndices[0]]->GetSphericalHarmonicsData();
    const SphericalHarmonicsData& shB = m_probes[tet.probeIndices[1]]->GetSphericalHarmonicsData();
    const SphericalHarmonicsData& shC = m_probes[tet.probeIndices[2]]->GetSphericalHarmonicsData();
    const SphericalHarmonicsData& shD = m_probes[tet.probeIndices[3]]->GetSphericalHarmonicsData();

    out = shA * weights[0]
        + shB * weights[1]
        + shC * weights[2]
        + shD * weights[3];

    // // Temp debug.
    // for (int i = 0; i < 9; i++)
    // {
    //     out.values[i * 3] = 1.0f;
    //     out.values[i * 3 + 1] = 0.0f;
    //     out.values[i * 3 + 2] = 0.0f;
    // }

    return EvaluateSphericalHarmonicsResult::Success_InTetra;
}

void ProbeVolume::RemoveAllProbes(bool freeMemory)
{
    auto childNodes = GetChildren();

    for (Node* node : childNodes)
    {
        if (node->IsA<IrradianceProbe>())
        {
            RemoveChild(node, /* moveToDetached */ false);
        }
    }

    m_probes.Clear();

    if (freeMemory)
    {
        m_probes.Refit();
    }
}

void ProbeVolume::RefreshProbe(IrradianceProbe& probe)
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    const BoundingBox probeBounds = probe.GetLocalBounds();
    const Mat4f worldTransform = probe.GetWorldMatrix();
    const BoundingBox worldBounds = worldTransform * probeBounds;

    Array<Entity*, ThreadAllocator> overlappingEntities;

    for (Scene* scene : world->GetScenes())
    {
        for (auto&& [entity, meshComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            const BoundingBox entityWorldBounds = entity->GetWorldBounds();

            if (entityWorldBounds.Overlaps(worldBounds))
            {
                overlappingEntities.PushBack(entity);
            }
        }
    }

    for (Entity* entity : overlappingEntities)
    {
        entity->AddTag<EntityTag::UpdateSphericalHarmonicsData>();
        entity->AddTag<EntityTag::UpdateRenderProxy>();

        if (entity->HasComponent<LightmapElementComponent>())
        {
            continue;
        }

        LightmapElementComponent component {};
        entity->AddComponent<LightmapElementComponent>(component);
    }

    probe.needsRender.Store(true);
}

#if HYP_EDITOR

void ProbeVolume::SetGridSize(const Vec3u& gridSize)
{
    if (m_gridSize == gridSize)
    {
        return;
    }

    m_gridSize = gridSize;

    MarkDirty();

    CreateProbes();
}

void ProbeVolume::CreateProbes()
{
    RemoveAllProbes(false);

    const BoundingBox localBounds = GetLocalBounds();
    const Vec3f extent = localBounds.GetExtent();

    const Vec3f cellSize = Vec3f(
        extent.x / float(m_gridSize.x),
        extent.y / float(m_gridSize.y),
        extent.z / float(m_gridSize.z));

    m_probes.Reserve(m_gridSize.Volume());

    uint32 seed = static_cast<uint32>(Time::Now().ToMilliseconds() % UINT32_MAX);

    for (uint32 z = 0; z < m_gridSize.z; z++)
    {
        for (uint32 y = 0; y < m_gridSize.y; y++)
        {
            for (uint32 x = 0; x < m_gridSize.x; x++)
            {
                const Vec3f cellMin = localBounds.min + Vec3f(float(x) * cellSize.x, float(y) * cellSize.y, float(z) * cellSize.z);

                BoundingBox probeLocalBounds;
                probeLocalBounds.min = -cellSize * 0.5f;
                probeLocalBounds.max = cellSize * 0.5f;

                const float jitterAmount = cellSize.x * 0.025f;
                const Vec3f jitter = Vec3f(
                    MathUtil::RandomInRange(seed, -jitterAmount, jitterAmount),
                    MathUtil::RandomInRange(seed, -jitterAmount, jitterAmount),
                    MathUtil::RandomInRange(seed, -jitterAmount, jitterAmount));

                Handle<IrradianceProbe> probe = MakeHandle<IrradianceProbe>(probeLocalBounds, Vec2u { 8, 8 });
                probe->SetLocalTranslation(cellMin + (cellSize * 0.5f) + jitter);
                probe->useVolumeEditTool = false;

                AddChild(probe);

                m_probes.PushBack(probe.Get());
            }
        }
    }

    BakeTetrahedra();

    for (IrradianceProbe* probe : m_probes)
    {
        RefreshProbe(*probe);
    }
}

void ProbeVolume::TessellateGrid()
{
    if (m_probes.Size() < 4)
    {
        return;
    }

    m_gridCount = m_gridSize;

    const uint32 countX = m_gridSize.x;
    const uint32 countY = m_gridSize.y;
    const uint32 countZ = m_gridSize.z;

    const uint32 numCellsX = countX - 1;
    const uint32 numCellsY = countY - 1;
    const uint32 numCellsZ = countZ - 1;

    m_tetrahedra.Reserve(numCellsX * numCellsY * numCellsZ * 6);

    // Probe index lookup: index(x, y, z) = x * countY * countZ + y * countZ + z
    auto probeIndex = [countY, countZ](uint32 x, uint32 y, uint32 z) -> uint32
    {
        return x * countY * countZ + y * countZ + z;
    };

    // Helper to push a tet with the 4 probe indices. Neighbours and
    // invEdgeMatrix are filled in by RebuildRuntimeData() after.
    auto pushTet = [this](uint32 i0, uint32 i1, uint32 i2, uint32 i3)
    {
        Tetrahedron tet;
        tet.probeIndices[0] = i0;
        tet.probeIndices[1] = i1;
        tet.probeIndices[2] = i2;
        tet.probeIndices[3] = i3;
        tet.neighbours[0] = -1;
        tet.neighbours[1] = -1;
        tet.neighbours[2] = -1;
        tet.neighbours[3] = -1;
        tet.invEdgeMatrix = Mat3f::Identity();
        m_tetrahedra.PushBack(tet);
    };

    for (uint32 x = 0; x < numCellsX; x++)
    {
        for (uint32 y = 0; y < numCellsY; y++)
        {
            for (uint32 z = 0; z < numCellsZ; z++)
            {
                const uint32 x0 = x, x1 = x + 1;
                const uint32 y0 = y, y1 = y + 1;
                const uint32 z0 = z, z1 = z + 1;

                const uint32 p000 = probeIndex(x0, y0, z0);
                const uint32 p001 = probeIndex(x0, y0, z1);
                const uint32 p010 = probeIndex(x0, y1, z0);
                const uint32 p011 = probeIndex(x0, y1, z1);
                const uint32 p100 = probeIndex(x1, y0, z0);
                const uint32 p101 = probeIndex(x1, y0, z1);
                const uint32 p110 = probeIndex(x1, y1, z0);
                const uint32 p111 = probeIndex(x1, y1, z1);

                // Decompose the cube into 6 tetrahedra.
                // Reference: http://www.iue.tuwien.ac.at/phd/wessner/node32.html

                // Prism 1
                pushTet(p111, p110, p100, p000);
                pushTet(p111, p011, p110, p000);
                pushTet(p110, p011, p010, p000);

                // Prism 2
                pushTet(p111, p100, p101, p000);
                pushTet(p111, p101, p001, p000);
                pushTet(p111, p001, p011, p000);
            }
        }
    }
}

void ProbeVolume::BakeTetrahedra()
{
    m_tetrahedra.Clear();

    {
        TUniqueLock lock(m_cachedState.mutex);

        m_cachedState.entityCellCache.Clear();
        m_cachedState.lastTetHint = 0;
    }

    TessellateGrid();

    RebuildRuntimeData();
}

#endif // HYP_EDITOR

#pragma endregion ProbeVolume

} // namespace Hyperion
