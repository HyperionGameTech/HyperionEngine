#include "Deferred.hpp"
#include "../Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;

DeferredPass::DeferredPass(bool is_indirect_pass)
    : FullScreenPass(Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB),
      m_is_indirect_pass(is_indirect_pass)
{
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader(Engine *engine)
{
    if (m_is_indirect_pass) {
        m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                SubShader{ShaderModule::Type::VERTEX, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred.vert.spv")).Read(),
                    {.name = "deferred indirect vert"}
                }},
                SubShader{ShaderModule::Type::FRAGMENT, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_indirect.frag.spv")).Read(),
                    {.name = "deferred indirect frag"}
                }}
            }
        ));
    } else {
        m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                SubShader{ShaderModule::Type::VERTEX, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred.vert.spv")).Read(),
                    {.name = "deferred direct vert"}
                }},
                SubShader{ShaderModule::Type::FRAGMENT, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_direct.frag.spv")).Read(),
                    {.name = "deferred direct frag"}
                }}
            }
        ));
    }

    m_shader->Init(engine);
}

void DeferredPass::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass().IncRef();
}

void DeferredPass::CreateDescriptors(Engine *engine)
{
    if (m_is_indirect_pass) {
        return;
    }

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();

        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::DEFERRED_RESULT);

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                descriptor->SetSubDescriptor({
                    .element_index = ~0u,
                    .image_view    = attachment_ref->GetImageView(),
                    .sampler       = attachment_ref->GetSampler(),
                });
            }
        }
    }
}

void DeferredPass::Create(Engine *engine)
{
    CreateShader(engine);
    CreateRenderPass(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[i].IncRef();
        
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_command_buffers[i] = std::move(command_buffer);
    }

    RenderableAttributeSet renderable_attributes {
        .bucket            = BUCKET_INTERNAL,
        .vertex_attributes = renderer::static_mesh_vertex_attributes,
        .fill_mode         = FillMode::FILL,
        .depth_write       = false,
        .depth_test        = false
    };

    if (!m_is_indirect_pass) {
        renderable_attributes.alpha_blending = true;
    }

    CreatePipeline(engine, renderable_attributes);
}

void DeferredPass::Destroy(Engine *engine)
{
    FullScreenPass::Destroy(engine); // flushes render queue
}

