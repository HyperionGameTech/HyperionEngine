/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Handle.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <rendering/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Material;

enum AccelerationStructureType : uint8
{
    AST_BOTTOM_LEVEL = 0,
    AST_TOP_LEVEL
};

enum RaytracingUpdateFlags : uint32
{
    RUF_NONE = 0x0,
    RUF_UPDATE_ACCELERATION_STRUCTURE = 0x1,
    RUF_UPDATE_MESH_DESCRIPTIONS = 0x2,
    RUF_UPDATE_INSTANCES = 0x4,
    RUF_UPDATE_TRANSFORM = 0x8,
    RUF_UPDATE_MATERIAL = 0x10
};

HYP_MAKE_ENUM_FLAGS(RaytracingUpdateFlags);

enum AccelerationStructureFlags : uint32
{
    ASF_NONE = 0x0,
    ASF_NEEDS_REBUILDING = 0x1,
    ASF_TRANSFORM_UPDATE = 0x2,
    ASF_MATERIAL_UPDATE = 0x4
};

HYP_MAKE_ENUM_FLAGS(AccelerationStructureFlags);

class TLASBase : public RenderObject<TLASBase>
{
protected:
    virtual ~TLASBase() override = default;

public:
    HYP_FORCE_INLINE AccelerationStructureType GetType() const
    {
        return AST_TOP_LEVEL;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetMeshDescriptionsBuffer() const
    {
        return m_meshDescriptionsBuffer;
    }

    virtual bool IsCreated() const = 0;

    virtual void AddBLAS(const BLASRef& blas) = 0;
    virtual void RemoveBLAS(const BLASRef& blas) = 0;
    virtual bool HasBLAS(const BLASRef& blas) = 0;

    virtual RendererResult Create() = 0;
    virtual RendererResult Destroy() = 0;

    virtual RendererResult UpdateStructure(EnumFlags<RaytracingUpdateFlags>& outUpdateStateFlags) = 0;

protected:
    GpuBufferRef m_meshDescriptionsBuffer;
};

class BLASBase : public RenderObject<BLASBase>
{
protected:
    BLASBase()
        : m_materialBinding(0)
    {
    }

    virtual ~BLASBase() override = default;

public:
    HYP_FORCE_INLINE AccelerationStructureType GetType() const
    {
        return AST_BOTTOM_LEVEL;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;
    virtual RendererResult Destroy() = 0;

    HYP_FORCE_INLINE const Handle<Material>& GetMaterial() const
    {
        return m_material;
    }

    HYP_FORCE_INLINE uint32 GetMaterialBinding() const
    {
        return m_materialBinding;
    }

    virtual void SetMaterialBinding(uint32 materialBinding)
    {
        m_materialBinding = materialBinding;
    }

    virtual void SetTransform(const Matrix4& transform) = 0;

protected:
    Handle<Material> m_material;
    uint32 m_materialBinding;
};

} // namespace hyperion
