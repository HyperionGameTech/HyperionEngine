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

#include <Rendering/RenderBucket.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/Shared.hpp>

namespace Hyperion {

HYP_ENUM()
enum GBufferTargetName : uint32
{
    GTN_ALBEDO = 0,
    GTN_NORMALS,
    GTN_MATERIAL,
    GTN_VELOCITY,
    GTN_DEPTH,

    GTN_MAX
};

static_assert(GTN_MAX == NumGBufferTargets, "GTN_MAX does not match NumGbufferTargets");

HYP_CLASS(NoScriptBindings)
class GBuffer : public ObjectBase
{
    HYP_OBJECT_BODY(GBuffer);

public:
    class GBufferTarget
    {
        friend class GBuffer;

        GBuffer* m_gbuffer;
        RenderBucket m_bucket;
        FramebufferRef m_framebuffer;

    public:
        GBufferTarget();
        GBufferTarget(const GBufferTarget& other) = delete;
        GBufferTarget& operator=(const GBufferTarget& other) = delete;
        GBufferTarget(GBufferTarget&& other) noexcept = delete;
        GBufferTarget& operator=(GBufferTarget&& other) noexcept = delete;
        ~GBufferTarget();

        HYP_FORCE_INLINE GBuffer* GetGBuffer() const
        {
            return m_gbuffer;
        }

        HYP_FORCE_INLINE void SetGBuffer(GBuffer* gbuffer)
        {
            m_gbuffer = gbuffer;
        }

        HYP_FORCE_INLINE RenderBucket GetBucket() const
        {
            return m_bucket;
        }

        HYP_FORCE_INLINE void SetBucket(RenderBucket rb)
        {
            m_bucket = rb;
        }

        HYP_FORCE_INLINE const FramebufferRef& GetFramebuffer() const
        {
            return m_framebuffer;
        }

        HYP_FORCE_INLINE void SetFramebuffer(const FramebufferRef& framebuffer)
        {
            m_framebuffer = framebuffer;
        }

        AttachmentBase* GetGBufferAttachment(GBufferTargetName resourceName) const;
    };

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

    HYP_FORCE_INLINE GBufferTarget& GetBucket(RenderBucket rb)
    {
        return m_buckets[uint32(rb)];
    }

    HYP_FORCE_INLINE const GBufferTarget& GetBucket(RenderBucket rb) const
    {
        return m_buckets[uint32(rb)];
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
    FramebufferRef CreateFramebuffer(const FramebufferRef& parentFramebuffer, Vec2u resolution, RenderBucket rb);

    FixedArray<GBufferTarget, NumRenderBuckets> m_buckets;
    Array<FramebufferRef> m_framebuffers;

    Vec2u m_extent;

    bool m_isCreated;
};

} // namespace Hyperion