void DeferredPass::Record(Engine *engine, UInt frame_index)
{
    if (m_is_indirect_pass) {
        FullScreenPass::Record(engine, frame_index);
        
        return;
    }

    // no lights bound, do not render direct shading at all
    if (engine->render_state.light_ids.Empty()) {
        return;
    }

    using renderer::Result;

    auto *command_buffer = m_command_buffers[frame_index].get();

    auto record_result = command_buffer->Record(
        engine->GetInstance()->GetDevice(),
        m_pipeline->GetPipeline()->GetConstructionInfo().render_pass,
        [this, engine, frame_index](CommandBuffer *cmd) {
            m_pipeline->GetPipeline()->push_constants = m_push_constant_data;
            m_pipeline->GetPipeline()->Bind(cmd);

            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER
            );

            // render with each light
            for (auto &light_id : engine->render_state.light_ids) {
                cmd->BindDescriptorSet(
                    engine->GetInstance()->GetDescriptorPool(),
                    m_pipeline->GetPipeline(),
                    DescriptorSet::scene_buffer_mapping[frame_index],
                    DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                    FixedArray {
                        UInt32(sizeof(SceneShaderData) * 0),
                        UInt32(sizeof(LightShaderData) * (light_id.value - 1))
                    }
                );

                full_screen_quad->Render(engine, cmd);
            }

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void DeferredPass::Render(Engine *engine, Frame *frame)
{
}

DeferredRenderer::DeferredRenderer()
    : Renderer(),
      m_indirect_pass(true),
      m_direct_pass(false)
{
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Create(engine);
    
    CreateComputePipelines(engine);

    m_indirect_pass.Create(engine);
    m_direct_pass.Create(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_mipmapped_results[i] = engine->resources.textures.Add(std::make_unique<Texture2D>(
            Extent2D { 512, 512 },
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
            Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER,
            nullptr
        ));

        m_mipmapped_results[i].Init();

        for (UInt j = 0; j < static_cast<UInt>(m_ssr_image_outputs[i].size()); j++) {
            m_ssr_image_outputs[i][j] = SSRImageOutput {
                .image = std::make_unique<StorageImage>(
                    m_mipmapped_results[i]->GetExtent(),
                    Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                    Image::Type::TEXTURE_TYPE_2D,
                    nullptr
                ),
                .image_view = std::make_unique<ImageView>()
            };

            m_ssr_image_outputs[i][j].Create(engine->GetDevice());
        }

        m_ssr_radius_output[i] = SSRImageOutput {
            .image = std::make_unique<StorageImage>(
                m_mipmapped_results[i]->GetExtent(),
                Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
                Image::Type::TEXTURE_TYPE_2D,
                nullptr
            ),
            .image_view = std::make_unique<ImageView>()
        };

        m_ssr_radius_output[i].Create(engine->GetDevice());
    }

    m_sampler = std::make_unique<Sampler>(Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP);
    HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetDevice()));

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &opaque_fbo = engine->GetRenderListContainer()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[i];
        
        auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        
        descriptor_set_pass->AddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_TEXTURES);

        UInt attachment_index = 0;

        /* Gbuffer textures */
        for (; attachment_index < RenderListContainer::gbuffer_textures.size() - 1; attachment_index++) {
            descriptor_set_pass
                ->GetDescriptor(DescriptorKey::GBUFFER_TEXTURES)
                ->SetSubDescriptor({
                    .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView()
                });
        }

        /* Depth texture */
        descriptor_set_pass
            ->AddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_DEPTH)
            ->SetSubDescriptor({
                .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView()
            });

        /* Mip chain */
        descriptor_set_pass
            ->AddDescriptor<ImageSamplerDescriptor>(DescriptorKey::GBUFFER_MIP_CHAIN)
            ->SetSubDescriptor({
                .image_view = &m_mipmapped_results[i]->GetImageView(),
                .sampler    = &m_mipmapped_results[i]->GetSampler()
            });

        /* Gbuffer sampler */
        descriptor_set_pass
            ->AddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_SAMPLER)
            ->SetSubDescriptor({
                .sampler = m_sampler.get()
            });

        /* SSR Data */
        descriptor_set_pass // 1st stage -- trace, write UVs
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_UV_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][0].image_view.get()
            });

        descriptor_set_pass // 2nd stage -- sample
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_SAMPLE_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][1].image_view.get()
            });

        descriptor_set_pass // 2nd stage -- write radii
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_RADIUS_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_radius_output[i].image_view.get()
            });

        descriptor_set_pass // 3rd stage -- blur horizontal
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_BLUR_HOR_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][2].image_view.get()
            });

        descriptor_set_pass // 3rd stage -- blur vertical
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_BLUR_VERT_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][3].image_view.get()
            });

        /* SSR Data */
        descriptor_set_pass // 1st stage -- trace, write UVs
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_UV_TEXTURE)
            ->SetSubDescriptor({
               .image_view = m_ssr_image_outputs[i][0].image_view.get()
           });

        descriptor_set_pass // 2nd stage -- sample
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_SAMPLE_TEXTURE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][1].image_view.get()
           });

        descriptor_set_pass // 2nd stage -- write radii
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_RADIUS_TEXTURE)
            ->SetSubDescriptor({
                .image_view = m_ssr_radius_output[i].image_view.get()
           });

        descriptor_set_pass // 3rd stage -- blur horizontal
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_BLUR_HOR_TEXTURE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][2].image_view.get()
            });

        descriptor_set_pass // 3rd stage -- blur vertical
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_BLUR_VERT_TEXTURE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][3].image_view.get()
            });
    }
    
    m_indirect_pass.CreateDescriptors(engine); // no-op
    m_direct_pass.CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void DeferredRenderer::CreateComputePipelines(Engine *engine)
{
    m_ssr_write_uvs = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_write_uvs.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_write_uvs.Init();

    m_ssr_sample = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_sample.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_sample.Init();

    m_ssr_blur_hor = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_blur_hor.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_blur_hor.Init();

    m_ssr_blur_vert = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_blur_vert.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_blur_vert.Init();
}

void DeferredRenderer::Destroy(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    //! TODO: remove all descriptors

    m_post_processing.Destroy(engine);

    m_ssr_write_uvs.Reset();
    m_ssr_sample.Reset();
    m_ssr_blur_hor.Reset();
    m_ssr_blur_vert.Reset();

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (UInt j = 0; j < static_cast<UInt>(m_ssr_image_outputs[i].size()); j++) {
            m_ssr_image_outputs[i][j].Destroy(engine->GetDevice());
        }

        m_ssr_radius_output[i].Destroy(engine->GetDevice());

        engine->SafeReleaseRenderable(std::move(m_mipmapped_results[i]));
    }

    HYPERION_ASSERT_RESULT(m_sampler->Destroy(engine->GetDevice()));

    m_indirect_pass.Destroy(engine);  // flushes render queue
    m_direct_pass.Destroy(engine);    // flushes render queue
}

