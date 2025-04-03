/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderTexture.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderMesh.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Texture.hpp>
#include <scene/Mesh.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateTexture) : renderer::RenderCommand
{
    WeakHandle<Texture>                     texture_weak;
    TResourceHandle<StreamedTextureData>    streamed_texture_data_handle;
    renderer::ResourceState                 initial_state;
    ImageRef                                image;
    ImageViewRef                            image_view;

    RENDER_COMMAND(CreateTexture)(
        const WeakHandle<Texture> &texture_weak,
        TResourceHandle<StreamedTextureData> &&streamed_texture_data_handle,
        renderer::ResourceState initial_state,
        ImageRef image,
        ImageViewRef image_view
    ) : texture_weak(texture_weak),
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
        if (Handle<Texture> texture = texture_weak.Lock()) {
            if (!image->IsCreated()) {
                image->SetIsRWTexture(texture->IsRWTexture());

                if (streamed_texture_data_handle && streamed_texture_data_handle->GetBufferSize() != 0) {
                    AssertThrowMsg(streamed_texture_data_handle->GetTextureData().buffer.Size() == streamed_texture_data_handle->GetBufferSize(),
                        "Streamed texture data buffer size mismatch! Expected: %u, Actual: %u (HashCode: %llu)",
                        streamed_texture_data_handle->GetBufferSize(), streamed_texture_data_handle->GetTextureData().buffer.Size(),
                        streamed_texture_data_handle->GetDataHashCode().Value());

                    HYPERION_BUBBLE_ERRORS(image->Create(g_engine->GetGPUDevice(), initial_state, streamed_texture_data_handle->GetTextureData()));
                } else {
                    HYPERION_BUBBLE_ERRORS(image->Create(g_engine->GetGPUDevice(), initial_state));
                }
            }

            if (!image_view->IsCreated()) {
                HYPERION_BUBBLE_ERRORS(image_view->Create(g_engine->GetGPUInstance()->GetDevice(), image.Get()));
            }
            
            if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
                g_engine->GetRenderData()->textures.AddResource(texture.GetID(), image_view);
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyTexture) : renderer::RenderCommand
{
    // Keep weak handle around to ensure the ID persists
    WeakHandle<Texture> texture;

    RENDER_COMMAND(DestroyTexture)(const WeakHandle<Texture> &texture)
        : texture(texture)
    {
        AssertThrow(texture.IsValid());
    }

    virtual ~RENDER_COMMAND(DestroyTexture)() override = default;

    virtual RendererResult operator()() override
    {
        // If shutting down, skip removing the resource here,
        // render data may have already been destroyed
        if (g_engine->IsShuttingDown()) {
            HYPERION_RETURN_OK;
        }

        if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            g_engine->GetRenderData()->textures.RemoveResource(texture.GetID());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateMipImageView) : renderer::RenderCommand
{
    ImageRef        src_image;
    ImageViewRef    mip_image_view;
    uint32          mip_level;

    RENDER_COMMAND(CreateMipImageView)(
        ImageRef src_image,
        ImageViewRef mip_image_view,
        uint32 mip_level
    ) : src_image(std::move(src_image)),
        mip_image_view(std::move(mip_image_view)),
        mip_level(mip_level)
    {
    }

    virtual RendererResult operator()() override
    {
        return mip_image_view->Create(
            g_engine->GetGPUDevice(),
            src_image.Get(),
            mip_level, 1,
            0, src_image->NumFaces()
        );
    }
};

struct RENDER_COMMAND(RenderTextureMipmapLevels) : renderer::RenderCommand
{
    ImageRef                    m_image;
    ImageViewRef                m_image_view;
    Array<ImageViewRef>         m_mip_image_views;
    Array<RC<FullScreenPass>>   m_passes;

    RENDER_COMMAND(RenderTextureMipmapLevels)(
        ImageRef image,
        ImageViewRef image_view,
        Array<ImageViewRef> mip_image_views,
        Array<RC<FullScreenPass>> passes
    ) : m_image(std::move(image)),
        m_image_view(std::move(image_view)),
        m_mip_image_views(std::move(mip_image_views)),
        m_passes(std::move(passes))
    {
        AssertThrow(m_image != nullptr);
        AssertThrow(m_image_view != nullptr);

        AssertThrow(m_passes.Size() == m_mip_image_views.Size());

        for (SizeType index = 0; index < m_mip_image_views.Size(); index++) {
            AssertThrow(m_mip_image_views[index] != nullptr);
            AssertThrow(m_passes[index] != nullptr);
        }
    }

    virtual RendererResult operator()() override
    {
        // draw a quad for each level
        renderer::SingleTimeCommands commands { g_engine->GetGPUDevice() };

        commands.Push([this](const CommandBufferRef &command_buffer)
        {
            const Vec3u extent = m_image->GetExtent();

            uint32 mip_width = extent.x,
                mip_height = extent.y;

            ImageRef &dst_image = m_image;

            for (uint32 mip_level = 0; mip_level < uint32(m_mip_image_views.Size()); mip_level++) {
                RC<FullScreenPass> &pass = m_passes[mip_level];
                AssertThrow(pass != nullptr);

                const uint32 prev_mip_width = mip_width,
                    prev_mip_height = mip_height;

                mip_width = MathUtil::Max(1u, extent.x >> (mip_level));
                mip_height = MathUtil::Max(1u, extent.y >> (mip_level));

                struct alignas(128)
                {
                    Vec4u   dimensions;
                    Vec4u   prev_dimensions;
                    uint32  mip_level;
                } push_constants;

                push_constants.dimensions = { mip_width, mip_height, 0, 0 };
                push_constants.prev_dimensions = { prev_mip_width, prev_mip_height, 0, 0 };
                push_constants.mip_level = mip_level;

                {
                    Frame temp_frame = Frame::TemporaryFrame(command_buffer);

                    pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
                    pass->Begin(&temp_frame);

                    pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind(
                        &temp_frame,
                        pass->GetRenderGroup()->GetPipeline(),
                        { }
                    );
                    
                    pass->GetQuadMesh()->GetRenderResource().Render(pass->GetCommandBuffer(temp_frame.GetFrameIndex()));
                    pass->End(&temp_frame);
                }

                const ImageRef &src_image = pass->GetAttachment(0)->GetImage();
                const ByteBuffer src_image_byte_buffer = src_image->ReadBack(g_engine->GetGPUDevice(), g_engine->GetGPUInstance());

                // Blit into mip level
                dst_image->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::COPY_DST
                );

                src_image->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::COPY_SRC
                );

                HYPERION_BUBBLE_ERRORS(
                    dst_image->Blit(
                        command_buffer,
                        src_image.Get(),
                        Rect<uint32> {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = src_image->GetExtent().x,
                            .y1 = src_image->GetExtent().y
                        },
                        Rect<uint32> {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = dst_image->GetExtent().x,
                            .y1 = dst_image->GetExtent().y
                        }
                    )
                );

                src_image->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::SHADER_RESOURCE
                );

                // put this mip into readable state
                dst_image->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::SHADER_RESOURCE
                );
            }

            // all mip levels have been transitioned into this state
            dst_image->SetResourceState(
                renderer::ResourceState::SHADER_RESOURCE
            );

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
            ShaderProperties()
        );

        for (uint32 mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

            DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

            const uint32 mip_width = MathUtil::Max(1u, extent.x >> mip_level);
            const uint32 mip_height = MathUtil::Max(1u, extent.y >> mip_level);

            ImageViewRef mip_image_view = MakeRenderObject<ImageView>();
            PUSH_RENDER_COMMAND(CreateMipImageView, m_image, mip_image_view, mip_level);

            const DescriptorSetRef &generate_mipmaps_descriptor_set = descriptor_table->GetDescriptorSet(NAME("GenerateMipmapsDescriptorSet"), 0);
            AssertThrow(generate_mipmaps_descriptor_set != nullptr);
            generate_mipmaps_descriptor_set->SetElement(NAME("InputTexture"), mip_level == 0 ? m_image_view : m_mip_image_views[mip_level - 1]);
            DeferCreate(descriptor_table, g_engine->GetGPUDevice());

            m_mip_image_views[mip_level] = std::move(mip_image_view);

            {
                RC<FullScreenPass> pass = MakeRefCountedPtr<FullScreenPass>(
                    shader,
                    descriptor_table,
                    m_image->GetTextureFormat(),
                    Vec2u { mip_width, mip_height }
                );

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
            m_passes
        );
    }

private:
    ImageRef                    m_image;
    ImageViewRef                m_image_view;
    Array<ImageViewRef>         m_mip_image_views;
    Array<RC<FullScreenPass>>   m_passes;
};

