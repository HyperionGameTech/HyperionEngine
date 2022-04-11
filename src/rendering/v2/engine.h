#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include "asset/assets.h"

#include "components/post_fx.h"
#include "components/render_list.h"
#include "components/deferred.h"
#include "components/shadows.h"
#include "components/octree.h"
#include "components/resources.h"

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_semaphore.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <util/enum_options.h>

#include <memory>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;
using renderer::StorageBuffer;


/* Current descriptor / attachment layout */

/*
 * | ====== Set 0 ====== | ====== Set 1 ====== | ====== Set 2 ====== | ====== Set 3 ====== | ====== Set 4 ====== |
 * | (UNUSED)            | GBuffer: color      | Scene data SSBO     | Material data SSBO  | Bindless textures   |
 * |                     | Gbuffer: normals    | empty               | Object data SSBO    | empty               |
 * |                     | Gbuffer: position   | empty               | Skeleton data SSBO  |                     |
 * |                     | Gbuffer: depth      | empty               | empty               | empty               |
 * |                     | Deferred result     | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Post effect 0       | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Shadow map          | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Image storage test  | empty               | empty               | empty               |
 */


/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine {
public:
    enum TextureFormatDefault {
        TEXTURE_FORMAT_DEFAULT_NONE    = 0,
        TEXTURE_FORMAT_DEFAULT_COLOR   = 1,
        TEXTURE_FORMAT_DEFAULT_DEPTH   = 2,
        TEXTURE_FORMAT_DEFAULT_GBUFFER = 4,
        TEXTURE_FORMAT_DEFAULT_STORAGE = 8
    };

    Engine(SystemSDL &, const char *app_name);
    ~Engine();

    inline Instance *GetInstance() { return m_instance.get(); }
    inline const Instance *GetInstance() const { return m_instance.get(); }

    inline PostProcessing &GetPostProcessing() { return m_post_processing; }
    inline const PostProcessing &GetPostProcessing() const { return m_post_processing; }

    inline DeferredRenderer &GetDeferredRenderer() { return m_deferred_renderer; }
    inline const DeferredRenderer &GetDeferredRenderer() const { return m_deferred_renderer; }

    inline RenderList &GetRenderList() { return m_render_list; }
    inline const RenderList &GetRenderList() const { return m_render_list; }

    inline Octree &GetOctree() { return m_octree; }
    inline const Octree &GetOctree() const { return m_octree; }

    inline Image::InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }



    /* Pipelines will be deferred until descriptor sets are built
     * We take in the builder object rather than a unique_ptr,
     * so that we can reuse pipelines
     */
    template <class ...Args>
    GraphicsPipeline::ID AddGraphicsPipeline(std::unique_ptr<GraphicsPipeline> &&pipeline, Args &&... args)
    {
        const auto bucket = pipeline->GetBucket();

        GraphicsPipeline::ID id = m_render_list[bucket]
            .pipelines.Add(this, std::move(pipeline), std::move(args)...);

        id.bucket = bucket;

        return id;
    }

    template <class ...Args>
    void RemoveGraphicsPipeline(GraphicsPipeline::ID id, Args &&... args)
        { return m_render_list[id.bucket].pipelines.Remove(this, id, std::move(args)...); }

    inline GraphicsPipeline *GetGraphicsPipeline(GraphicsPipeline::ID id)
        { return m_render_list[id.bucket].pipelines.Get(id); }

    inline const GraphicsPipeline *GetGraphicsPipeline(GraphicsPipeline::ID id) const
        { return const_cast<Engine*>(this)->GetGraphicsPipeline(id); }

    void SetSpatialTransform(Spatial *spatial, const Transform &transform);


    void Initialize();
    void Destroy();
    void PrepareSwapchain();
    void Compile();
    void UpdateDescriptorData(uint32_t frame_index);

    void RenderShadows(CommandBuffer *primary, uint32_t frame_index);
    void RenderDeferred(CommandBuffer *primary, uint32_t frame_index);
    void RenderPostProcessing(CommandBuffer *primary, uint32_t frame_index);
    void RenderSwapchain(CommandBuffer *command_buffer) const;


    ShaderGlobals          *shader_globals;

    EngineCallbacks         callbacks;
    Resources               resources;
    Assets                  assets;

private:
    void FindTextureFormatDefaults();

    std::unique_ptr<Instance>         m_instance;
    std::unique_ptr<GraphicsPipeline> m_root_pipeline;

    EnumOptions<TextureFormatDefault, Image::InternalFormat, 5> m_texture_format_defaults;

    PostProcessing   m_post_processing;
    DeferredRenderer m_deferred_renderer;
    ShadowRenderer   m_shadow_renderer;
    RenderList       m_render_list;


    Octree::Root m_octree_root;
    Octree       m_octree;

    /* TMP */
    std::vector<std::unique_ptr<renderer::Attachment>> m_render_pass_attachments;
};

} // namespace hyperion::v2

#endif

