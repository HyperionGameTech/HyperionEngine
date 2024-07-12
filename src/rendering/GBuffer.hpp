/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GBUFFER_HPP
#define HYPERION_GBUFFER_HPP

#include <Constants.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/utilities/Variant.hpp>
#include <core/Handle.hpp>

#include <rendering/RenderBucket.hpp>
#include <rendering/DefaultFormats.hpp>

#include <rendering/backend/RendererImage.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;

using renderer::Image;

using GBufferFormat = Variant<TextureFormatDefault, InternalFormat, Array<InternalFormat>>;

enum GBufferResourceName : uint
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

        Bucket GetBucket() const { return bucket; }
        void SetBucket(Bucket bucket) { this->bucket = bucket; }
        
        const FramebufferRef &GetFramebuffer() const
            { return framebuffer; }

        const AttachmentRef &GetGBufferAttachment(GBufferResourceName resource_name) const;

        void AddRenderGroup(Handle<RenderGroup> &render_group);
        void AddFramebuffersToRenderGroup(Handle<RenderGroup> &render_group);
        void CreateFramebuffer();
        void Destroy();
    };

    GBuffer();
    GBuffer(const GBuffer &other)               = delete;
    GBuffer &operator=(const GBuffer &other)    = delete;
    ~GBuffer()                                  = default;

    GBufferBucket &Get(Bucket bucket)
        { return m_buckets[static_cast<uint>(bucket)]; }

    const GBufferBucket &Get(Bucket bucket) const
        { return m_buckets[static_cast<uint>(bucket)]; }

    GBufferBucket &operator[](Bucket bucket)
        { return Get(bucket); }

    const GBufferBucket &operator[](Bucket bucket) const
        { return Get(bucket); }

    void Create();
    void Destroy();

private:
    FixedArray<GBufferBucket, Bucket::BUCKET_MAX>   m_buckets;
};

} // namespace hyperion

#endif