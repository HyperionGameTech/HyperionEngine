/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <baking/LightmapTexel.hpp>

#include <Core/Types.hpp>

#include <Core/math/Ray.hpp>
#include <Core/math/Mat4f.hpp>
#include <Core/math/BoundingBox.hpp>
#include <Core/math/Transform.hpp>
#include <Core/math/Triangle.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/containers/FlatSet.hpp>
#include <Core/containers/Array.hpp>

#include <rendering/Vertex.hpp>

#include <scene/BVH.hpp>

namespace Hyperion {

class Entity;

namespace Baking {

struct BakeEntity;

struct LightmapRayHit : RayHit
{
    Handle<Entity> entity;
    Triangle triangle;

    LightmapRayHit() = default;

    LightmapRayHit(const RayHit& rayHit, const Handle<Entity>& entity, const Triangle& triangle)
        : RayHit(rayHit),
          entity(entity),
          triangle(triangle)
    {
    }

    LightmapRayHit(const LightmapRayHit& other) = default;
    LightmapRayHit& operator=(const LightmapRayHit& other) = default;

    LightmapRayHit(LightmapRayHit&& other) noexcept
        : RayHit(static_cast<RayHit&&>(std::move(other))),
          entity(std::move(other.entity)),    // NOLINT(bugprone-use-after-move)
          triangle(std::move(other.triangle)) // NOLINT(bugprone-use-after-move)
    {
    }

    LightmapRayHit& operator=(LightmapRayHit&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        RayHit::operator=(static_cast<RayHit&&>(std::move(other)));
        entity = std::move(other.entity);     // NOLINT(bugprone-use-after-move)
        triangle = std::move(other.triangle); // NOLINT(bugprone-use-after-move)

        return *this;
    }

    virtual ~LightmapRayHit() = default;

    bool operator==(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) == static_cast<const RayHit&>(other)
            && entity == other.entity
            && triangle == other.triangle;
    }

    bool operator!=(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) != static_cast<const RayHit&>(other)
            || entity != other.entity
            || triangle != other.triangle;
    }

    bool operator<(const LightmapRayHit& other) const
    {
        if (static_cast<const RayHit&>(*this) < static_cast<const RayHit&>(other))
        {
            return true;
        }

        if (entity < other.entity)
        {
            return true;
        }

        if (entity == other.entity && triangle.GetPosition() < other.triangle.GetPosition())
        {
            return true;
        }

        return false;
    }
};

using LightmapRayTestResults = FlatSet<LightmapRayHit>;

class LightmapBottomLevelAccelerationStructure
{
public:
    LightmapBottomLevelAccelerationStructure(
        const BakeEntity* bakeEntity,
        BVHNode&& bvh,
        Array<Vertex>&& vertices,
        Array<uint32>&& indices)
        : m_bakeEntity(bakeEntity),
          m_root(std::move(bvh)),
          m_cachedVertices(std::move(vertices)),
          m_cachedIndices(std::move(indices))
    {
        Assert(m_bakeEntity != nullptr);
    }

    LightmapBottomLevelAccelerationStructure(const LightmapBottomLevelAccelerationStructure& other) = delete;
    LightmapBottomLevelAccelerationStructure& operator=(const LightmapBottomLevelAccelerationStructure& other) = delete;

    LightmapBottomLevelAccelerationStructure(LightmapBottomLevelAccelerationStructure&& other) noexcept
        : m_bakeEntity(other.m_bakeEntity),
          m_root(std::move(other.m_root)),
          m_cachedVertices(std::move(other.m_cachedVertices)),
          m_cachedIndices(std::move(other.m_cachedIndices))
    {
        other.m_bakeEntity = nullptr;
    }

