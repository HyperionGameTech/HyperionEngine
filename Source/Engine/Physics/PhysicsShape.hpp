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

#include <Core/Containers/Array.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/BoundingSphere.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Utilities/Span.hpp>

#include <Asset/AssetObject.hpp>

#include <Physics/PhysicsMemory.hpp>

namespace Hyperion {

class Mesh;

HYP_ENUM()
enum class PhysicsShapeType : uint8
{
    Box,
    Sphere,
    Plane,
    ConvexHull,
    Capsule,
    HeightField,
    Compound,

    Max
};

HYP_STRUCT()
struct ConvexDecompositionSettings
{
    HYP_STRUCT_BODY(ConvexDecompositionSettings);

    HYP_FIELD(Property = "MaxHulls", Serialize)
    uint32 maxHulls = 8;

    ///Voxel resolution the source mesh is decomposed at - higher resolves finer concavities.
    HYP_FIELD(Property = "Resolution", Serialize)
    uint32 resolution = 8000;

    HYP_FIELD(Property = "MaxVerticesPerHull", Serialize)
    uint32 maxVerticesPerHull = 64;

    HYP_FIELD(Property = "MaxRecursionDepth", Serialize)
    uint32 maxRecursionDepth = 10;

    HYP_FIELD(Property = "ShrinkWrap", Serialize)
    bool shrinkWrap = true;
};

HYP_STRUCT()
struct ConvexHullRange
{
    HYP_STRUCT_BODY(ConvexHullRange);

    HYP_FIELD(Serialize)
    uint32 firstVertex = 0;

    HYP_FIELD(Serialize)
    uint32 numVertices = 0;

    HYP_FIELD(Serialize)
    uint32 firstIndex = 0;

    HYP_FIELD(Serialize)
    uint32 numIndices = 0;
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

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        if (m_vertexData.size != 0)
        {
            outReferences.EmplaceBack("HULL", 1, &m_vertexData);
        }
    }

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

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

HYP_CLASS()
class HeightFieldPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(HeightFieldPhysicsShape);

public:
    HeightFieldPhysicsShape()
        : PhysicsShape(Name::Invalid(), PhysicsShapeType::HeightField),
          m_numSamples(0)
    {
    }

    HeightFieldPhysicsShape(Name name)
        : PhysicsShape(name, PhysicsShapeType::HeightField),
          m_numSamples(0)
    {
    }

    HeightFieldPhysicsShape(Name name, Span<const float> heights, uint32 numSamplesXZ);

    ~HeightFieldPhysicsShape() override = default;

    HYP_FORCE_INLINE const Array<float>& GetHeights() const
    {
        return m_heights;
    }

    HYP_FORCE_INLINE uint32 GetNumSamples() const
    {
        return m_numSamples;
    }

    void SetHeights(Span<const float> heights, uint32 numSamplesXZ);

protected:
    Array<float> m_heights;
    uint32 m_numSamples;
};

HYP_CLASS()
class CompoundPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(CompoundPhysicsShape);

public:
    CompoundPhysicsShape()
        : PhysicsShape(Name::Invalid(), PhysicsShapeType::Compound)
    {
    }

    CompoundPhysicsShape(Name name)
        : PhysicsShape(name, PhysicsShapeType::Compound)
    {
    }

    CompoundPhysicsShape(const CompoundPhysicsShape&) = delete;
    CompoundPhysicsShape& operator=(const CompoundPhysicsShape&) = delete;

    CompoundPhysicsShape(CompoundPhysicsShape&&) noexcept = delete;
    CompoundPhysicsShape& operator=(CompoundPhysicsShape&&) noexcept = delete;

    ~CompoundPhysicsShape() override;

    HYP_METHOD(Property = "NumHulls", EditEnabled = false, Transient)
    uint32 NumHulls() const
    {
        return uint32(m_hulls.Size());
    }

    HYP_FORCE_INLINE const Array<ConvexHullRange>& GetHulls() const
    {
        return m_hulls;
    }

    Span<const float> GetHullVertices(uint32 hullIndex) const;

    Span<const uint32> GetHullIndices(uint32 hullIndex) const;

    void SetHulls(Span<const float> positions, Span<const uint32> indices, Span<const ConvexHullRange> hulls);

    HYP_FORCE_INLINE const ConvexDecompositionSettings& GetDecompositionSettings() const
    {
        return m_decompositionSettings;
    }

    void SetDecompositionSettings(const ConvexDecompositionSettings& settings);

    HYP_FORCE_INLINE const Handle<Mesh>& GetSourceMesh() const
    {
        return m_sourceMesh;
    }

    void SetSource(const Handle<Mesh>& sourceMesh, uint64 sourceDataHash);

    HYP_METHOD(Property = "CollisionOutOfDate", Editor = false, Transient)
    bool IsOutOfDate() const;

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Regenerate", EditCondition = "CanRegenerate")
    void Regenerate();

    HYP_METHOD()
    bool CanRegenerate() const;
#endif // HYP_EDITOR

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        if (m_vertexData.size != 0)
        {
            outReferences.EmplaceBack("CHV", 1, &m_vertexData);
        }

        if (m_indexData.size != 0)
        {
            outReferences.EmplaceBack("CHI", 1, &m_indexData);
        }
    }

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

    HYP_FIELD(Property = "Hulls", Serialize, EditEnabled = false)
    Array<ConvexHullRange> m_hulls;

    HYP_FIELD(Property = "VertexData", Serialize, Editor = false)
    BlobDataReference m_vertexData;

    HYP_FIELD(Property = "IndexData", Serialize, Editor = false)
    BlobDataReference m_indexData;

    HYP_FIELD(Property = "DecompositionSettings", Serialize)
    ConvexDecompositionSettings m_decompositionSettings;

    HYP_FIELD(Property = "SourceMesh", Serialize, EditEnabled = false)
    Handle<Mesh> m_sourceMesh;

    HYP_FIELD(Property = "SourceDataHash", Serialize, Editor = false)
    uint64 m_sourceDataHash = 0;
};

} // namespace Hyperion