#pragma endregion TextureMipmapRenderer

#pragma region TextureRenderResource

TextureRenderResource::TextureRenderResource(Texture *texture)
    : m_texture(texture),
      m_image(MakeRenderObject<Image>(texture->GetTextureDesc())),
      m_image_view(MakeRenderObject<ImageView>())
{
}

TextureRenderResource::~TextureRenderResource()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));
}

void TextureRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    PUSH_RENDER_COMMAND(
        CreateTexture,
        m_texture->WeakHandleFromThis(),
        m_texture->GetStreamedTextureData() ? TResourceHandle<StreamedTextureData>(*m_texture->GetStreamedTextureData()) : TResourceHandle<StreamedTextureData>(),
        renderer::ResourceState::SHADER_RESOURCE,
        m_image,
        m_image_view
    );
}

void TextureRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    PUSH_RENDER_COMMAND(DestroyTexture, m_texture->WeakHandleFromThis());
}

void TextureRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

void TextureRenderResource::RenderMipmaps()
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

void TextureRenderResource::Readback(ByteBuffer &out_byte_buffer)
{
    HYP_SCOPE;

    Execute([this, &out_byte_buffer]()
    {
        Threads::AssertOnThread(g_render_thread);

        GPUBufferRef gpu_buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);
        HYPERION_ASSERT_RESULT(gpu_buffer->Create(g_engine->GetGPUDevice(), m_image->GetByteSize()));
        gpu_buffer->Map(g_engine->GetGPUDevice());
        gpu_buffer->SetResourceState(renderer::ResourceState::COPY_DST);

        renderer::SingleTimeCommands commands { g_engine->GetGPUDevice() };

        commands.Push([this, &gpu_buffer](const CommandBufferRef &command_buffer)
        {
            const renderer::ResourceState previous_resource_state = m_image->GetResourceState();

            m_image->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

            gpu_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);
            m_image->CopyToBuffer(command_buffer, gpu_buffer);
            gpu_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

            m_image->InsertBarrier(command_buffer, previous_resource_state);

            HYPERION_RETURN_OK;
        });

        RendererResult result = commands.Execute();
        AssertThrowMsg(!result.HasError(), "Failed to readback texture: %s", result.GetError().GetMessage().Data());

        out_byte_buffer.SetSize(gpu_buffer->Size());
        gpu_buffer->Read(g_engine->GetGPUDevice(), out_byte_buffer.Size(), out_byte_buffer.Data());

        gpu_buffer->Destroy(g_engine->GetGPUDevice());
    }, /* force_owner_thread */ true);
}

GPUBufferHolderBase *TextureRenderResource::GetGPUBufferHolder() const
{
    return nullptr;
}

#pragma endregion TextureRenderResource

} // namespace hyperion
