/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Texture.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererSampler.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Result;

class Texture;
class TextureMipmapRenderer;

const FixedArray<std::pair<Vector3, Vector3>, 6> Texture::cubemap_directions = {
    std::make_pair(Vector3(1, 0, 0), Vector3(0, 1, 0)),
    std::make_pair(Vector3(-1, 0, 0),  Vector3(0, 1, 0)),
    std::make_pair(Vector3(0, 1, 0),  Vector3(0, 0, -1)),
    std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, 1)),
    std::make_pair(Vector3(0, 0, 1), Vector3(0, 1, 0)),
    std::make_pair(Vector3(0, 0, -1),  Vector3(0, 1, 0)),
};

#pragma region Render commands

struct RENDER_COMMAND(CreateTexture) : renderer::RenderCommand
{
    ID<Texture>             id;
    renderer::ResourceState initial_state;
    ImageRef                image;
    ImageViewRef            image_view;

    RENDER_COMMAND(CreateTexture)(
        ID<Texture> id,
        renderer::ResourceState initial_state,
        ImageRef image,
        ImageViewRef image_view
    ) : id(id),
        initial_state(initial_state),
        image(std::move(image)),
        image_view(std::move(image_view))
    {
        AssertThrow(this->image.IsValid());
        AssertThrow(this->image_view.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateTexture)() override = default;

    virtual Result operator()() override
    {
        if (!image->IsCreated()) {
            HYPERION_BUBBLE_ERRORS(image->Create(g_engine->GetGPUDevice(), g_engine->GetGPUInstance(), initial_state));
        }

        if (!image_view->IsCreated()) {
            HYPERION_BUBBLE_ERRORS(image_view->Create(g_engine->GetGPUInstance()->GetDevice(), image.Get()));
        }
        
        if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            g_engine->GetRenderData()->textures.AddResource(id, image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyTexture) : renderer::RenderCommand
{
    ID<Texture>     id;
    ImageRef        image;
    ImageViewRef    image_view;

    RENDER_COMMAND(DestroyTexture)(
        ID<Texture> id,
        ImageRef &&image,
        ImageViewRef &&image_view
    ) : id(id),
        image(std::move(image)),
        image_view(std::move(image_view))
    {
    }

    virtual ~RENDER_COMMAND(DestroyTexture)() override = default;

    virtual Result operator()() override
    {
        // If shutting down, skip removing the resource here,
        // render data may have already been destroyed
        if (g_engine->IsShuttingDown()) {
            HYPERION_RETURN_OK;
        }

        if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            g_engine->GetRenderData()->textures.RemoveResource(id);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateMipImageView) : renderer::RenderCommand
{
    ImageRef        src_image;
    ImageViewRef    mip_image_view;
    uint            mip_level;

    RENDER_COMMAND(CreateMipImageView)(
        ImageRef src_image,
        ImageViewRef mip_image_view,
        uint mip_level
    ) : src_image(std::move(src_image)),
        mip_image_view(std::move(mip_image_view)),
        mip_level(mip_level)
    {
    }

    virtual Result operator()() override
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

    virtual Result operator()() override
    {
        // draw a quad for each level
        auto commands = g_engine->GetGPUInstance()->GetSingleTimeCommands();

        commands.Push([this](const CommandBufferRef &command_buffer)
        {
            const Extent3D extent = m_image->GetExtent();

            uint32 mip_width = extent.width,
                mip_height = extent.height;

            ImageRef &dst_image = m_image;

            for (uint mip_level = 0; mip_level < uint(m_mip_image_views.Size()); mip_level++) {
                auto &pass = m_passes[mip_level];
                AssertThrow(pass != nullptr);

                const uint32 prev_mip_width = mip_width,
                    prev_mip_height = mip_height;

                mip_width = MathUtil::Max(1u, extent.width >> (mip_level));
                mip_height = MathUtil::Max(1u, extent.height >> (mip_level));

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
                    
                    pass->GetQuadMesh()->Render(pass->GetCommandBuffer(temp_frame.GetFrameIndex()));
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
                            .x1 = src_image->GetExtent().width,
                            .y1 = src_image->GetExtent().height
                        },
                        Rect<uint32> {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = dst_image->GetExtent().width,
                            .y1 = dst_image->GetExtent().height
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

        return commands.Execute(g_engine->GetGPUInstance()->GetDevice());
    }
};

#pragma endregion Render commands

#pragma region TextureMipmapRenderer

class TextureMipmapRenderer
{
public:
    TextureMipmapRenderer(ImageRef image, ImageViewRef image_view)
        : m_image(std::move(image)),
          m_image_view(std::move(image_view))
    {
    }

    void Create()
    {
        const uint num_mip_levels = m_image->NumMipmaps();

        m_mip_image_views.Resize(num_mip_levels);
        m_passes.Resize(num_mip_levels);

        const Extent3D extent = m_image->GetExtent();

        ShaderRef shader = g_shader_manager->GetOrCreate(
            NAME("GenerateMipmaps"),
            ShaderProperties()
        );

        for (uint mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

            DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

            const uint32 mip_width = MathUtil::Max(1u, extent.width >> mip_level);
            const uint32 mip_height = MathUtil::Max(1u, extent.height >> mip_level);

            ImageViewRef mip_image_view = MakeRenderObject<ImageView>();
            PUSH_RENDER_COMMAND(CreateMipImageView, m_image, mip_image_view, mip_level);

            const DescriptorSetRef &generate_mipmaps_descriptor_set = descriptor_table->GetDescriptorSet(NAME("GenerateMipmapsDescriptorSet"), 0);
            AssertThrow(generate_mipmaps_descriptor_set != nullptr);
            generate_mipmaps_descriptor_set->SetElement(NAME("InputTexture"), mip_level == 0 ? m_image_view : m_mip_image_views[mip_level - 1]);
            DeferCreate(descriptor_table, g_engine->GetGPUDevice());

            m_mip_image_views[mip_level] = std::move(mip_image_view);

            {
                RC<FullScreenPass> pass(new FullScreenPass(
                    shader,
                    descriptor_table,
                    m_image->GetTextureFormat(),
                    Extent2D { mip_width, mip_height }
                ));

                pass->Create();

                m_passes[mip_level] = std::move(pass);
            }
        }
    }

    void Destroy()
    {
        for (auto &pass : m_passes) {
            pass->Destroy();
        }

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

#pragma region Texture

Texture::Texture() : Texture(
    TextureDesc
    {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA8,
        Extent3D { 1, 1, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    }
)
{
}

Texture::Texture(const TextureDesc &texture_desc) : Texture(
    renderer::Image(texture_desc)
)
{
}

Texture::Texture(const RC<StreamedTextureData> &streamed_data) : Texture(
    renderer::Image(streamed_data)
)
{
}

Texture::Texture(RC<StreamedTextureData> &&streamed_data) : Texture(
    renderer::Image(std::move(streamed_data))
)
{
}

Texture::Texture(
    ImageRef image,
    ImageViewRef image_view
) : BasicObject(),
    m_image(std::move(image)),
    m_image_view(std::move(image_view))
{
    AssertThrowMsg(m_image.IsValid(), "Image must be valid");
    AssertThrowMsg(m_image_view.IsValid(), "ImageView must be valid");
}

Texture::Texture(
    Image &&image
) : BasicObject(),
    m_image(MakeRenderObject<Image>(std::move(image))),
    m_image_view(MakeRenderObject<ImageView>())
{
    AssertThrowMsg(m_image.IsValid(), "Image must be valid");
    AssertThrowMsg(m_image_view.IsValid(), "ImageView must be valid");
}

Texture::Texture(Texture &&other) noexcept
    : BasicObject(std::move(other)),
      m_image(std::move(other.m_image)),
      m_image_view(std::move(other.m_image_view))
{
}

Texture::~Texture()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));

    if (IsInitCalled()) {
        PUSH_RENDER_COMMAND(
            DestroyTexture,
            m_id,
            std::move(m_image),
            std::move(m_image_view)
        );
    }
}

void Texture::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        SafeRelease(std::move(m_image));
        SafeRelease(std::move(m_image_view));

        if (IsInitCalled()) {
            PUSH_RENDER_COMMAND(
                DestroyTexture,
                m_id,
                std::move(m_image),
                std::move(m_image_view)
            );
        }
    }));

    PUSH_RENDER_COMMAND(
        CreateTexture,
        m_id,
        renderer::ResourceState::SHADER_RESOURCE,
        m_image,
        m_image_view
    );

    SetReady(true);
}

void Texture::GenerateMipmaps()
{
    AssertReady();

    TextureMipmapRenderer mipmap_renderer(m_image, m_image_view);
    mipmap_renderer.Create();
    mipmap_renderer.Render();
    mipmap_renderer.Destroy();
}

Vec4f Texture::Sample(Vec2f uv) const
{
    if (!IsReady()) {
        return Vec4f::Zero();
    }

    const RC<StreamedTextureData> &streamed_data = m_image->GetStreamedData();

    if (!streamed_data) {
        return Vec4f::Zero();
    }

    auto ref = streamed_data->AcquireRef();
    const TextureData &texture_data = ref->GetTextureData();

    if (texture_data.buffer.Size() == 0) {
        return Vec4f::Zero();
    }

    const Vec2u coord = {
        uint32(uv.x * texture_data.desc.extent.width),
        uint32(uv.y * texture_data.desc.extent.height)
    };

    const uint32 bytes_per_pixel = renderer::NumBytes(texture_data.desc.format);

    if (bytes_per_pixel != 1) {
        HYP_LOG(Texture, LogLevel::ERROR, "Texture::Sample: Unsupported bytes per pixel: {}", bytes_per_pixel);

        return Vec4f::Zero();
    }

    const uint32 num_components = renderer::NumComponents(m_image->GetTextureFormat());

    const uint32 index = coord.y *  texture_data.desc.extent.width * bytes_per_pixel * num_components + coord.x * bytes_per_pixel * num_components;

    if (index >= texture_data.buffer.Size()) {
        return Vec4f::Zero();
    }

    const uint8 *data = texture_data.buffer.Data() + index;

    switch (num_components) {
    case 1:
        return Vec4f(data[0] / 255.0f);
    case 2:
        return Vec4f(data[0] / 255.0f, data[1] / 255.0f, 0.0f, 1.0f);
    case 3:
        return Vec4f(data[0] / 255.0f, data[1] / 255.0f, data[2] / 255.0f, 1.0f);
    case 4:
        return Vec4f(data[0] / 255.0f, data[1] / 255.0f, data[2] / 255.0f, data[3] / 255.0f);
    default: // should never happen
        return Vec4f::Zero();
    }
}

#pragma endregion Texture

} // namespace hyperion
