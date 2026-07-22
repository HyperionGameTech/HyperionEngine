/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Util/VoxelOctree.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/BVH.hpp>

#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/RenderableAttributes.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>

#include <limits>

namespace Hyperion {

VoxelOctreeBuildResult VoxelOctree::Build(const VoxelOctreeParams& params, EntityManager& entityManager)
{
    m_params = params;

    OctreeBase::m_aabb = params.aabb;

    if (!params.allowResize && (!OctreeBase::m_aabb.IsValid() || !OctreeBase::m_aabb.IsFinite() || OctreeBase::m_aabb.IsZero()))
    {
        return HYP_MAKE_ERROR(Error, "Voxel octree is not allowed to resize and has an invalid AABB");
    }

    OctreeBase::Clear();

    BoundingBox newAabb = OctreeBase::m_aabb;

    Array<Tuple<VoxelOctreeElement, MeshDesc, VertexArrayView, Span<const ubyte>, UniquePtr<TSharedResLock<AssetObject>>>> meshDatas;

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent] : entityManager.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (!meshComponent.mesh.IsValid())
        {
            continue;
        }

        if (!meshComponent.material.IsValid())
        {
            continue;
        }

        if (meshComponent.material->GetBucket() != RenderBucket::Opaque
            && meshComponent.material->GetBucket() != RenderBucket::Lightmapped
            && meshComponent.material->GetBucket() != RenderBucket::Translucent)
        {
            continue;
        }

        if (params.allowResize)
        {
            newAabb = newAabb.Union(boundingBoxComponent.worldAabb);
        }
        else
        {
            if (!OctreeBase::m_aabb.Overlaps(boundingBoxComponent.worldAabb))
            {
                // Skip meshes that are out of bounds
                continue;
            }
        }

        VoxelOctreeElement element {};
        element.entity = MakeStrongRef(entity);
        element.mesh = meshComponent.mesh;
        element.material = meshComponent.material;
        element.transformMatrix = entity->GetWorldMatrix();
        element.aabb = boundingBoxComponent.worldAabb;

        auto lock = MakeUnique<TSharedResLock<AssetObject>>(*meshComponent.mesh);

        meshDatas.EmplaceBack(
            element,
            meshComponent.mesh->GetMeshDesc(),
            meshComponent.mesh->GetVertexData(0),
            meshComponent.mesh->GetIndexData(0),
            std::move(lock));
    }

    if (!newAabb.IsValid() || !newAabb.IsFinite())
    {
        return HYP_MAKE_ERROR(Error, "Invalid AABB, cannot build voxel octree");
    }

    if (params.allowResize)
    {
        Vec3f extent = newAabb.GetExtent();

        const Vec3f center = newAabb.GetCenter();
        float maxExtent = extent.Max();

        newAabb.SetExtent(Vec3f(maxExtent));
        newAabb.SetCenter(center);

        OctreeBase::m_aabb = newAabb;
    }

    InitOctants();

    auto insertIntoOctree = [&](const VoxelOctreeElement& element, const MeshDesc& meshDesc, const VertexArrayView& vertexData, Span<const ubyte> indexData) -> bool
    {
        if (meshDesc.lods[0].numIndices > 0)
        {
            const Mat4f& transformMatrix = element.transformMatrix;

            // @TODO fix for non-uint32 sized indices

            Span<const uint32> meshIndices = Span<const uint32>(
                reinterpret_cast<const uint32*>(indexData.Data()),
                reinterpret_cast<const uint32*>(indexData.Data()) + (indexData.Size() / sizeof(uint32)));

            Assert(meshIndices.Size() % 3 == 0);

            const size_t vertexSizeInFloats = vertexData.layoutDesc.VertexSize() / sizeof(float);

            for (size_t i = 0; i < meshIndices.Size(); i += 3)
            {
                const float* floatDataOffset0 = vertexData.floatData + (meshIndices[i + 0] * vertexSizeInFloats);
                const float* floatDataOffset1 = vertexData.floatData + (meshIndices[i + 1] * vertexSizeInFloats);
                const float* floatDataOffset2 = vertexData.floatData + (meshIndices[i + 2] * vertexSizeInFloats);

                const TVertexPacket<VT_Position>* packet0 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset0);
                const TVertexPacket<VT_Position>* packet1 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset1);
                const TVertexPacket<VT_Position>* packet2 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset2);

                const Vec3f positions[3] = {
                    transformMatrix.TransformVector(packet0->GetPosition()),
                    transformMatrix.TransformVector(packet1->GetPosition()),
                    transformMatrix.TransformVector(packet2->GetPosition())
                };

                const Triangle triangle {
                    {
                        positions[0].x, positions[0].y, positions[0].z,
                        positions[1].x, positions[1].y, positions[1].z,
                        positions[2].x, positions[2].y, positions[2].z
                    }
                };

                BoundingBox triangleBounds = triangle.GetBoundingBox().Expand(0.002f);
                AssertDebug(triangleBounds.IsValid() && triangleBounds.IsFinite());

                if (!triangleBounds.IsValid() || !triangleBounds.IsFinite())
                {
                    continue;
                }

                (void)OctreeBase::Insert(VoxelOctreePayload { 1 }, triangleBounds);
            }
        }

        return true;
    };

    for (const auto& tup : meshDatas)
    {
        const VoxelOctreeElement& element = tup.GetElement<0>();
        const MeshDesc& meshDesc = tup.GetElement<1>();
        const VertexArrayView& vertexData = tup.GetElement<2>();
        const Span<const ubyte> indexData = tup.GetElement<3>();

        insertIntoOctree(element, meshDesc, vertexData, indexData);
    }

    return {};
}

