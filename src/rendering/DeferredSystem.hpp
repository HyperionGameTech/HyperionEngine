#ifndef HYPERION_V2_DEFERRED_SYSTEM_HPP
#define HYPERION_V2_DEFERRED_SYSTEM_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/HandleID.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/DefaultFormats.hpp>
#include <scene/Scene.hpp>
#include <Types.hpp>

#include <mutex>
#include <array>

namespace hyperion::v2 {

class Engine;

using renderer::Image;
using renderer::AttachmentRef;

using GBufferFormat = Variant<TextureFormatDefault, InternalFormat, Array<InternalFormat>>;

enum GBufferResourceName : UInt
{
    GBUFFER_RESOURCE_ALBEDO = 0,
    GBUFFER_RESOURCE_NORMALS = 1,
    GBUFFER_RESOURCE_MATERIAL = 2,
    GBUFFER_RESOURCE_TANGENTS = 3,
    GBUFFER_RESOURCE_VELOCITY = 4,
    GBUFFER_RESOURCE_MASK = 5,
    GBUFFER_RESOURCE_DEPTH = 6,

    GBUFFER_RESOURCE_MAX
};

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

        Bucket bucket { BUCKET_OPAQUE };
        Handle<RenderPass> render_pass;
        FixedArray<Handle<Framebuffer>, max_frames_in_flight> framebuffers;
        Array<std::unique_ptr<Attachment>> attachments;
        Array<Handle<RenderGroup>> renderer_instances;
        Array<Handle<RenderGroup>> renderer_instances_pending_addition;
        AtomicVar<bool> renderer_instances_changed;
        std::mutex renderer_instances_mutex;

    public:
        RenderGroupHolder();
        ~RenderGroupHolder();

        Bucket GetBucket() const { return bucket; }
        void SetBucket(Bucket bucket) { this->bucket = bucket; }
        
        Handle<RenderPass> &GetRenderPass() { return render_pass; }
        const Handle<RenderPass> &GetRenderPass() const { return render_pass; }
        
        FixedArray<Handle<Framebuffer>, max_frames_in_flight> &GetFramebuffers() { return framebuffers; }
        const FixedArray<Handle<Framebuffer>, max_frames_in_flight> &GetFramebuffers() const { return framebuffers; }

        Array<Handle<RenderGroup>> &GetRenderGroups() { return renderer_instances; }
        const Array<Handle<RenderGroup>> &GetRenderGroups() const { return renderer_instances; }

        AttachmentRef *GetGBufferAttachment(GBufferResourceName resource_name) const
        {
            AssertThrow(render_pass.IsValid());
            AssertThrow(UInt(resource_name) < UInt(GBUFFER_RESOURCE_MAX));

            return render_pass->GetRenderPass().GetAttachmentRefs()[UInt(resource_name)];
        }

        void AddRenderGroup(Handle<RenderGroup> &renderer_instance);
        void AddPendingRenderGroups();
        void AddFramebuffersToPipelines();
        void AddFramebuffersToPipeline(Handle<RenderGroup> &pipeline);
        void CreateRenderPass();
        void CreateFramebuffers();
        void Destroy();
    };

    DeferredSystem();
    DeferredSystem(const DeferredSystem &other) = delete;
    DeferredSystem &operator=(const DeferredSystem &other) = delete;
    ~DeferredSystem() = default;

    RenderGroupHolder &Get(Bucket bucket)
        { return m_buckets[static_cast<UInt>(bucket)]; }

    const RenderGroupHolder &Get(Bucket bucket) const
        { return m_buckets[static_cast<UInt>(bucket)]; }

    RenderGroupHolder &operator[](Bucket bucket)
        { return Get(bucket); }

    const RenderGroupHolder &operator[](Bucket bucket) const
        { return Get(bucket); }

    void Create();
    void Destroy();

    void AddPendingRenderGroups();
    void AddFramebuffersToPipelines();

private:

    FixedArray<RenderGroupHolder, Bucket::BUCKET_MAX> m_buckets;
};

} // namespace hyperion::v2

#endif