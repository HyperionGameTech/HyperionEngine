/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Utilities/Variant.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Rendering/GpuImage.hpp>
#include <Rendering/Shared.hpp>

namespace Hyperion {

HYP_ENUM()
enum class GBufferPass : uint8
{
    Opaque,
    Translucent,
    Lightmapped,
    Debug,
    Effect,
    Max
};

static constexpr uint8 NumGBufferPasses = static_cast<uint8>(GBufferPass::Max);

class GBuffer;

struct GBufferTarget
{
    enum TargetName : uint8
    {
        Albedo = 0,
        Normals,
        MatData,
        Velocity,
        Depth,
        Max
    };

    static_assert(static_cast<uint8>(Max) == NumGBufferTargets, "Max does not match NumGBufferTargets");

    GBuffer* gbuffer;
    GBufferPass pass;
    FramebufferRef framebuffer;

    GBufferTarget() = default;

    GBufferTarget(const GBufferTarget& other) = delete;
    GBufferTarget& operator=(const GBufferTarget& other) = delete;

    Attachment* GetAttachment(TargetName resourceName) const;
};

HYP_CLASS(NoScriptBindings)
class GBuffer : public ObjectBase
{
    HYP_OBJECT_BODY(GBuffer);

public:
    GBuffer(Vec2u extent);

    GBuffer(const GBuffer& other) = delete;
    GBuffer& operator=(const GBuffer& other) = delete;

    GBuffer(GBuffer&& other) noexcept = delete;
    GBuffer& operator=(GBuffer&& other) noexcept = delete;

    ~GBuffer();

    HYP_FORCE_INLINE bool IsCreated() const
    {
        return m_isCreated;
    }

    HYP_FORCE_INLINE GBufferTarget& GetPass(GBufferPass pass)
    {
        return m_passes[static_cast<uint8>(pass)];
    }

    HYP_FORCE_INLINE const GBufferTarget& GetPass(GBufferPass pass) const
    {
        return m_passes[static_cast<uint8>(pass)];
    }

    HYP_FORCE_INLINE const Array<FramebufferRef>& GetFramebuffers() const
    {
        return m_framebuffers;
    }

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_extent;
    }

    void Create();

    void Resize(Vec2u extent);

    Delegate<void, Vec2u> OnGBufferResolutionChanged;

private:
    void CreateBucketFramebuffers();
    FramebufferRef CreateFramebuffer(const FramebufferRef& parentFramebuffer, Vec2u resolution, GBufferPass pass);

    FixedArray<GBufferTarget, NumGBufferPasses> m_passes;
    Array<FramebufferRef> m_framebuffers;

    Vec2u m_extent;

    bool m_isCreated;
};

} // namespace Hyperion
