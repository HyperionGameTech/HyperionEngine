#include "ScreenspaceReflectionRenderer.hpp"
#include "../Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;

ScreenspaceReflectionRenderer::ScreenspaceReflectionRenderer(const Extent2D &extent)
    : m_extent(extent),
      m_is_rendered(false)
{
}

ScreenspaceReflectionRenderer::~ScreenspaceReflectionRenderer()
{
}

void ScreenspaceReflectionRenderer::Create(Engine *engine)
{
    CreateComputePipelines(engine);
    
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (auto &image_output : m_ssr_image_outputs[i]) {
            image_output = SSRImageOutput {
                .image = std::make_unique<StorageImage>(
                    Extent3D(m_extent),
                    Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                    Image::Type::TEXTURE_TYPE_2D,
                    nullptr
                ),
                .image_view = std::make_unique<ImageView>()
            };

            image_output.Create(engine->GetDevice());
        }

        m_ssr_radius_output[i] = SSRImageOutput {
            .image = std::make_unique<StorageImage>(
                Extent3D(m_extent),
                Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
                Image::Type::TEXTURE_TYPE_2D,
                nullptr
            ),
            .image_view = std::make_unique<ImageView>()
        };

        m_ssr_radius_output[i].Create(engine->GetDevice());
    }

    CreateDescriptors(engine);
}

void ScreenspaceReflectionRenderer::Destroy(Engine *engine)
{
    m_is_rendered = false;

    m_ssr_write_uvs.Reset();
    m_ssr_sample.Reset();
    m_ssr_blur_hor.Reset();
    m_ssr_blur_vert.Reset();

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (UInt j = 0; j < static_cast<UInt>(m_ssr_image_outputs[i].size()); j++) {
            m_ssr_image_outputs[i][j].Destroy(engine->GetDevice());
        }

        m_ssr_radius_output[i].Destroy(engine->GetDevice());
    }
}

void ScreenspaceReflectionRenderer::CreateDescriptors(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

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
}

void ScreenspaceReflectionRenderer::CreateComputePipelines(Engine *engine)
{
    m_ssr_write_uvs = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/ssr/ssr_write_uvs.comp.spv")).Read()}}
            }
        )
    );

    engine->InitObject(m_ssr_write_uvs);

    m_ssr_sample = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/ssr/ssr_sample.comp.spv")).Read()}}
            }
        )
    );

    engine->InitObject(m_ssr_sample);

    m_ssr_blur_hor = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/ssr/ssr_blur_hor.comp.spv")).Read()}}
            }
        )
    );

    engine->InitObject(m_ssr_blur_hor);

    m_ssr_blur_vert = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/ssr/ssr_blur_vert.comp.spv")).Read()}}
            }
        )
    );

    engine->InitObject(m_ssr_blur_vert);
}

void ScreenspaceReflectionRenderer::Render(
    Engine *engine,
    Frame *frame
)
{
    const auto &scene_binding = engine->render_state.GetScene();
    const auto scene_index = scene_binding ? scene_binding.id.value - 1 : 0;

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    DebugMarker begin_ssr_marker(command_buffer, "Begin SSR");
    
    // PASS 1 -- write UVs

    // start by putting the UV image in a writeable state
    const Pipeline::PushConstantData ssr_push_constant_data {
        .ssr_data = {
            .width                  = m_extent.width,
            .height                 = m_extent.height,
            .ray_step               = 1.8f,
            .num_iterations         = 128.0f,
            .max_ray_distance       = 256.0f,
            .distance_bias          = 0.15f,
            .offset                 = 0.01f,
            .eye_fade_start         = 0.95f,
            .eye_fade_end           = 0.98f,
            .screen_edge_fade_start = 0.65f,
            .screen_edge_fade_end   = 0.75f
        }
    };

    m_ssr_image_outputs[frame_index][0].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_write_uvs->GetPipeline()->Bind(command_buffer, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_write_uvs->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_write_uvs->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    HYP_RENDER_OBJECT_OFFSET(Light, 0)
                }
            }
        }
    );

    m_ssr_write_uvs->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition the UV image back into read state
    m_ssr_image_outputs[frame_index][0].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures

    // put sample image in writeable state
    m_ssr_image_outputs[frame_index][1].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);
    // put radius image in writeable state
    m_ssr_radius_output[frame_index].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_sample->GetPipeline()->Bind(command_buffer, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_sample->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_sample->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    HYP_RENDER_OBJECT_OFFSET(Light, 0)
                }
            }
        }
    );

    m_ssr_sample->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition sample image back into read state
    m_ssr_image_outputs[frame_index][1].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);
    // transition radius image back into read state
    m_ssr_radius_output[frame_index].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    // PASS 3 - blur image using radii in output from previous stage

    //put blur image in writeable state
    m_ssr_image_outputs[frame_index][2].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_blur_hor->GetPipeline()->Bind(command_buffer, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_blur_hor->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_blur_hor->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    HYP_RENDER_OBJECT_OFFSET(Light, 0)
                }
            }
        }
    );

    m_ssr_blur_hor->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition blur image back into read state
    m_ssr_image_outputs[frame_index][2].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);


    // PASS 4 - blur image vertically

    //put blur image in writeable state
    m_ssr_image_outputs[frame_index][3].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_blur_vert->GetPipeline()->Bind(command_buffer, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_blur_vert->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_blur_vert->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    HYP_RENDER_OBJECT_OFFSET(Light, 0)
                }
            }
        }
    );

    m_ssr_blur_vert->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition blur image back into read state
    m_ssr_image_outputs[frame_index][3].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);


    m_is_rendered = true;
    /* ==========  END SSR  ========== */
}

} // namespace hyperion::v2
