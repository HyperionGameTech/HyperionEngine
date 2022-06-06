#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include <asset/assets.h>

#include <rendering/post_fx.h>
#include <rendering/render_list.h>
#include <rendering/deferred.h>
#include <rendering/shadows.h>
#include <rendering/resources.h>
#include <rendering/shader_manager.h>
#include <rendering/renderable_attributes.h>
#include <rendering/default_formats.h>
#include <rendering/dummy_data.h>
#include <scene/octree.h>

#include "game_thread.h"

#include <core/scheduler.h>
#include <core/lib/flat_map.h>

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_image_view.h>
#include <rendering/backend/renderer_sampler.h>
#include <rendering/backend/renderer_semaphore.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <util/enum_options.h>
#include <builders/shader_compiler/shader_compiler.h>

#include <types.h>

#include <memory>
#include <mutex>
#include <stack>

#define HYP_FLUSH_RENDER_QUEUE(engine) \
    do { \
        (engine)->render_scheduler.FlushOrWait([](auto &fn) { \
            HYPERION_ASSERT_RESULT(fn(nullptr, 0)); \
        }); \
    } while (0)

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;
using renderer::StorageBuffer;

class Engine;

/* Current descriptor / attachment layout */

/*
 * | ====== Set 0 ====== | ====== Set 1 ====== | ====== Set 2 ====== | ====== Set 3 ====== | ====== Set 4 ====== |
 * | (UNUSED)            | GBuffer: textures[] | Scene data SSBO     | Material data SSBO  | Bindless textures   |
 * |                     | Gbuffer: depth      | empty               | Object data SSBO    | empty               |
 * |                     |                     | empty               | Skeleton data SSBO  | empty               |
 * |                     |                     | empty               | empty               | empty               |
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

using EngineThreadMask = uint;

enum EngineThread : EngineThreadMask {
    THREAD_MAIN   = 0x01,
    THREAD_RENDER = 0x01, // for now
    THREAD_GAME   = 0x02
};

struct RenderState {
    struct SceneBinding {
        Scene::ID id;
        Scene::ID parent_id;

        explicit operator bool() const { return bool(id); }
    };

    std::stack<SceneBinding> scene_ids;

    void BindScene(const Scene *scene)
    {
        scene_ids.push(
            scene == nullptr
                ? SceneBinding{}
                : SceneBinding{scene->GetId(), scene->GetParentId()}
        );
    }

    void UnbindScene()
    {
        scene_ids.pop();
    }

    SceneBinding GetScene() const
    {
        return scene_ids.empty() ? SceneBinding{} : scene_ids.top();
    }
};

struct GraphicsPipelineAttributeSet {
    Ref<Shader>        shader;
    VertexAttributeSet vertex_attributes;
    Bucket             bucket;

    bool operator<(const GraphicsPipelineAttributeSet &other) const
    {
        const auto shader_id = shader ? shader->GetId() : Shader::ID{0};
        const auto other_shader_id = other.shader ? other.shader->GetId() : Shader::ID{0};

        return std::tie(shader_id, vertex_attributes, bucket) <
            std::tie(other_shader_id, other.vertex_attributes, other.bucket);
    }

    bool operator==(const GraphicsPipelineAttributeSet &other) const
    {
        return shader == other.shader
            && vertex_attributes == other.vertex_attributes
            && bucket == other.bucket;
    }
};

/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine {
public:
    static const FlatMap<EngineThread, ThreadId> thread_ids;

    static void AssertOnThread(EngineThreadMask mask);

    Engine(SystemSDL &, const char *app_name);
    ~Engine();
    
    Instance *GetInstance() const                             { return m_instance.get(); }
    Device   *GetDevice() const                               { return m_instance ? m_instance->GetDevice() : nullptr; }

    DeferredRenderer &GetDeferredRenderer()                   { return m_deferred_renderer; }
    const DeferredRenderer &GetDeferredRenderer() const       { return m_deferred_renderer; }

    RenderListContainer &GetRenderListContainer()             { return m_render_list_container; }
    const RenderListContainer &GetRenderListContainer() const { return m_render_list_container; }

    auto &GetRenderScheduler()                                { return render_scheduler; }
    const auto &GetRenderScheduler() const                    { return render_scheduler; }

    auto &GetShaderData()                                     { return shader_globals; }
    const auto &GetShaderData() const                         { return shader_globals; }
    
    auto &GetDummyData()                                      { return m_dummy_data; }
    const auto &GetDummyData() const                          { return m_dummy_data; }

    Octree &GetOctree() { return m_octree; }
    const Octree &GetOctree() const { return m_octree; }

    Image::InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }

    Ref<GraphicsPipeline> FindOrCreateGraphicsPipeline(const RenderableAttributeSet &renderable_attributes);
    Ref<GraphicsPipeline> AddGraphicsPipeline(std::unique_ptr<GraphicsPipeline> &&pipeline);

    void Initialize();
    void PrepareSwapchain();
    void Compile();

    void ResetRenderState();
    void UpdateBuffersAndDescriptors(uint32_t frame_index);
    
    void RenderDeferred(Frame *frame);
    void RenderFinalPass(CommandBuffer *command_buffer) const;

    ShaderGlobals          *shader_globals;

    EngineCallbacks         callbacks;
    Resources               resources;
    Assets                  assets;
    ShaderManager           shader_manager;

    RenderState             render_state;
    
    std::atomic_bool m_running{false};

    Scheduler<
        renderer::Result,
        CommandBuffer * /* command_buffer */,
        uint32_t /* frame_index */
    > render_scheduler;

    GameThread game_thread;

private:
    void CreateDummyData();
    void FindTextureFormatDefaults();

    std::unique_ptr<Instance>         m_instance;
    std::unique_ptr<GraphicsPipeline> m_root_pipeline;

    EnumOptions<TextureFormatDefault, Image::InternalFormat, 16> m_texture_format_defaults;

    DeferredRenderer    m_deferred_renderer;
    RenderListContainer m_render_list_container;


    Octree::Root m_octree_root;
    Octree       m_octree;

    /* TMP */
    std::vector<std::unique_ptr<renderer::Attachment>> m_render_pass_attachments;

    FlatMap<RenderableAttributeSet, GraphicsPipeline::ID> m_graphics_pipeline_mapping;

    DummyData m_dummy_data;
};

} // namespace hyperion::v2

#endif

