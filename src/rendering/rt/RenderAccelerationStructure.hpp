/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Handle.hpp>

#include <rendering/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Material;

enum class AccelerationStructureType : uint8
{
    BOTTOM_LEVEL,
    TOP_LEVEL
};

using RTUpdateStateFlags = uint32;

enum RTUpdateStateFlagBits : RTUpdateStateFlags
{
    RT_UPDATE_STATE_FLAGS_NONE = 0x0,
    RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE = 0x1,
    RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS = 0x2,
    RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES = 0x4,
    RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM = 0x8,
    RT_UPDATE_STATE_FLAGS_UPDATE_MATERIAL = 0x10
};

using AccelerationStructureFlags = uint32;

enum AccelerationStructureFlagBits : AccelerationStructureFlags
{
    ACCELERATION_STRUCTURE_FLAGS_NONE = 0x0,
    ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING = 0x1,
    ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE = 0x2,
    ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE = 0x4
};

class HYP_API TLASBase : public RenderObject<TLASBase>
{
protected:
    virtual ~TLASBase() override = default;

public:
    HYP_FORCE_INLINE AccelerationStructureType GetType() const
    {
        return AccelerationStructureType::TOP_LEVEL;
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

    virtual RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags) = 0;

protected:
    GpuBufferRef m_meshDescriptionsBuffer;
};

class HYP_API BLASBase : public RenderObject<BLASBase>
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
        return AccelerationStructureType::BOTTOM_LEVEL;
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
