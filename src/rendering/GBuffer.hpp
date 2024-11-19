/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GBUFFER_HPP
#define HYPERION_GBUFFER_HPP

#include <Constants.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/utilities/Variant.hpp>

#include <core/functional/Delegate.hpp>

#include <core/Handle.hpp>

#include <rendering/RenderBucket.hpp>
#include <rendering/DefaultFormats.hpp>

#include <rendering/backend/RendererImage.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;

using renderer::Image;

using GBufferFormat = Variant<TextureFormatDefault, InternalFormat, Array<InternalFormat>>;

enum GBufferResourceName : uint32
{
    GBUFFER_RESOURCE_ALBEDO = 0,
    GBUFFER_RESOURCE_NORMALS = 1,
    GBUFFER_RESOURCE_MATERIAL = 2,
    GBUFFER_RESOURCE_TANGENTS = 3,
    GBUFFER_RESOURCE_VELOCITY = 4,
    GBUFFER_RESOURCE_MASK = 5,
    GBUFFER_RESOURCE_WS_NORMALS = 6,
    GBUFFER_RESOURCE_DEPTH = 7,

    GBUFFER_RESOURCE_MAX
};

static_assert(uint(GBufferResourceName::GBUFFER_RESOURCE_MAX) == num_gbuffer_textures, "GBufferResourceName enum does not match num_gbuffer_textures");

struct GBufferResource
{
    GBufferFormat format;
};

class GBuffer
{
public:
    static const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> gbuffer_resources;
    
    class GBufferBucket
    {
        friend class GBuffer;

        Bucket          bucket { BUCKET_OPAQUE };
        FramebufferRef  framebuffer;

    public:
        GBufferBucket();
        GBufferBucket(const GBufferBucket &other)                   = delete;
        GBufferBucket &operator=(const GBufferBucket &other)        = delete;
        GBufferBucket(GBufferBucket &&other) noexcept               = delete;
        GBufferBucket &operator=(GBufferBucket &&other) noexcept    = delete;
        ~GBufferBucket();

        HYP_FORCE_INLINE Bucket GetBucket() const
            { return bucket; }
            
        HYP_FORCE_INLINE void SetBucket(Bucket bucket)
            { this->bucket = bucket; }
        
        HYP_FORCE_INLINE const FramebufferRef &GetFramebuffer() const
            { return framebuffer; }

        const AttachmentRef &GetGBufferAttachment(GBufferResourceName resource_name) const;

        void CreateFramebuffer(Vec2u resolution);
        void Resize(Vec2u resolution);
        void Destroy();
    };

    GBuffer();
    GBuffer(const GBuffer &other)                   = delete;
    GBuffer &operator=(const GBuffer &other)        = delete;
    GBuffer(GBuffer &&other) noexcept               = delete;
    GBuffer &operator=(GBuffer &&other) noexcept    = delete;
    ~GBuffer()                                      = default;

    HYP_FORCE_INLINE GBufferBucket &GetBucket(Bucket bucket)
        { return m_buckets[int(bucket)]; }

    HYP_FORCE_INLINE const GBufferBucket &GetBucket(Bucket bucket) const
        { return m_buckets[int(bucket)]; }

    HYP_FORCE_INLINE const FixedArray<GBufferBucket, Bucket::BUCKET_MAX> &GetBuckets() const
        { return m_buckets; }

    HYP_FORCE_INLINE const Vec2u &GetResolution() const
        { return m_resolution; }

    void Create();
    void Destroy();

    void Resize(Vec2u resolution);

    Delegate<void, Vec2u>                           OnGBufferResolutionChanged;

private:
    FixedArray<GBufferBucket, Bucket::BUCKET_MAX>   m_buckets;

    Vec2u                                           m_resolution;
};

} // namespace hyperion

#endif