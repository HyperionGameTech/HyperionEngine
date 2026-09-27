/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Utilities/RangeAllocator.hpp>

#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

struct RenderProxyMesh;

/// One slot of the instance data buffer, as meshes read it.
/// A slot holds an instance's InstanceData buffers packed in order, so other layouts
/// (e.g UI's transform + UIInstanceAttributes) fit as long as they stay within the size
struct InstanceTransformShaderData
{
    Mat4f transform;
    Mat4f previousTransform;
};

static_assert(sizeof(InstanceTransformShaderData) == 128, "InstanceTransformShaderData layout is shared with shaders");

static constexpr uint32 IdentityInstanceSlot = 0;

struct EntityInstanceSlots
{
    uint32 base = ~0u;
    uint32 count = 0;
    uint32 capacity = 0;

    Array<Vec4f, RenderAllocator> boundingSpheres;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return base != ~0u;
    }
};

class InstanceDataPool final
{
public:
    InstanceDataPool();

    InstanceDataPool(const InstanceDataPool& other) = delete;
    InstanceDataPool& operator=(const InstanceDataPool& other) = delete;

    InstanceDataPool(InstanceDataPool&& other) noexcept = delete;
    InstanceDataPool& operator=(InstanceDataPool&& other) noexcept = delete;

    ~InstanceDataPool();

    /// Writes the identity slot. The instance data buffer must exist by now
    void Initialize();

    void WriteInstances(uint32 entityBinding, const RenderProxyMesh& proxy);
    void ReleaseInstances(uint32 entityBinding);

    /// nullptr when the entity has nothing uploaded: it isn't instanced, or the buffer was full
    const EntityInstanceSlots* GetSlots(uint32 entityBinding) const;

private:
    void ReleaseSlots(EntityInstanceSlots& slots);

    RangeAllocator<RenderAllocator> m_slotAllocator;

    // indexed by entity binding
    Array<EntityInstanceSlots, RenderAllocator> m_entitySlots;
};

} // namespace Hyperion
