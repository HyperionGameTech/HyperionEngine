/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/util/VoxelOctree.hpp>

#include <scene/Entity.hpp>
#include <scene/EntityManager.hpp>
#include <scene/BVH.hpp>

#include <scene/components/TransformComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <limits>

namespace Hyperion {

static BoundingBox SnapAabbToVoxel(const BoundingBox& aabb, float voxelSize)
{

    Vec3f extent = aabb.GetExtent();
    Vec3f newExtent = Vec3f(
        MathUtil::Ceil(extent.x / voxelSize) * voxelSize,
        MathUtil::Ceil(extent.y / voxelSize) * voxelSize,
        MathUtil::Ceil(extent.z / voxelSize) * voxelSize);

    BoundingBox newAabb = aabb;
    newAabb.SetExtent(newExtent);

    return newAabb;
}

class VoxelOctreeBlas
{
public:
    VoxelOctreeBlas(const VoxelOctreeElement& element, const BVHNode* bvh)
        : m_element(element),
          m_root(bvh)
    {
        Assert(m_root != nullptr);
    }

    VoxelOctreeBlas(const VoxelOctreeBlas& other) = delete;
    VoxelOctreeBlas& operator=(const VoxelOctreeBlas& other) = delete;

    VoxelOctreeBlas(VoxelOctreeBlas&& other) noexcept
        : m_element(other.m_element),
          m_root(other.m_root)
    {
        other.m_root = nullptr;
    }

    VoxelOctreeBlas& operator=(VoxelOctreeBlas&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_element = std::move(other.m_element);
        m_root = std::move(other.m_root);

        other.m_root = nullptr;

        return *this;
    }

    ~VoxelOctreeBlas() = default;

    HYP_FORCE_INLINE const VoxelOctreeElement& GetElement() const
    {
        return m_element;
    }

    HYP_FORCE_INLINE const BVHNode* GetRoot() const
    {
        return m_root;
    }

private:
    VoxelOctreeElement m_element;
    const BVHNode* m_root;
};

struct VoxelOctreeTlas
{
    HYP_FORCE_INLINE const Transform& GetTransform() const
    {
        return Transform::identity;
    }

    HYP_FORCE_INLINE const Array<VoxelOctreeBlas>& GetAccelerationStructures() const
    {
        return m_accelerationStructures;
    }

    void Add(const VoxelOctreeElement& element, const BVHNode* bvh)
    {
        m_accelerationStructures.EmplaceBack(element, bvh);
    }

    void RemoveAll()
    {
        m_accelerationStructures.Clear();
    }

private:
    Array<VoxelOctreeBlas> m_accelerationStructures;
};

VoxelOctreeBuildResult VoxelOctree::Build(const VoxelOctreeParams& params, EntityManager* entityManager)
{
    Assert(entityManager != nullptr);

    m_aabb = params.aabb;

    if (!params.allowResize && (!m_aabb.IsValid() || !m_aabb.IsFinite() || m_aabb.IsZero()))
    {
        return HYP_MAKE_ERROR(Error, "Voxel octree is not allowed to resize and has an invalid AABB");
    }

    OctreeBase::Clear();

    BoundingBox newAabb = m_aabb;

    Array<Tuple<VoxelOctreeElement, MeshDesc, Span<const Vertex>, Span<const ubyte>, UniquePtr<TSharedLock<AssetObject>>>> meshDatas;

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent] : entityManager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (!meshComponent.mesh.IsValid())
        {
            continue;
        }

        if (!meshComponent.material.IsValid())
        {
            continue;
        }

        // Only process opaque and translucent materials
        if (meshComponent.material->GetBucket() != RB_OPAQUE && meshComponent.material->GetBucket() != RB_TRANSLUCENT)
        {
            continue;
        }

        if (!meshComponent.mesh->GetBVH().IsValid())
        {
            HYP_LOG_ONCE(Misc, Warning, "No valid BVH for mesh {} (ID: {}) on entity {}, skipping.", meshComponent.mesh->GetName(), meshComponent.mesh->Id(), entity->Id());

            continue;
        }

        if (params.allowResize)
        {
            newAabb = newAabb.Union(boundingBoxComponent.worldAabb);
        }
        else
        {
            if (!m_aabb.Overlaps(boundingBoxComponent.worldAabb))
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

        meshDatas.EmplaceBack(
            element,
            meshComponent.mesh->GetMeshDesc(),
            meshComponent.mesh->GetVertexData(),
            meshComponent.mesh->GetIndexData(),
            MakeUnique<TSharedLock<AssetObject>>(*meshComponent.mesh));
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

        m_aabb = newAabb;
    }

    InitOctants();

    Proc<bool(const VoxelOctreeElement&, const MeshDesc&, Span<const Vertex>, Span<const ubyte>)> InsertIntoOctree;

    InsertIntoOctree = [&](const VoxelOctreeElement& element, const MeshDesc& meshDesc, Span<const Vertex> vertexData, Span<const ubyte> indexData) -> bool
    {
        if (meshDesc.numIndices > 0)
        {
            const Mat4f& transformMatrix = element.transformMatrix;

            // @TODO fix for non-uint32 sized indices

            Span<const uint32> meshIndices = Span<const uint32>(
                reinterpret_cast<const uint32*>(indexData.Data()),
                reinterpret_cast<const uint32*>(indexData.Data()) + (indexData.Size() / sizeof(uint32)));

            Assert(meshIndices.Size() % 3 == 0);

            for (SizeType i = 0; i < meshIndices.Size(); i += 3)
            {
                const Vec3f positions[3] = {
                    transformMatrix * vertexData[meshIndices[i + 0]].position,
                    transformMatrix * vertexData[meshIndices[i + 1]].position,
                    transformMatrix * vertexData[meshIndices[i + 2]].position
                };

                const Triangle triangle {
                    {
                        positions[0].x, positions[0].y, positions[0].z,
                        positions[1].x, positions[1].y, positions[1].z,
                        positions[2].x, positions[2].y, positions[2].z
                    }
                };

                BoundingBox triangleAabb = triangle.GetBoundingBox().Expand(0.002f);

                (void)OctreeBase::Insert(VoxelOctreePayload { .occupiedBit = 1 }, triangleAabb);
            }
        }

        return true;
    };

    for (const auto& tup : meshDatas)
    {
        const VoxelOctreeElement& element = tup.GetElement<0>();
        const MeshDesc& meshDesc = tup.GetElement<1>();
        const Span<const Vertex> vertexData = tup.GetElement<2>();
        const Span<const ubyte> indexData = tup.GetElement<3>();

        InsertIntoOctree(element, meshDesc, vertexData, indexData);
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
        auto checkAxis = [&](double pVal, double minVal, double maxVal)
        {
            if (pVal < minVal)
                distSq += (minVal - pVal) * (minVal - pVal);
            else if (pVal > maxVal)
                distSq += (pVal - maxVal) * (pVal - maxVal);
        };
        checkAxis(p.x, min.x, max.x);
        checkAxis(p.y, min.y, max.y);
        checkAxis(p.z, min.z, max.z);
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

} // namespace Hyperion
