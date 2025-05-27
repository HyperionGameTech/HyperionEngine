/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_ACCELERATION_STRUCTURE_HPP
#define HYPERION_RENDERER_BACKEND_ACCELERATION_STRUCTURE_HPP

#include <core/Defines.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

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
    RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM = 0x8
};

using AccelerationStructureFlags = uint32;

enum AccelerationStructureFlagBits : AccelerationStructureFlags
{
    ACCELERATION_STRUCTURE_FLAGS_NONE = 0x0,
    ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING = 0x1,
    ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE = 0x2,
    ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE = 0x4
};

class TLASBase : public RenderObject<TLASBase>
{
public:
    virtual ~TLASBase() override = default;

    HYP_FORCE_INLINE AccelerationStructureType GetType() const
    {
        return AccelerationStructureType::TOP_LEVEL;
    }

    HYP_FORCE_INLINE const GPUBufferRef& GetMeshDescriptionsBuffer() const
    {
        return m_mesh_descriptions_buffer;
    }

    HYP_API virtual void AddBLAS(const BLASRef& blas) = 0;
    HYP_API virtual void RemoveBLAS(const BLASRef& blas) = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual RendererResult UpdateStructure(RTUpdateStateFlags& out_update_state_flags) = 0;

protected:
    GPUBufferRef m_mesh_descriptions_buffer;
};

class BLASBase : public RenderObject<BLASBase>
{
public:
    virtual ~BLASBase() override = default;

    HYP_FORCE_INLINE AccelerationStructureType GetType() const
    {
        return AccelerationStructureType::BOTTOM_LEVEL;
    }

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual void SetTransform(const Matrix4& transform) = 0;
};

} // namespace renderer
} // namespace hyperion

#endif