    LightmapBottomLevelAccelerationStructure& operator=(LightmapBottomLevelAccelerationStructure&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_bakeEntity = other.m_bakeEntity;
        m_root = std::move(other.m_root);
        m_cachedVertices = std::move(other.m_cachedVertices);
        m_cachedIndices = std::move(other.m_cachedIndices);

        other.m_bakeEntity = nullptr;

        return *this;
    }

    ~LightmapBottomLevelAccelerationStructure() = default;

    HYP_FORCE_INLINE const Handle<Entity>& GetEntity() const
    {
        return m_bakeEntity->entity;
    }

    HYP_FORCE_INLINE const Mat4f& GetTransformMatrix() const
    {
        return m_bakeEntity->transformMatrix;
    }

    LightmapRayTestResults TestRay(const Ray& ray) const
    {
        LightmapRayTestResults results;

        const Mat4f modelMatrix = m_bakeEntity->transformMatrix;

        const Ray localSpaceRay = modelMatrix.Inverse() * ray;

        RayTestResults localBvhResults = m_root.TestRay(localSpaceRay, m_cachedVertices, m_cachedIndices);

        if (localBvhResults.Any())
        {
            const Mat3f normalMatrix = Mat3f(modelMatrix).Inverse().Transpose();

            RayTestResults bvhResults;

            for (RayHit hit : localBvhResults)
            {
                Vec3f transformedNormal = (normalMatrix * hit.normal).Normalized();
                hit.normal = transformedNormal;

                Vec4f transformedPosition = modelMatrix * Vec4f(hit.hitpoint, 1.0f);
                transformedPosition /= transformedPosition.w;

                hit.hitpoint = transformedPosition.GetXYZ();

                hit.distance = (hit.hitpoint - ray.position).Length();

                bvhResults.AddHit(hit);
            }

            for (const RayHit& rayHit : bvhResults)
            {
                Assert(rayHit.userData != nullptr);

                const BVHNode* bvhNode = static_cast<const BVHNode*>(rayHit.userData);

                const uint32 triangleId = rayHit.id;

                AssertDebug(triangleId < m_cachedIndices.Size() / 3);

                const uint32 i0 = m_cachedIndices[triangleId * 3 + 0];
                const uint32 i1 = m_cachedIndices[triangleId * 3 + 1];
                const uint32 i2 = m_cachedIndices[triangleId * 3 + 2];

                const Triangle triangle {
                    m_cachedVertices[i0].position,
                    m_cachedVertices[i1].position,
                    m_cachedVertices[i2].position
                };

                results.Emplace(rayHit, m_bakeEntity->entity, triangle);
            }
        }

        return results;
    }

    HYP_FORCE_INLINE const BVHNode& GetRoot() const
    {
        return m_root;
    }

private:
    const BakeEntity* m_bakeEntity;

    BVHNode m_root;
    Array<Vertex> m_cachedVertices;
    Array<uint32> m_cachedIndices;
};

class LightmapTopLevelAccelerationStructure
{
public:
    ~LightmapTopLevelAccelerationStructure() = default;

    LightmapRayTestResults TestRay(const Ray& ray) const
    {
        LightmapRayTestResults results;

        for (const LightmapBottomLevelAccelerationStructure& accelerationStructure : m_accelerationStructures)
        {
            if (!ray.TestAABB(accelerationStructure.GetTransformMatrix() * accelerationStructure.GetRoot().aabb))
            {
                continue;
            }

            results.Merge(accelerationStructure.TestRay(ray));
        }

        return results;
    }

    void Add(
        const BakeEntity* bakeEntity,
        BVHNode&& bvh,
        Array<Vertex>&& vertices,
        Array<uint32>&& indices)
    {
        m_accelerationStructures.EmplaceBack(bakeEntity, std::move(bvh), std::move(vertices), std::move(indices));
    }

    void RemoveAll()
    {
        m_accelerationStructures.Clear();
    }

private:
    Array<LightmapBottomLevelAccelerationStructure> m_accelerationStructures;
};

} // namespace Baking

} // namespace Hyperion
