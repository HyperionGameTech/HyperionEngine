/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(SetDepthPyramidInGlobalDescriptorSet) : renderer::RenderCommand
{
    ImageViewRef    depth_pyramid_image_view;

    RENDER_COMMAND(SetDepthPyramidInGlobalDescriptorSet)(
        const ImageViewRef &depth_pyramid_image_view
    ) : depth_pyramid_image_view(depth_pyramid_image_view)
    {
        AssertThrow(depth_pyramid_image_view.IsValid());
    }

    virtual ~RENDER_COMMAND(SetDepthPyramidInGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DepthPyramidResult"), depth_pyramid_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetDepthPyramidInGlobalDescriptorSet) : renderer::RenderCommand
{
    virtual ~RENDER_COMMAND(UnsetDepthPyramidInGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DepthPyramidResult"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

DepthPyramidRenderer::DepthPyramidRenderer()
    : m_is_rendered(false)
{
}

DepthPyramidRenderer::~DepthPyramidRenderer()
{
    PUSH_RENDER_COMMAND(UnsetDepthPyramidInGlobalDescriptorSet);

    SafeRelease(std::move(m_depth_pyramid));
    SafeRelease(std::move(m_depth_pyramid_view));
    SafeRelease(std::move(m_depth_pyramid_mips));

    for (auto &mip_descriptor_table : m_mip_descriptor_tables) {
        SafeRelease(std::move(mip_descriptor_table));
    }

    SafeRelease(std::move(m_depth_pyramid_sampler));
    SafeRelease(std::move(m_depth_attachment));

    SafeRelease(std::move(m_generate_depth_pyramid));
}

void DepthPyramidRenderer::Create()
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    auto CreateDepthPyramidResources = [this]()
    {
        HYP_NAMED_SCOPE("Create depth pyramid resources");
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        m_depth_attachment = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(BUCKET_OPAQUE).GetFramebuffer()->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
        AssertThrow(m_depth_attachment.IsValid());

        m_depth_pyramid_sampler = MakeRenderObject<Sampler>(
            FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        );

        HYPERION_ASSERT_RESULT(m_depth_pyramid_sampler->Create(g_engine->GetGPUDevice()));
    
        const ImageRef &depth_image = m_depth_attachment->GetImage();
        AssertThrow(depth_image.IsValid());

        // create depth pyramid image
        m_depth_pyramid = MakeRenderObject<Image>(TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            InternalFormat::R32F,
            Vec3u {
                uint32(MathUtil::NextPowerOf2(depth_image->GetExtent().x)),
                uint32(MathUtil::NextPowerOf2(depth_image->GetExtent().y)),
                1
            },
            FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP,
            FilterMode::TEXTURE_FILTER_NEAREST
        });

        m_depth_pyramid->SetIsRWTexture(true);
        m_depth_pyramid->Create(g_engine->GetGPUDevice());

        m_depth_pyramid_view = MakeRenderObject<ImageView>();
        m_depth_pyramid_view->Create(g_engine->GetGPUDevice(), m_depth_pyramid);

        const uint num_mip_levels = m_depth_pyramid->NumMipmaps();

        m_depth_pyramid_mips.Clear();
        m_depth_pyramid_mips.Reserve(num_mip_levels);

        for (uint mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            ImageViewRef mip_image_view = MakeRenderObject<ImageView>();

            HYPERION_ASSERT_RESULT(mip_image_view->Create(
                g_engine->GetGPUDevice(),
                m_depth_pyramid,
                mip_level, 1,
                0, m_depth_pyramid->NumFaces()
            ));

            m_depth_pyramid_mips.PushBack(std::move(mip_image_view));
        }

        ShaderRef shader = g_shader_manager->GetOrCreate(NAME("GenerateDepthPyramid"), { });
        AssertThrow(shader.IsValid());
        
        const renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        const renderer::DescriptorSetDeclaration *depth_pyramid_descriptor_set_decl = descriptor_table_decl.FindDescriptorSetDeclaration(NAME("DepthPyramidDescriptorSet"));
        AssertThrow(depth_pyramid_descriptor_set_decl != nullptr);

        while (m_mip_descriptor_tables.Size() > num_mip_levels) {
            SafeRelease(m_mip_descriptor_tables.PopFront());
        }

        while (m_mip_descriptor_tables.Size() < num_mip_levels) {
            m_mip_descriptor_tables.PushFront(DescriptorTableRef { });
        }

        for (uint mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            DescriptorTableRef &descriptor_table = m_mip_descriptor_tables[mip_level];

            const auto SetDescriptorSetElements = [&]()
            {
                for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                    const DescriptorSetRef &depth_pyramid_descriptor_set = descriptor_table->GetDescriptorSet(NAME("DepthPyramidDescriptorSet"), frame_index);
                    AssertThrow(depth_pyramid_descriptor_set != nullptr);

                    if (mip_level == 0) {
                        // first mip level -- input is the actual depth image
                        depth_pyramid_descriptor_set->SetElement(NAME("InImage"), m_depth_attachment->GetImageView());
                    } else {
                        AssertThrow(m_depth_pyramid_mips[mip_level - 1] != nullptr);

                        depth_pyramid_descriptor_set->SetElement(NAME("InImage"), m_depth_pyramid_mips[mip_level - 1]);
                    }

                    depth_pyramid_descriptor_set->SetElement(NAME("OutImage"), m_depth_pyramid_mips[mip_level]);
                    depth_pyramid_descriptor_set->SetElement(NAME("DepthPyramidSampler"), m_depth_pyramid_sampler);
                }
            };

            if (!descriptor_table.IsValid()) {
                descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

                SetDescriptorSetElements();

                HYPERION_ASSERT_RESULT(descriptor_table->Create(g_engine->GetGPUDevice()));
            } else {
                SetDescriptorSetElements();

                for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                    HYPERION_ASSERT_RESULT(descriptor_table->Update(g_engine->GetGPUDevice(), frame_index));
                }
            }
        }

        // use the first mip descriptor table to create the compute pipeline, since the descriptor set layout is the same for all mip levels
        m_generate_depth_pyramid = MakeRenderObject<ComputePipeline>(shader, m_mip_descriptor_tables.Front());
        DeferCreate(m_generate_depth_pyramid, g_engine->GetGPUDevice());

        PUSH_RENDER_COMMAND(SetDepthPyramidInGlobalDescriptorSet, m_depth_pyramid_view);
    };

    CreateDepthPyramidResources();

    m_create_depth_pyramid_resources_handler = g_engine->GetDeferredRenderer()->GetGBuffer()->OnGBufferResolutionChanged.Bind([this, CreateDepthPyramidResources](...)
    {
        PUSH_RENDER_COMMAND(UnsetDepthPyramidInGlobalDescriptorSet);

        SafeRelease(std::move(m_depth_pyramid));
        SafeRelease(std::move(m_depth_pyramid_view));
        SafeRelease(std::move(m_depth_pyramid_mips));
        SafeRelease(std::move(m_depth_pyramid_sampler));
        SafeRelease(std::move(m_depth_attachment));

        SafeRelease(std::move(m_generate_depth_pyramid));

        CreateDepthPyramidResources();
    });
}