void DeferredRenderer::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const auto scene_binding = engine->render_state.GetScene();
    const auto scene_cull_id = scene_binding.parent_id ? scene_binding.parent_id : scene_binding.id;
    const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();
    auto &mipmapped_result = m_mipmapped_results[frame_index]->GetImage();

    m_indirect_pass.Record(engine, frame_index); // could be moved to only do once
    m_direct_pass.Record(engine, frame_index);

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(BUCKET_OPAQUE);
    
    // begin opaque objs
    bucket.GetFramebuffers()[frame_index]->BeginCapture(primary);
    RenderOpaqueObjects(engine, frame);
    bucket.GetFramebuffers()[frame_index]->EndCapture(primary);
    // end opaque objs
    
    

    /* ========== BEGIN SSR ========== */

    // PASS 1 -- write UVs

    // start by putting the UV image in a writeable state
    const Pipeline::PushConstantData ssr_push_constant_data {
        .ssr_data = {
            .width            = mipmapped_result.GetExtent().width,
            .height           = mipmapped_result.GetExtent().height,
            .ray_step         = 0.45f,
            .num_iterations   = 100.0f,
            .max_ray_distance = 128.0f
        }
    };

    m_ssr_image_outputs[frame_index][0].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_write_uvs->GetPipeline()->Bind(primary, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_ssr_write_uvs->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_ssr_write_uvs->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData)) // light unused here
                }
            }
        }
    );

    m_ssr_write_uvs->GetPipeline()->Dispatch(primary, mipmapped_result.GetExtent() / Extent3D{8, 8, 1});

    // transition the UV image back into read state
    m_ssr_image_outputs[frame_index][0].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures

    // put sample image in writeable state
    m_ssr_image_outputs[frame_index][1].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);
    // put radius image in writeable state
    m_ssr_radius_output[frame_index].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_sample->GetPipeline()->Bind(primary, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_ssr_sample->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_ssr_sample->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData)) // light unused here
                }
            }
        }
    );

    m_ssr_sample->GetPipeline()->Dispatch(primary, mipmapped_result.GetExtent() / Extent3D{8, 8, 1});

    // transition sample image back into read state
    m_ssr_image_outputs[frame_index][1].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);
    // transition radius image back into read state
    m_ssr_radius_output[frame_index].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    // PASS 3 - blur image using radii in output from previous stage

    //put blur image in writeable state
    m_ssr_image_outputs[frame_index][2].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_blur_hor->GetPipeline()->Bind(primary, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_ssr_blur_hor->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_ssr_blur_hor->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData)) // light unused here
                }
            }
        }
    );

    m_ssr_blur_hor->GetPipeline()->Dispatch(primary, mipmapped_result.GetExtent() / Extent3D{8, 8, 1});

    // transition blur image back into read state
    m_ssr_image_outputs[frame_index][2].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);


    // PASS 4 - blur image vertically

    //put blur image in writeable state
    m_ssr_image_outputs[frame_index][3].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_blur_vert->GetPipeline()->Bind(primary, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_ssr_blur_vert->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_ssr_blur_vert->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData)) // light unused here
                }
            }
        }
    );

    m_ssr_blur_vert->GetPipeline()->Dispatch(primary, mipmapped_result.GetExtent() / Extent3D{8, 8, 1});

    // transition blur image back into read state
    m_ssr_image_outputs[frame_index][3].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);


    /* ==========  END SSR  ========== */

    m_post_processing.RenderPre(engine, frame);

    // begin shading
    m_direct_pass.GetFramebuffer(frame_index)->BeginCapture(primary);

    // indirect shading
    HYPERION_ASSERT_RESULT(m_indirect_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));

    // direct shading
    if (!engine->render_state.light_ids.Empty()) {
        HYPERION_ASSERT_RESULT(m_direct_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));
    }

    // begin translucent with forward rendering
    RenderTranslucentObjects(engine, frame);

    // end shading
    m_direct_pass.GetFramebuffer(frame_index)->EndCapture(primary);

    /* ========== BEGIN MIP CHAIN GENERATION ========== */
    auto *framebuffer_image = m_direct_pass.GetFramebuffer(frame_index)->GetFramebuffer()//bucket.GetFramebuffers()[frame_index]->GetFramebuffer()
        .GetAttachmentRefs()[0]->GetAttachment()->GetImage();
    
    framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_SRC);
    mipmapped_result.GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    mipmapped_result.Blit(
        primary,
        framebuffer_image,
        Rect { 0, 0, framebuffer_image->GetExtent().width, framebuffer_image->GetExtent().height },
        Rect { 0, 0, mipmapped_result.GetExtent().width, mipmapped_result.GetExtent().height }
    );

    HYPERION_ASSERT_RESULT(mipmapped_result.GenerateMipmaps(engine->GetDevice(), primary));

    framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    /* ==========  END MIP CHAIN GENERATION ========== */

    m_post_processing.RenderPost(engine, frame);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

} // namespace hyperion::v2
