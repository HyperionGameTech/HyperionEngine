/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Texture.hpp>
#include <scene/Mesh.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateTexture)
    : RenderCommand
{
    WeakHandle<Texture> texture_weak;
    TResourceHandle<StreamedTextureData> streamed_texture_data_handle;
    ResourceState initial_state;
    ImageRef image;
    ImageViewRef image_view;

    RENDER_COMMAND(CreateTexture)(
        const WeakHandle<Texture>& texture_weak,
        TResourceHandle<StreamedTextureData>&& streamed_texture_data_handle,
        ResourceState initial_state,
        ImageRef image,
        ImageViewRef image_view)
        : texture_weak(texture_weak),
          streamed_texture_data_handle(std::move(streamed_texture_data_handle)),
          initial_state(initial_state),
          image(std::move(image)),
          image_view(std::move(image_view))
    {
        AssertThrow(this->image.IsValid());
        AssertThrow(this->image_view.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateTexture)() override = default;

    virtual RendererResult operator()() override
    {
        if (Handle<Texture> texture = texture_weak.Lock())
        {
            if (!image->IsCreated())
            {
                HYPERION_BUBBLE_ERRORS(image->Create());

                if (streamed_texture_data_handle && streamed_texture_data_handle->GetBufferSize() != 0)
                {
                    const TextureData& texture_data = streamed_texture_data_handle->GetTextureData();
                    const TextureDesc& texture_desc = streamed_texture_data_handle->GetTextureDesc();

                    AssertThrowMsg(texture_data.buffer.Size() == texture_desc.GetByteSize(),
                        "Streamed texture data buffer size mismatch in CreateTexture! Texture Id: %u, Texture name: %s, Expected: %u, Actual: %u (HashCode: %llu)",
                        texture->GetID().Value(), texture->GetName().LookupString(),
                        texture_desc.GetByteSize(), texture_data.buffer.Size(), streamed_texture_data_handle->GetDataHashCode().Value());

                    AssertThrowMsg(streamed_texture_data_handle->GetTextureData().buffer.Size() == streamed_texture_data_handle->GetBufferSize(),
                        "Streamed texture data buffer size mismatch! Expected: %u, Actual: %u (HashCode: %llu)",
                        streamed_texture_data_handle->GetBufferSize(), streamed_texture_data_handle->GetTextureData().buffer.Size(),
                        streamed_texture_data_handle->GetDataHashCode().Value());

                    GpuBufferRef staging_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, texture_data.buffer.Size());
                    HYPERION_BUBBLE_ERRORS(staging_buffer->Create());
                    staging_buffer->Copy(texture_data.buffer.Size(), texture_data.buffer.Data());

                    HYP_DEFER({ SafeRelease(std::move(staging_buffer)); });

                    // @FIXME add back ConvertTo32BPP

                    SingleTimeCommands commands;

                    commands.Push([&](CmdList& cmd)
                        {
                            cmd.Add<InsertBarrier>(
                                image,
                                RS_COPY_DST);

                            cmd.Add<CopyBufferToImage>(staging_buffer, image);

                            if (texture_data.desc.HasMipmaps())
                            {
                                cmd.Add<GenerateMipmaps>(image);
                            }

                            cmd.Add<InsertBarrier>(
                                image,
                                initial_state);
                        });

                    if (RendererResult result = commands.Execute(); result.HasError())
                    {
                        return result;
                    }
                }
                else if (initial_state != RS_UNDEFINED)
                {
                    SingleTimeCommands commands;

                    commands.Push([&](CmdList& cmd)
                        {
                            // Transition to initial state
                            cmd.Add<InsertBarrier>(image, initial_state);
                        });

                    if (RendererResult result = commands.Execute(); result.HasError())
                    {
                        return result;
                    }
                }
            }

            if (!image_view->IsCreated())
            {
                HYPERION_BUBBLE_ERRORS(image_view->Create());
            }

            if (g_render_backend->GetRenderConfig().IsBindlessSupported())
            {
                g_render_global_state->BindlessTextures.AddResource(texture.GetID(), image_view);
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyTexture)
    : RenderCommand
{
    // Keep weak handle around to ensure the Id persists
    WeakHandle<Texture> texture;

    RENDER_COMMAND(DestroyTexture)(const WeakHandle<Texture>& texture)
        : texture(texture)
    {
        AssertThrow(texture.IsValid());
    }

    virtual ~RENDER_COMMAND(DestroyTexture)() override = default;

    virtual RendererResult operator()() override
    {
        // If shutting down, skip removing the resource here,
        // render data may have already been destroyed
        if (g_engine->IsShuttingDown())
        {
            HYPERION_RETURN_OK;
        }

        if (g_render_backend->GetRenderConfig().IsBindlessSupported())
        {
            g_render_global_state->BindlessTextures.RemoveResource(texture.GetID());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RenderTextureMipmapLevels)
    : RenderCommand
{
    ImageRef m_image;
    ImageViewRef m_image_view;
    Array<ImageViewRef> m_mip_image_views;
    Array<RC<FullScreenPass>> m_passes;

    RENDER_COMMAND(RenderTextureMipmapLevels)(
        ImageRef image,
        ImageViewRef image_view,
        Array<ImageViewRef> mip_image_views,
        Array<RC<FullScreenPass>> passes)
        : m_image(std::move(image)),
          m_image_view(std::move(image_view)),
          m_mip_image_views(std::move(mip_image_views)),
          m_passes(std::move(passes))
    {
        AssertThrow(m_image != nullptr);
        AssertThrow(m_image_view != nullptr);

        AssertThrow(m_passes.Size() == m_mip_image_views.Size());

        for (SizeType index = 0; index < m_mip_image_views.Size(); index++)
        {
            AssertThrow(m_mip_image_views[index] != nullptr);
            AssertThrow(m_passes[index] != nullptr);
        }
    }

    virtual RendererResult operator()() override
    {
        // draw a quad for each level
        SingleTimeCommands commands;

        commands.Push([this](CmdList& cmd)
            {
                const Vec3u extent = m_image->GetExtent();

                uint32 mip_width = extent.x,
                       mip_height = extent.y;

                ImageRef& dst_image = m_image;

                for (uint32 mip_level = 0; mip_level < uint32(m_mip_image_views.Size()); mip_level++)
                {
                    RC<FullScreenPass>& pass = m_passes[mip_level];
                    AssertThrow(pass != nullptr);

                    const uint32 prev_mip_width = mip_width,
                                 prev_mip_height = mip_height;

                    mip_width = MathUtil::Max(1u, extent.x >> (mip_level));
                    mip_height = MathUtil::Max(1u, extent.y >> (mip_level));

                    struct
                    {
                        Vec4u dimensions;
                        Vec4u prev_dimensions;
                        uint32 mip_level;
                    } push_constants;

                    push_constants.dimensions = { mip_width, mip_height, 0, 0 };
                    push_constants.prev_dimensions = { prev_mip_width, prev_mip_height, 0, 0 };
                    push_constants.mip_level = mip_level;

                    {
                        FrameRef temp_frame = g_render_backend->MakeFrame(0);

                        pass->GetGraphicsPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
                        pass->Begin(temp_frame, NullRenderSetup());

                        temp_frame->GetCommandList().Add<BindDescriptorTable>(
                            pass->GetGraphicsPipeline()->GetDescriptorTable(),
                            pass->GetGraphicsPipeline(),
                            ArrayMap<Name, ArrayMap<Name, uint32>> {},
                            temp_frame->GetFrameIndex());

                        pass->GetQuadMesh()->GetRenderResource().Render(temp_frame->GetCommandList());
                        pass->End(temp_frame, NullRenderSetup());

                        cmd.Concat(std::move(temp_frame->GetCommandList()));

                        HYPERION_ASSERT_RESULT(temp_frame->Destroy());
                    }

                    const ImageRef& src_image = pass->GetAttachment(0)->GetImage();

                    // Blit into mip level
                    cmd.Add<InsertBarrier>(
                        dst_image,
                        RS_COPY_DST,
                        ImageSubResource { .base_mip_level = mip_level });

                    cmd.Add<InsertBarrier>(
                        src_image,
                        RS_COPY_SRC,
                        ImageSubResource { .base_mip_level = mip_level });

                    cmd.Add<BlitRect>(
                        src_image,
                        dst_image,
                        Rect<uint32> {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = src_image->GetExtent().x,
                            .y1 = src_image->GetExtent().y },
                        Rect<uint32> {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = dst_image->GetExtent().x,
                            .y1 = dst_image->GetExtent().y });

                    cmd.Add<InsertBarrier>(
                        src_image,
                        RS_SHADER_RESOURCE,
                        ImageSubResource { .base_mip_level = mip_level });

                    cmd.Add<InsertBarrier>(
                        dst_image,
                        RS_SHADER_RESOURCE,
                        ImageSubResource { .base_mip_level = mip_level });
                }

                // all mip levels have been transitioned into this state
                cmd.Add<InsertBarrier>(
                    dst_image,
                    RS_SHADER_RESOURCE);

                HYPERION_RETURN_OK;
            });

        return commands.Execute();
    }
};

#pragma endregion Render commands

#pragma region TextureMipmapRenderer

class TextureMipmapRenderer // Come back to this for: UI rendering (caching each object as its own texture)
{
public:
    TextureMipmapRenderer(ImageRef image, ImageViewRef image_view)
        : m_image(std::move(image)),
          m_image_view(std::move(image_view))
    {
    }

    void Create()
    {
        const uint32 num_mip_levels = m_image->NumMipmaps();

        m_mip_image_views.Resize(num_mip_levels);
        m_passes.Resize(num_mip_levels);

        const Vec3u extent = m_image->GetExtent();

        ShaderRef shader = g_shader_manager->GetOrCreate(
            NAME("GenerateMipmaps"),
            ShaderProperties());

        for (uint32 mip_level = 0; mip_level < num_mip_levels; mip_level++)
        {
            const DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

            DescriptorTableRef descriptor_table = g_render_backend->MakeDescriptorTable(&descriptor_table_decl);

            const uint32 mip_width = MathUtil::Max(1u, extent.x >> mip_level);
            const uint32 mip_height = MathUtil::Max(1u, extent.y >> mip_level);

            ImageViewRef mip_image_view = g_render_backend->MakeImageView(m_image, mip_level, 1, 0, m_image->NumFaces());
            DeferCreate(mip_image_view);

            const DescriptorSetRef& generate_mipmaps_descriptor_set = descriptor_table->GetDescriptorSet(NAME("GenerateMipmapsDescriptorSet"), 0);
            AssertThrow(generate_mipmaps_descriptor_set != nullptr);
            generate_mipmaps_descriptor_set->SetElement(NAME("InputTexture"), mip_level == 0 ? m_image_view : m_mip_image_views[mip_level - 1]);
            DeferCreate(descriptor_table);

            m_mip_image_views[mip_level] = std::move(mip_image_view);

            {
                RC<FullScreenPass> pass = MakeRefCountedPtr<FullScreenPass>(
                    shader,
                    descriptor_table,
                    m_image->GetTextureFormat(),
                    Vec2u { mip_width, mip_height },
                    nullptr);

                pass->Create();

                m_passes[mip_level] = std::move(pass);
            }
        }
    }

    void Destroy()
    {
        m_passes.Clear();

        SafeRelease(std::move(m_image));
        SafeRelease(std::move(m_image_view));
        SafeRelease(std::move(m_mip_image_views));
    }

    void Render()
    {
        PUSH_RENDER_COMMAND(
            RenderTextureMipmapLevels,
            m_image,
            m_image_view,
            m_mip_image_views,
            m_passes);
    }

private:
    ImageRef m_image;
    ImageViewRef m_image_view;
    Array<ImageViewRef> m_mip_image_views;
    Array<RC<FullScreenPass>> m_passes;
};

#pragma endregion TextureMipmapRenderer

#pragma region RenderTexture

RenderTexture::RenderTexture(Texture* texture)
    : m_texture(texture),
      m_image(g_render_backend->MakeImage(texture->GetTextureDesc())),
      m_image_view(g_render_backend->MakeImageView(m_image))
{
}

RenderTexture::~RenderTexture()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));
}

void RenderTexture::Initialize_Internal()
{
    HYP_SCOPE;

    PUSH_RENDER_COMMAND(
        CreateTexture,
        m_texture->WeakHandleFromThis(),
        m_texture->GetStreamedTextureData() ? TResourceHandle<StreamedTextureData>(*m_texture->GetStreamedTextureData()) : TResourceHandle<StreamedTextureData>(),
        RS_SHADER_RESOURCE,
        m_image,
        m_image_view);
}

void RenderTexture::Destroy_Internal()
{
    HYP_SCOPE;

    PUSH_RENDER_COMMAND(DestroyTexture, m_texture->WeakHandleFromThis());
}

void RenderTexture::Update_Internal()
{
    HYP_SCOPE;
}

void RenderTexture::RenderMipmaps()
{
    HYP_SCOPE;

    Execute([this]()
        {
            TextureMipmapRenderer mipmap_renderer(m_image, m_image_view);
            mipmap_renderer.Create();
            mipmap_renderer.Render();
            mipmap_renderer.Destroy();
        });
}

void RenderTexture::Readback(ByteBuffer& out_byte_buffer)
{
    HYP_SCOPE;

    Task<Result> task;

    if (!IsInitialized())
    {
        task.Fulfill(HYP_MAKE_ERROR(Error, "RenderTexture is not initialized, cannot readback texture data"));

        return;
    }

    Execute([this, &out_byte_buffer, promise = task.Promise()]()
        {
            Threads::AssertOnThread(g_render_thread);

            GpuBufferRef gpu_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, m_image->GetByteSize());
            HYPERION_ASSERT_RESULT(gpu_buffer->Create());
            gpu_buffer->Map();

            SingleTimeCommands commands;

            commands.Push([this, &gpu_buffer](CmdList& cmd)
                {
                    const ResourceState previous_resource_state = m_image->GetResourceState();

                    cmd.Add<InsertBarrier>(m_image, RS_COPY_SRC);
                    cmd.Add<InsertBarrier>(gpu_buffer, RS_COPY_DST);

                    cmd.Add<CopyImageToBuffer>(m_image, gpu_buffer);

                    cmd.Add<InsertBarrier>(gpu_buffer, RS_COPY_SRC);
                    cmd.Add<InsertBarrier>(m_image, previous_resource_state);
                });

            RendererResult result = commands.Execute();

            if (result.HasError())
            {
                promise->Fulfill(result.GetError());

                return;
            }

            out_byte_buffer.SetSize(gpu_buffer->Size());
            gpu_buffer->Read(out_byte_buffer.Size(), out_byte_buffer.Data());

            gpu_buffer->Destroy();

            promise->Fulfill(Result());
        },
        /* force_owner_thread */ true);

    Result result = task.Await();

    if (result.HasError())
    {
        HYP_FAIL("Failed to readback texture! %s", result.GetError().GetMessage().Data());
    }
}

void RenderTexture::Resize(const Vec3u& extent)
{
    HYP_SCOPE;

    TextureDesc texture_desc = m_texture->GetTextureDesc();

    Execute([this, extent, texture_desc]()
        {
            // HYPERION_ASSERT_RESULT(m_image->Resize(extent));

            // if (m_image_view->IsCreated())
            // {
            //     m_image_view->Destroy();
            //     HYPERION_ASSERT_RESULT(m_image_view->Create());
            // }

            SafeRelease(std::move(m_image));
            SafeRelease(std::move(m_image_view));

            m_image = g_render_backend->MakeImage(texture_desc);
            HYPERION_ASSERT_RESULT(m_image->Create());

            m_image_view = g_render_backend->MakeImageView(m_image);
            HYPERION_ASSERT_RESULT(m_image_view->Create());
        },
        /* force_owner_thread */ true);
}

GpuBufferHolderBase* RenderTexture::GetGpuBufferHolder() const
{
    return nullptr;
}

#pragma endregion RenderTexture

} // namespace hyperion