double VoxelOctree::GetSignedDistanceAtPoint(const Vec3f& point) const
{
    double minDistanceSq = INFINITY;
    bool isInsideAnyVoxel = false;

    struct NodeEntry
    {
        double distSq;
        const VoxelOctree* node;
    };

    auto DistSqPointAABB = [](const BoundingBox& aabb, const Vec3f& p) -> double
    {
        const Vec3f min = aabb.GetMin();
        const Vec3f max = aabb.GetMax();
        double distSq = 0.0;

        auto CheckAxis = [&](double pVal, double minVal, double maxVal)
        {
            if (pVal < minVal)
                distSq += (minVal - pVal) * (minVal - pVal);
            else if (pVal > maxVal)
                distSq += (pVal - maxVal) * (pVal - maxVal);
        };

        CheckAxis(p.x, min.x, max.x);
        CheckAxis(p.y, min.y, max.y);
        CheckAxis(p.z, min.z, max.z);

        return distSq;
    };

    Array<NodeEntry> stack;
    stack.Reserve(64);

    double rootDistSq = DistSqPointAABB(this->GetAABB(), point);
    stack.PushBack({ rootDistSq, this });

    while (!stack.Empty())
    {
        NodeEntry entry = stack.Back();
        stack.PopBack();

        double nodeDistSq = entry.distSq;

        if (nodeDistSq >= minDistanceSq)
        {
            continue;
        }

        const VoxelOctree* node = entry.node;
        bool isLeaf = !node->IsDivided();

        if (isLeaf && node->GetPayload().occupiedBit)
        {
            if (nodeDistSq < minDistanceSq)
            {
                minDistanceSq = nodeDistSq;
            }

            if (node->GetAABB().ContainsPoint(point))
            {
                isInsideAnyVoxel = true;
                break;
            }
        }

        if (!isLeaf)
        {
            NodeEntry children[8];
            int childCount = 0;

            for (uint8 i = 0; i < 8; i++)
            {
                const VoxelOctree* childNode = node->m_octants[i].octree;
                if (childNode != nullptr)
                {
                    double childDist = DistSqPointAABB(childNode->GetAABB(), point);

                    if (childDist < minDistanceSq)
                    {
                        children[childCount++] = { childDist, childNode };
                    }
                }
            }

            std::sort(children, children + childCount,
                [](const NodeEntry& a, const NodeEntry& b)
                {
                    return a.distSq > b.distSq;
                });

            for (int i = 0; i < childCount; i++)
            {
                stack.PushBack(children[i]);
            }
        }
    }

    if (isInsideAnyVoxel)
    {
        return -1.0;
    }

    if (minDistanceSq != INFINITY)
    {
        return std::sqrt(minDistanceSq);
    }

    return INFINITY;
}

bool VoxelOctree::RayCastOccluded(const Vec3f& start, const Vec3f& direction, float maxDistance) const
{
    float t = 0.0f;
    
    constexpr uint32 MaxRaySteps = 1000;

    for (uint32 i = 0; i < MaxRaySteps && t < maxDistance; i++)
    {
        const Vec3f pos = start + direction * t;
        const double sdf = GetSignedDistanceAtPoint(pos);

        if (sdf < -0.001)
        {
            return true;
        }

        const float step = MathUtil::Max(float(sdf), 0.01f);
        t += step;
    }

    return false;
}

} // namespace Hyperion
