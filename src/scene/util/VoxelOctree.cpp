/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/util/VoxelOctree.hpp>
#include <scene/Entity.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <scene/BVH.hpp>

#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

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

    if (params.voxelSize < 0.001)
    {
        return HYP_MAKE_ERROR(Error, "Voxel size must be greater than 0.001");
    }

    OctreeBase::Clear();

    BoundingBox newAabb = BoundingBox::Empty();

    Array<Tuple<VoxelOctreeElement, MeshDesc, MeshData*, ResourceHandle>> meshDatas;

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

        newAabb = newAabb.Union(boundingBoxComponent.worldAabb);

        VoxelOctreeElement element {};
        element.entity = MakeStrongRef(entity);
        element.mesh = meshComponent.mesh;
        element.material = meshComponent.material;
        element.transform = transformComponent.transform;
        element.aabb = boundingBoxComponent.worldAabb;

        if (!meshComponent.mesh->GetAsset())
        {
            continue;
        }

        ResourceHandle resourceHandle(*meshComponent.mesh->GetAsset()->GetResource());
        meshDatas.EmplaceBack(
            element,
            meshComponent.mesh->GetAsset()->GetMeshDesc(),
            meshComponent.mesh->GetAsset()->GetMeshData(),
            resourceHandle);
    }

    if (!newAabb.IsValid() || !newAabb.IsFinite())
    {
        return HYP_MAKE_ERROR(Error, "Invalid AABB, cannot build voxel octree");
    }

    Vec3f extent = newAabb.GetExtent();

    const Vec3f center = newAabb.GetCenter();
    float maxExtent = extent.Max();

    newAabb.SetExtent(Vec3f(maxExtent));
    newAabb.SetCenter(center);

    m_aabb = newAabb;
    InitOctants();

    Proc<bool(const VoxelOctreeElement&, const MeshDesc&, const MeshData&)> insertIntoOctree;

    insertIntoOctree = [&](const VoxelOctreeElement& element, const MeshDesc& meshDesc, const MeshData& meshData) -> bool
    {
        if (meshDesc.numIndices > 0)
        {
            Span<const uint32> meshIndices = Span<const uint32>(
                reinterpret_cast<const uint32*>(meshData.indexData.Data()),
                reinterpret_cast<const uint32*>(meshData.indexData.Data()) + (meshData.indexData.Size() / sizeof(uint32)));

            Assert(meshIndices.Size() % 3 == 0);

            for (SizeType i = 0; i < meshIndices.Size(); i += 3)
            {
                Triangle triangle {
                    Vertex(element.transform.GetMatrix() * meshData.vertexData[meshIndices[i + 0]].position),
                    Vertex(element.transform.GetMatrix() * meshData.vertexData[meshIndices[i + 1]].position),
                    Vertex(element.transform.GetMatrix() * meshData.vertexData[meshIndices[i + 2]].position)
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
        const MeshData* meshData = tup.GetElement<2>();

        Assert(meshData != nullptr);

        insertIntoOctree(element, meshDesc, *meshData);
    }

    return {};
}

} // namespace hyperion
