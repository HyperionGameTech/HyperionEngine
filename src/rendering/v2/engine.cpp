#include "engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

#include "components/post_fx.h"
#include "components/compute.h"

#include <rendering/backend/renderer_features.h>

#include "rendering/mesh.h"

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;
using renderer::AttachmentBase;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;

Engine::Engine(SystemSDL &_system, const char *app_name)
    : m_instance(new Instance(_system, app_name, "HyperionEngine")),
      m_shader_globals(nullptr)
{
}

Engine::~Engine()
{
    AssertThrowMsg(m_instance == nullptr, "Instance should have been destroyed");
}

Framebuffer::ID Engine::AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass_id)
{
    AssertThrow(framebuffer != nullptr);

    RenderPass *render_pass = GetRenderPass(render_pass_id);
    AssertThrow(render_pass != nullptr);

    return m_framebuffers.Add(this, std::move(framebuffer), &render_pass->Get());
}

Framebuffer::ID Engine::AddFramebuffer(size_t width, size_t height, RenderPass::ID render_pass_id)
{
    RenderPass *render_pass = GetRenderPass(render_pass_id);
    AssertThrow(render_pass != nullptr);

    auto framebuffer = std::make_unique<Framebuffer>(width, height);

    /* Add all attachments from the renderpass */
    for (auto &it : render_pass->Get().GetAttachments()) {
        framebuffer->Get().AddAttachment(it.second.format);
    }

    return AddFramebuffer(std::move(framebuffer), render_pass_id);
}

void Engine::SetSpatialTransform(Spatial::ID id, const Transform &transform)
{
    Spatial *spatial = GetSpatial(id);
    spatial->SetTransform(transform);

    m_shader_globals->objects.Set(id.value - 1, {.model_matrix = spatial->GetTransform().GetMatrix()});
}

void Engine::InitializeInstance()
{
    HYPERION_ASSERT_RESULT(m_instance->Initialize(true));
}

void Engine::FindTextureFormatDefaults()
{
    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_STORAGE,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
        )
    );
}

void Engine::PrepareSwapchain()
{
    m_deferred_rendering.CreateShader(this);
    m_deferred_rendering.CreateRenderPass(this);
    m_deferred_rendering.CreateFrameData(this);

    uint32_t binding_index = 4; /* TMP */
    m_deferred_rendering.CreateDescriptors(this, binding_index);

    m_post_processing.Create(this);

    Shader::ID shader_id{};

    // TODO: should be moved elsewhere. SPIR-V for rendering quad could be static
    {
        shader_id = AddShader(std::make_unique<Shader>(std::vector<SubShader>{
            {ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_vert.spv").Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_frag.spv").Read()}}
        }));
    }

    RenderPass::ID render_pass_id{};

    {
        auto render_pass = std::make_unique<RenderPass>(renderer::RenderPass::RENDER_PASS_STAGE_PRESENT, renderer::RenderPass::RENDER_PASS_INLINE);
        /* For our color attachment */
        render_pass->Get().AddColorAttachment(
            std::make_unique<Attachment<VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR>>
                (0, m_instance->swapchain->image_format));

        render_pass->Get().AddAttachment({
            .format = m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH)
        });
        
        render_pass_id = AddRenderPass(std::move(render_pass));
    }

    m_swapchain_render_container = std::make_unique<GraphicsPipeline>(shader_id, render_pass_id, GraphicsPipeline::Bucket::BUCKET_BUFFER);

    for (VkImage img : m_instance->swapchain->images) {
        auto image_view = std::make_unique<ImageView>();

        /* Create imageview independent of a Image */
        HYPERION_ASSERT_RESULT(image_view->Create(
            m_instance->GetDevice(),
            img,
            m_instance->swapchain->image_format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D
        ));

        auto fbo = std::make_unique<Framebuffer>(m_instance->swapchain->extent.width, m_instance->swapchain->extent.height);
        fbo->Get().AddAttachment(
            FramebufferObject::AttachmentImageInfo{
                .image = nullptr,
                .image_view = std::move(image_view),
                .sampler = nullptr,
                .image_needs_creation = false,
                .image_view_needs_creation = false,
                .sampler_needs_creation = true
            }
        );

        /* Now we add a depth buffer */
        HYPERION_ASSERT_RESULT(fbo->Get().AddAttachment(m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH)));

        m_swapchain_render_container->AddFramebuffer(AddFramebuffer(std::move(fbo), render_pass_id));
    }

    m_swapchain_render_container->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
}

