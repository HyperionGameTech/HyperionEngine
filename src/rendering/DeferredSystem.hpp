#ifndef HYPERION_V2_DEFERRED_SYSTEM_HPP
#define HYPERION_V2_DEFERRED_SYSTEM_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/Handle.hpp>
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

class DeferredSystem
{
public:
    using GBufferFormat = Variant<TextureFormatDefault, InternalFormat>;

    static const FixedArray<GBufferFormat, num_gbuffer_textures> gbuffer_texture_formats;
    
    class RendererInstanceHolder
    {
        friend class DeferredSystem;

        Bucket bucket { BUCKET_OPAQUE };
        Handle<RenderPass> render_pass;
        DynArray<Handle<Framebuffer>> framebuffers;
        DynArray<std::unique_ptr<Attachment>> attachments;
        DynArray<Handle<RendererInstance>> renderer_instances;
        DynArray<Handle<RendererInstance>> renderer_instances_pending_addition;
        AtomicVar<bool> renderer_instances_changed;
        std::mutex renderer_instances_mutex;

    public:
        RendererInstanceHolder();
        ~RendererInstanceHolder();

        Bucket GetBucket() const { return bucket; }
        void SetBucket(Bucket bucket) { this->bucket = bucket; }
        
        Handle<RenderPass> &GetRenderPass() { return render_pass; }
        const Handle<RenderPass> &GetRenderPass() const { return render_pass; }
        
        DynArray<Handle<Framebuffer>> &GetFramebuffers() { return framebuffers; }
        const DynArray<Handle<Framebuffer>> &GetFramebuffers() const { return framebuffers; }

        DynArray<Handle<RendererInstance>> &GetRendererInstances() { return renderer_instances; }
        const DynArray<Handle<RendererInstance>> &GetRendererInstances() const { return renderer_instances; }

        void AddRendererInstance(Handle<RendererInstance> &renderer_instance);
        void AddPendingRendererInstances(Engine *engine);
        void AddFramebuffersToPipelines();
        void AddFramebuffersToPipeline(Handle<RendererInstance> &pipeline);
        void CreateRenderPass(Engine *engine);
        void CreateFramebuffers(Engine *engine);
        void Destroy(Engine *engine);
    };

    DeferredSystem();
    DeferredSystem(const DeferredSystem &other) = delete;
    DeferredSystem &operator=(const DeferredSystem &other) = delete;
    ~DeferredSystem() = default;

    RendererInstanceHolder &Get(Bucket bucket)
        { return m_buckets[static_cast<UInt>(bucket)]; }

    const RendererInstanceHolder &Get(Bucket bucket) const
        { return m_buckets[static_cast<UInt>(bucket)]; }

    RendererInstanceHolder &operator[](Bucket bucket)
        { return Get(bucket); }

    const RendererInstanceHolder &operator[](Bucket bucket) const
        { return Get(bucket); }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void AddPendingRendererInstances(Engine *engine);
    void AddFramebuffersToPipelines(Engine *engine);

private:
    FixedArray<RendererInstanceHolder, Bucket::BUCKET_MAX> m_buckets;
};

} // namespace hyperion::v2

#endif