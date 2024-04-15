/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_DEFERRED_SYSTEM_HPP
#define HYPERION_V2_DEFERRED_SYSTEM_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/ID.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/DefaultFormats.hpp>
#include <scene/Scene.hpp>
#include <Types.hpp>

#include <mutex>
#include <array>

namespace hyperion::v2 {

class Engine;

using renderer::Image;
using renderer::AttachmentUsage;

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

class DeferredSystem
{
public:
    static const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> gbuffer_resources;
    
    class RenderGroupHolder
    {
        friend class DeferredSystem;

        Bucket                      bucket { BUCKET_OPAQUE };
        Handle<Framebuffer>         framebuffer;
        Array<AttachmentRef>        attachments;
        Array<Handle<RenderGroup>>  renderer_instances;
        Array<Handle<RenderGroup>>  renderer_instances_pending_addition;
        AtomicVar<bool>             renderer_instances_changed;
        std::mutex                  renderer_instances_mutex;

    public:
        RenderGroupHolder();
        RenderGroupHolder(const RenderGroupHolder &other)                   = delete;
        RenderGroupHolder &operator=(const RenderGroupHolder &other)        = delete;
        RenderGroupHolder(RenderGroupHolder &&other) noexcept               = delete;
        RenderGroupHolder &operator=(RenderGroupHolder &&other) noexcept    = delete;
        ~RenderGroupHolder();

        Bucket GetBucket() const { return bucket; }
        void SetBucket(Bucket bucket) { this->bucket = bucket; }
        
        Handle<Framebuffer> &GetFramebuffer() { return framebuffer; }
        const Handle<Framebuffer> &GetFramebuffer() const { return framebuffer; }

        Array<Handle<RenderGroup>> &GetRenderGroups() { return renderer_instances; }
        const Array<Handle<RenderGroup>> &GetRenderGroups() const { return renderer_instances; }

        const AttachmentUsageRef &GetGBufferAttachment(GBufferResourceName resource_name) const
        {
            AssertThrow(framebuffer.IsValid());
            AssertThrow(uint(resource_name) < uint(GBUFFER_RESOURCE_MAX));

            return framebuffer->GetAttachmentUsages()[uint(resource_name)];
        }

        void AddRenderGroup(Handle<RenderGroup> &render_group);
        void AddPendingRenderGroups();
        void AddFramebuffersToRenderGroups();
        void AddFramebuffersToRenderGroup(Handle<RenderGroup> &pipeline);
        void CreateFramebuffer();
        void Destroy();
    };

    DeferredSystem();
    DeferredSystem(const DeferredSystem &other) = delete;
    DeferredSystem &operator=(const DeferredSystem &other) = delete;
    ~DeferredSystem() = default;

    RenderGroupHolder &Get(Bucket bucket)
        { return m_buckets[static_cast<uint>(bucket)]; }

    const RenderGroupHolder &Get(Bucket bucket) const
        { return m_buckets[static_cast<uint>(bucket)]; }

    RenderGroupHolder &operator[](Bucket bucket)
        { return Get(bucket); }

    const RenderGroupHolder &operator[](Bucket bucket) const
        { return Get(bucket); }

    void Create();
    void Destroy();

    void AddPendingRenderGroups();
    void AddFramebuffersToRenderGroups();

private:

    FixedArray<RenderGroupHolder, Bucket::BUCKET_MAX> m_buckets;
};

} // namespace hyperion::v2

#endif