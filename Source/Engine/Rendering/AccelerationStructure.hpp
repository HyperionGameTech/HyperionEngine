
/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

namespace Hyperion {

class Material;

HYP_ENUM()
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

HYP_CLASS(Abstract, NoScriptBindings)
class GpuTlasBase : public ObjectBase
{
    HYP_OBJECT_BODY(GpuTlasBase);

public:
    static Pool* GetAllocator()
    {
        return g_rhiPool;
    }

    static constexpr uint32 MaxBlases = 1024;

    GpuTlasBase()
        : m_meshDescriptionsBuffer(MaxBlases, sizeof(MeshDescription))
    {
    }

    virtual ~GpuTlasBase() override = default;

#ifdef HYP_RHI_DEBUG_NAMES
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    HYP_FORCE_INLINE AccelerationStructureType GetType() const
    {
        return AccelerationStructureType::TOP_LEVEL;
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetMeshDescriptionsBuffer() const
    {
        return m_meshDescriptionsBuffer;
    }

    virtual bool IsCreated() const = 0;

    virtual void AddGpuBlas(uint64 key, GpuBlas* blas) = 0;
    virtual void RemoveGpuBlas(uint64 key) = 0;
    virtual bool HasGpuBlas(uint64 key) = 0;

    virtual RendererResult Create() = 0;

    virtual RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags) = 0;

protected:
    StructuredBuffer m_meshDescriptionsBuffer;

#ifdef HYP_RHI_DEBUG_NAMES
    Name m_debugName;
#endif
};

HYP_CLASS(Abstract, NoScriptBindings)
class GpuBlasBase : public ObjectBase
{
    HYP_OBJECT_BODY(GpuBlasBase);

protected:
    GpuBlasBase()
        : m_materialBinding(0)
    {
    }

public:
    static Pool* GetAllocator()
    {
        return g_rhiPool;
    }

    virtual ~GpuBlasBase() override = default;

#ifdef HYP_RHI_DEBUG_NAMES
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    HYP_FORCE_INLINE AccelerationStructureType GetType() const
    {
        return AccelerationStructureType::BOTTOM_LEVEL;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

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

    virtual void SetTransform(const Mat4f& transform) = 0;

protected:
    Handle<Material> m_material;
    uint32 m_materialBinding;

#ifdef HYP_RHI_DEBUG_NAMES
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanAccelerationStructure.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12AccelerationStructure.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