Vec2u DepthPyramidRenderer::GetExtent() const
{
    if (!m_depth_pyramid.IsValid()) {
        return Vec2u::One();
    }

    const Vec3u extent = m_depth_pyramid->GetExtent();

    return { extent.x, extent.y };
}

void DepthPyramidRenderer::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    DebugMarker marker(command_buffer, "Depth pyramid generation");

    const auto num_depth_pyramid_mip_levels = m_depth_pyramid_mips.Size();

    const Vec3u &image_extent = m_depth_attachment->GetImage()->GetExtent();
    const Vec3u &depth_pyramid_extent = m_depth_pyramid->GetExtent();

    uint32 mip_width = image_extent.x,
        mip_height = image_extent.y;

    for (uint mip_level = 0; mip_level < num_depth_pyramid_mip_levels; mip_level++) {
        // level 0 == write just-rendered depth image into mip 0

        // put the mip into writeable state
        m_depth_pyramid->InsertSubResourceBarrier(
            command_buffer,
            renderer::ImageSubResource { .base_mip_level = mip_level },
            renderer::ResourceState::UNORDERED_ACCESS
        );
        
        const uint32 prev_mip_width = mip_width,
            prev_mip_height = mip_height;

        mip_width = MathUtil::Max(1u, depth_pyramid_extent.x >> (mip_level));
        mip_height = MathUtil::Max(1u, depth_pyramid_extent.y >> (mip_level));

        m_mip_descriptor_tables[mip_level]->Bind(frame, m_generate_depth_pyramid, { });

        struct alignas(128)
        {
            Vec2u   mip_dimensions;
            Vec2u   prev_mip_dimensions;
            uint32  mip_level;
        } depth_pyramid_data;

        depth_pyramid_data.mip_dimensions = { mip_width, mip_height };
        depth_pyramid_data.prev_mip_dimensions = { prev_mip_width, prev_mip_height };
        depth_pyramid_data.mip_level = mip_level;

        m_generate_depth_pyramid->SetPushConstants(&depth_pyramid_data, sizeof(depth_pyramid_data));

        // set push constant data for the current mip level
        m_generate_depth_pyramid->Bind(command_buffer);
        
        // dispatch to generate this mip level
        m_generate_depth_pyramid->Dispatch(
            command_buffer,
            Vec3u {
                (mip_width + 31) / 32,
                (mip_height + 31) / 32,
                1
            }
        );

        // put this mip into readable state
        m_depth_pyramid->InsertSubResourceBarrier(
            command_buffer,
            renderer::ImageSubResource { .base_mip_level = mip_level },
            renderer::ResourceState::SHADER_RESOURCE
        );
    }

    // all mip levels have been transitioned into this state
    m_depth_pyramid->SetResourceState(
        renderer::ResourceState::SHADER_RESOURCE
    );

    m_is_rendered = true;
}

} // namespace hyperion