void Engine::Initialize()
{
    InitializeInstance();
    FindTextureFormatDefaults();

    m_shader_globals = new ShaderGlobals(m_instance->GetFrameHandler()->NumFrames());
    
    /* for scene data */
    m_shader_globals->scenes.Create(m_instance->GetDevice());
    /* for materials */
    m_shader_globals->materials.Create(m_instance->GetDevice());
    /* for objects */
    m_shader_globals->objects.Create(m_instance->GetDevice());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        ->AddSubDescriptor({
            .gpu_buffer = m_shader_globals->scenes.GetBuffers()[0].get(),
            .range = sizeof(SceneShaderData)
        });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        ->AddSubDescriptor({
            .gpu_buffer = m_shader_globals->materials.GetBuffers()[0].get(),
            .range = sizeof(MaterialShaderData)
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        ->AddSubDescriptor({
            .gpu_buffer = m_shader_globals->objects.GetBuffers()[0].get(),
            .range = sizeof(ObjectShaderData)
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        ->AddSubDescriptor({
            .gpu_buffer = m_shader_globals->scenes.GetBuffers()[1].get(),
            .range = sizeof(SceneShaderData)
        });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        ->AddSubDescriptor({
            .gpu_buffer = m_shader_globals->materials.GetBuffers()[1].get(),
            .range = sizeof(MaterialShaderData)
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        ->AddSubDescriptor({
            .gpu_buffer = m_shader_globals->objects.GetBuffers()[1].get(),
            .range = sizeof(ObjectShaderData)
        });
}

void Engine::Destroy()
{
    AssertThrow(m_instance != nullptr);

    (void)m_instance->GetDevice()->Wait();

    m_deferred_rendering.Destroy(this);

    m_post_processing.Destroy(this);

    m_framebuffers.RemoveAll(this);
    m_render_passes.RemoveAll(this);
    m_shaders.RemoveAll(this);
    m_textures.RemoveAll(this);
    m_materials.RemoveAll(this);
    m_compute_pipelines.RemoveAll(this);

    m_swapchain_render_container->Destroy(this);
    m_render_bucket_container.Destroy(this);

    if (m_shader_globals != nullptr) {
        m_shader_globals->scenes.Destroy(m_instance->GetDevice());
        m_shader_globals->objects.Destroy(m_instance->GetDevice());
        m_shader_globals->materials.Destroy(m_instance->GetDevice());

        delete m_shader_globals;
    }

    m_instance->Destroy();
    m_instance.reset();
}

void Engine::Compile()
{
    /* Finalize materials */
    for (size_t i = 0; i < m_instance->GetFrameHandler()->NumFrames(); i++) {
        m_shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        m_shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), i);
    }

    /* Finalize descriptor pool */
    HYPERION_ASSERT_RESULT(m_instance->GetDescriptorPool().Create(m_instance->GetDevice()));

    m_deferred_rendering.CreatePipeline(this);

    m_post_processing.BuildPipelines(this);

    m_swapchain_render_container->Create(this);
    m_render_bucket_container.Create(this);
    m_compute_pipelines.CreateAll(this);
}

void Engine::UpdateDescriptorData(uint32_t frame_index)
{
    m_shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), frame_index);
}

void Engine::Render(CommandBuffer *primary, uint32_t frame_index)
{
    m_render_bucket_container.GetBucket(GraphicsPipeline::Bucket::BUCKET_OPAQUE).objects[0]->Get().BeginRenderPass(primary, 0);
    for (const auto &pipeline : m_render_bucket_container.GetBucket(GraphicsPipeline::Bucket::BUCKET_SKYBOX).objects) {
        pipeline->Render(this, primary, frame_index);
    }

    for (const auto &pipeline : m_render_bucket_container.GetBucket(GraphicsPipeline::Bucket::BUCKET_OPAQUE).objects) {
        pipeline->Render(this, primary, frame_index);
    }

    m_render_bucket_container.GetBucket(GraphicsPipeline::Bucket::BUCKET_OPAQUE).objects[0]->Get().EndRenderPass(primary, 0);
}

void Engine::RenderDeferred(CommandBuffer *primary, uint32_t frame_index)
{
    m_deferred_rendering.Record(this, frame_index);
    m_deferred_rendering.Render(this, primary, frame_index);

    for (const auto &pipeline : m_render_bucket_container.GetBucket(GraphicsPipeline::Bucket::BUCKET_TRANSLUCENT).objects) {
        pipeline->Render(this, primary, frame_index);
    }
}

void Engine::RenderPostProcessing(CommandBuffer *primary, uint32_t frame_index)
{
    m_post_processing.Render(this, primary, frame_index);
}

void Engine::RenderSwapchain(CommandBuffer *command_buffer) const
{
    auto &pipeline = m_swapchain_render_container->Get();
    const uint32_t acquired_image_index = m_instance->GetFrameHandler()->GetAcquiredImageIndex();

    pipeline.BeginRenderPass(command_buffer, acquired_image_index, VK_SUBPASS_CONTENTS_INLINE);
    pipeline.Bind(command_buffer);

    m_instance->GetDescriptorPool().Bind(command_buffer, &pipeline, {{.count = 2}});

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    PostEffect::full_screen_quad->RenderVk(command_buffer, m_instance.get(), nullptr);

    pipeline.EndRenderPass(command_buffer, acquired_image_index);
}
} // namespace hyperion::v2