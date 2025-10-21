/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Triangle.hpp>

#include <core/utilities/Result.hpp>

// #include <core/logging/LoggerFwd.hpp>

#include <util/octree/Octree.hpp>

namespace hyperion {

class Entity;
class Mesh;
class Material;
class EntityManager;

struct VoxelOctreeParams
{
    float voxelSize = 0.025f;
};

struct VoxelOctreeElement
{
    Handle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<Material> material;
    Transform transform;
    BoundingBox aabb;

    HYP_FORCE_INLINE bool operator==(const VoxelOctreeElement& other) const
    {
        return entity == other.entity
            && mesh == other.mesh
            && material == other.material
            && transform == other.transform
            && aabb == other.aabb;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(entity.GetHashCode());
        hc.Add(mesh.GetHashCode());
        hc.Add(material.GetHashCode());
        hc.Add(transform.GetHashCode());
        hc.Add(aabb);

        return hc;
    }
};

struct VoxelOctreeNode
{
    ObjId<Entity> entityId;
    ObjId<Mesh> meshId;
    Triangle triangle;

    HYP_FORCE_INLINE bool operator==(const VoxelOctreeNode& other) const
    {
        return entityId == other.entityId
            && meshId == other.meshId
            && triangle == other.triangle;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(entityId);
        hc.Add(meshId);
        hc.Add(triangle);

        return hc;
    }
};

using VoxelOctreeBuildResult = Result;

struct VoxelOctreePayload
{
    uint8 occupiedBit : 1 = 0;

    HYP_FORCE_INLINE bool Empty() const
    {
        return occupiedBit == 0;
    }
};

class HYP_API VoxelOctree : public OctreeBase<VoxelOctree, VoxelOctreePayload>
{
    friend class OctreeBase<VoxelOctree, VoxelOctreePayload>;

    VoxelOctree(VoxelOctree* parent, const BoundingBox& aabb, uint8 index = 0)
        : OctreeBase(parent, aabb, index)
    {
    }

public:
    static constexpr uint8 MaxDepth = 8;
    static constexpr EnumFlags<OctreeFlags> Flags = OctreeFlags::OF_INSERT_ON_OVERLAP;

    VoxelOctree()
    {
    }

    ~VoxelOctree() = default;

    VoxelOctreeBuildResult Build(const VoxelOctreeParams& params, EntityManager* entityManager);

protected:
    static VoxelOctree* CreateChildOctant(VoxelOctree* parent, const BoundingBox& aabb, uint8 index)
    {
        return new VoxelOctree(parent, aabb, index);
    }
};

} // namespace hyperion
