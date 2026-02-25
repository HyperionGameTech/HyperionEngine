/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/containers/FixedArray.hpp>

#include <Core/utilities/Variant.hpp>

#include <Core/functional/Delegate.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <rendering/RenderBucket.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/Shared.hpp>

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
        AssertDebug(rb > RenderBucket::RB_NONE && rb < RenderBucket::RB_MAX);

        return m_buckets[int(rb) - 1];
    }

    HYP_FORCE_INLINE const GBufferTarget& GetBucket(RenderBucket rb) const
    {
        AssertDebug(rb > RenderBucket::RB_NONE && rb < RenderBucket::RB_MAX);

        return m_buckets[int(rb) - 1];
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

    FixedArray<GBufferTarget, uint32(RB_MAX) - 1> m_buckets;
    Array<FramebufferRef> m_framebuffers;

    Vec2u m_extent;

    bool m_isCreated;
};

} // namespace Hyperion
