/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 * */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/BoundingSphere.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Asset/AssetObject.hpp>

#include <Physics/PhysicsMemory.hpp>

namespace Hyperion {

HYP_ENUM()
enum class PhysicsShapeType : uint8
{
    Box,
    Sphere,
    Plane,
    ConvexHull,
    Capsule,

    Max
};

HYP_CLASS(Abstract, AssetBucket = "PhysicsShapes")
class PhysicsShape : public AssetObject
{
    HYP_OBJECT_BODY(PhysicsShape);

protected:
    PhysicsShape() = default;
    PhysicsShape(Name name, PhysicsShapeType type)
        : AssetObject(name),
          m_type(type)
    {
    }

public:
    static Pool* GetAllocator() { return g_physicsPool; }

    ~PhysicsShape() override = default;

    HYP_METHOD()
    PhysicsShapeType GetType() const
    {
        return m_type;
    }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_FORCE_INLINE void* GetInternalData() const
    {
        return m_internalData.Get();
    }

    /*! \brief Set the internal handle of the PhysicsShape. Only to be used
        by a PhysicsAdapter. */
    HYP_FORCE_INLINE void SetInternalData(SharedPtr<void>&& internalData)
    {
        m_internalData = std::move(internalData);
    }

    HYP_FORCE_INLINE void Invalidate()
    {
        m_internalData.Reset();
    }

protected:
    const PhysicsShapeType m_type;

    SharedPtr<void> m_internalData;
};

HYP_CLASS()
class BoxPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(BoxPhysicsShape);

public:
    BoxPhysicsShape()
        : PhysicsShape(Name::Invalid(), PhysicsShapeType::Box),
          m_aabb(Vec3f(-1.0f), Vec3f(1.0f))
    {
    }

    BoxPhysicsShape(Name name, const BoundingBox& aabb)
        : PhysicsShape(name, PhysicsShapeType::Box),
          m_aabb(aabb)
    {
    }

    ~BoxPhysicsShape() override = default;

    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    /*! \brief Set the axis-aligned bounds of the box, in the shape's local space. Used by the
     *  editor's "fit to mesh" action and the box reshape gizmo. */
    HYP_FORCE_INLINE void SetAABB(const BoundingBox& aabb)
    {
        if (m_aabb == aabb)
        {
            return;
        }

        m_aabb = aabb;

        MarkDirty();
    }

protected:
    HYP_FIELD(Property = "Bounds", Serialize)
    BoundingBox m_aabb;
};

HYP_CLASS()
class SpherePhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(SpherePhysicsShape);

public:
    SpherePhysicsShape()
        : PhysicsShape(Name::Invalid(), PhysicsShapeType::Sphere),
          m_sphere(Vec3f::Zero(), 1.0f)
    {
    }

    SpherePhysicsShape(Name name, const BoundingSphere& sphere)
        : PhysicsShape(name, PhysicsShapeType::Sphere),
          m_sphere(sphere)
    {
    }

    ~SpherePhysicsShape() override = default;

    HYP_FORCE_INLINE const BoundingSphere& GetSphere() const
    {
        return m_sphere;
    }

    HYP_FORCE_INLINE void SetSphere(const BoundingSphere& sphere)
    {
        if (m_sphere == sphere)
        {
            return;
        }

        m_sphere = sphere;
        
        MarkDirty();
    }

protected:
    HYP_FIELD(Property = "Bounds", Serialize)
    BoundingSphere m_sphere;
};

HYP_CLASS()
class PlanePhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(PlanePhysicsShape);

public:
    PlanePhysicsShape()
        : PhysicsShape(Name::Invalid(), PhysicsShapeType::Plane),
          m_plane(0.0f, 1.0f, 0.0f, 0.0f)
    {
    }

    PlanePhysicsShape(Name name, const Vec4f& plane)
        : PhysicsShape(name, PhysicsShapeType::Plane),
          m_plane(plane)
    {
    }

    ~PlanePhysicsShape() override = default;

    HYP_FORCE_INLINE const Vec4f& GetPlane() const
    {
        return m_plane;
    }

    HYP_FORCE_INLINE void SetPlane(const Vec4f& plane)
    {
        if (m_plane == plane)
        {
            return;
        }

        m_plane = plane;
        
        MarkDirty();
    }

protected:
    HYP_FIELD(Property = "Plane", Serialize)
    Vec4f m_plane;
};

HYP_CLASS()
class ConvexHullPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(ConvexHullPhysicsShape);

public:
    ConvexHullPhysicsShape()
        : PhysicsShape(Name::Invalid(), PhysicsShapeType::ConvexHull)
    {
    }

    ConvexHullPhysicsShape(Name name, const struct VertexArrayView& vertexData);

    ConvexHullPhysicsShape(const ConvexHullPhysicsShape&) = delete;
    ConvexHullPhysicsShape& operator=(const ConvexHullPhysicsShape&) = delete;

    ConvexHullPhysicsShape(ConvexHullPhysicsShape&&) noexcept = delete;
    ConvexHullPhysicsShape& operator=(ConvexHullPhysicsShape&&) noexcept = delete;

    ~ConvexHullPhysicsShape() override;

    HYP_FORCE_INLINE const float* GetVertexData() const
    {
        return reinterpret_cast<const float*>(m_vertexData.raw);
    }

    HYP_FORCE_INLINE size_t NumVertices() const
    {
        return m_vertexData.size / (sizeof(float) * 3);
    }

    void SetVertexData(const struct VertexArrayView& vertexData);

protected:
    HYP_FIELD(Property = "VertexData", Serialize)
    BlobDataReference m_vertexData;
};

HYP_CLASS()
class CapsulePhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(CapsulePhysicsShape);

public:
    CapsulePhysicsShape()
        : PhysicsShape(Name::Invalid(), PhysicsShapeType::Capsule),
          m_radius(0.2f),
          m_height(1.7f) // Avg. height of a human :)
    {
    }

    CapsulePhysicsShape(Name name, float radius, float height)
        : PhysicsShape(name, PhysicsShapeType::Capsule),
          m_radius(radius),
          m_height(height)
    {
    }

    ~CapsulePhysicsShape() override = default;

    HYP_FORCE_INLINE float GetRadius() const
    {
        return m_radius;
    }

    HYP_FORCE_INLINE void SetRadius(float radius)
    {
        if (m_radius == radius)
        {
            return;
        }

        m_radius = radius;
        
        MarkDirty();
    }

    HYP_FORCE_INLINE float GetHeight() const
    {
        return m_height;
    }

    HYP_FORCE_INLINE void SetHeight(float height)
    {
        if (m_height == height)
        {
            return;
        }

        m_height = height;
        
        MarkDirty();
    }

protected:
    HYP_FIELD(Property = "Radius", Serialize)
    float m_radius;

    HYP_FIELD(Property = "Height", Serialize)
    float m_height;
};

} // namespace Hyperion
