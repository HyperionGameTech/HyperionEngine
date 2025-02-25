/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Mesh.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {


class Texture;
class TextureMipmapRenderer;

const FixedArray<Pair<Vec3f, Vec3f>, 6> Texture::cubemap_directions = {
    Pair<Vec3f, Vec3f> { Vec3f { 1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { -1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 1, 0 }, Vec3f { 0, 0, -1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, -1, 0 }, Vec3f { 0, 0, 1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, 1 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, -1 }, Vec3f { 0, 1, 0 } }
};

#pragma region Render commands

struct RENDER_COMMAND(CreateTexture) : renderer::RenderCommand
{
    WeakHandle<Texture>     texture;
    renderer::ResourceState initial_state;
    ImageRef                image;
    ImageViewRef            image_view;

    RENDER_COMMAND(CreateTexture)(
        const WeakHandle<Texture> &texture,
        renderer::ResourceState initial_state,
        ImageRef image,
        ImageViewRef image_view
    ) : texture(texture),
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
        if (Handle<Texture> texture_locked = texture.Lock()) {
            if (!image->IsCreated()) {
                HYPERION_BUBBLE_ERRORS(image->Create(g_engine->GetGPUDevice(), g_engine->GetGPUInstance(), initial_state));
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
    WeakHandle<Texture> texture;

    RENDER_COMMAND(DestroyTexture)(const WeakHandle<Texture> &texture)
        : texture(texture)
    {
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

#pragma region Texture

Texture::Texture() : Texture(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA8,
        Vec3u { 1, 1, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    })
{
}

Texture::Texture(const TextureDesc &texture_desc)
    : Texture(renderer::Image(texture_desc))
{
}

Texture::Texture(const RC<StreamedTextureData> &streamed_data)
    : Texture(renderer::Image(streamed_data))
{
}

Texture::Texture(RC<StreamedTextureData> &&streamed_data)
    : Texture(renderer::Image(std::move(streamed_data)))
{
}

Texture::Texture(
    ImageRef image,
    ImageViewRef image_view
) : HypObject(),
    m_image(std::move(image)),
    m_image_view(std::move(image_view))
{
    AssertThrowMsg(m_image.IsValid(), "Image must be valid");
    AssertThrowMsg(m_image_view.IsValid(), "ImageView must be valid");

    const Vec3u extent = m_image->GetTextureDesc().extent;
    AssertThrow(extent.x <= 32768);
    AssertThrow(extent.y <= 32768);
    AssertThrow(extent.z <= 32768);
}

Texture::Texture(
    Image &&image
) : HypObject(),
    m_image(MakeRenderObject<Image>(std::move(image))),
    m_image_view(MakeRenderObject<ImageView>())
{
    AssertThrowMsg(m_image.IsValid(), "Image must be valid");
    AssertThrowMsg(m_image_view.IsValid(), "ImageView must be valid");

    const Vec3u extent = m_image->GetTextureDesc().extent;
    AssertThrow(extent.x <= 32768);
    AssertThrow(extent.y <= 32768);
    AssertThrow(extent.z <= 32768);
}

Texture::~Texture()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));

    if (IsInitCalled()) {
        PUSH_RENDER_COMMAND(DestroyTexture, WeakHandleFromThis());
    }
}

void Texture::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        SafeRelease(std::move(m_image));
        SafeRelease(std::move(m_image_view));

        if (IsInitCalled()) {
            PUSH_RENDER_COMMAND(DestroyTexture, WeakHandleFromThis());
        }
    }));

    PUSH_RENDER_COMMAND(
        CreateTexture,
        WeakHandleFromThis(),
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

void Texture::Readback() const
{
    AssertReady();

    struct RENDER_COMMAND(Texture_Readback) : public renderer::RenderCommand
    {
        ImageRef    image;
        ByteBuffer  &result_byte_buffer;

        RENDER_COMMAND(Texture_Readback)(const ImageRef &image, ByteBuffer &result_byte_buffer)
            : image(image),
              result_byte_buffer(result_byte_buffer)
        {
        }

        virtual ~RENDER_COMMAND(Texture_Readback)() override
        {
            SafeRelease(std::move(image));
        }

        virtual RendererResult operator()() override
        {
            GPUBufferRef gpu_buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);
            HYPERION_ASSERT_RESULT(gpu_buffer->Create(g_engine->GetGPUDevice(), image->GetByteSize()));
            gpu_buffer->Map(g_engine->GetGPUDevice());
            gpu_buffer->SetResourceState(renderer::ResourceState::COPY_DST);

            renderer::SingleTimeCommands commands { g_engine->GetGPUDevice() };

            commands.Push([this, &gpu_buffer](const CommandBufferRef &command_buffer)
            {
                image->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

                gpu_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

                image->CopyToBuffer(
                    command_buffer,
                    gpu_buffer
                );

                gpu_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

                HYPERION_RETURN_OK;
            });

            RendererResult result = commands.Execute();

            if (!result) {
                return result;
            }

            result_byte_buffer.SetSize(gpu_buffer->Size());
            gpu_buffer->Read(g_engine->GetGPUDevice(), result_byte_buffer.Size(), result_byte_buffer.Data());

            SafeRelease(std::move(gpu_buffer));

            HYPERION_RETURN_OK;
        }
    };

    ByteBuffer result_byte_buffer;

    PUSH_RENDER_COMMAND(Texture_Readback, m_image, result_byte_buffer);
    HYP_SYNC_RENDER();

    // sanity check -- temp
    const SizeType expected = m_image->GetByteSize();
    const SizeType real = result_byte_buffer.Size();
    AssertThrowMsg(expected == real, "Failed to readback texture: expected size: %llu, got %llu", expected, real);

    RC<StreamedTextureData> streamed_data = MakeRefCountedPtr<StreamedTextureData>(TextureData {
        GetTextureDesc(),
        std::move(result_byte_buffer)
    });

    m_image->SetStreamedData(streamed_data);
}

Vec4f Texture::Sample(Vec2f uv) const
{
    if (!IsReady()) {
        HYP_LOG_ONCE(Texture, Warning, "Texture is not ready, cannot sample");

        return Vec4f::Zero();
    }

    if (GetType() != ImageType::TEXTURE_TYPE_2D) {
        HYP_LOG_ONCE(Texture, Warning, "Unsupported texture type for sampling directly: {}", GetType());

        return Vec4f::Zero();
    }

    const RC<StreamedTextureData> &streamed_data = m_image->GetStreamedData();

    if (!streamed_data) {
        HYP_LOG_ONCE(Texture, Warning, "Texture does not have streamed data present, attempting readback...");

        Readback();

        if (!streamed_data) {
            HYP_LOG_ONCE(Texture, Warning, "Texture readback failed. Sample will return zero.");

            return Vec4f::Zero();
        }
    }

    auto ref = streamed_data->AcquireRef();
    const TextureData &texture_data = ref->GetTextureData();

    if (texture_data.buffer.Size() == 0) {
        HYP_LOG_ONCE(Texture, Warning, "Texture buffer is empty");

        return Vec4f::Zero();
    }

    const Vec2u coord = {
        uint32(uv.x * (texture_data.desc.extent.x - 1)),
        uint32(uv.y * (texture_data.desc.extent.y - 1))
    };

    const uint32 bytes_per_pixel = renderer::NumBytes(texture_data.desc.format);

    if (bytes_per_pixel != 1) {
        HYP_LOG_ONCE(Texture, Warning, "Unsupported bytes per pixel: {}", bytes_per_pixel);

        return Vec4f::Zero();
    }

    const uint32 num_components = renderer::NumComponents(m_image->GetTextureFormat());

    const uint32 index = coord.y * texture_data.desc.extent.x * bytes_per_pixel * num_components + coord.x * bytes_per_pixel * num_components;

    if (index >= texture_data.buffer.Size()) {
        HYP_LOG(Texture, Warning, "Index out of bounds, index: {}, buffer size: {}, x: {}, y: {}, width: {}, height: {}", index, texture_data.buffer.Size(),
            coord.x, coord.y, texture_data.desc.extent.x, texture_data.desc.extent.y);

        return Vec4f::Zero();
    }

    const ubyte *data = texture_data.buffer.Data() + index;

    switch (num_components) {
    case 1:
        return Vec4f(float(data[0]) / 255.0f);
    case 2:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, 0.0f, 1.0f);
    case 3:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, float(data[2]) / 255.0f, 1.0f);
    case 4:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, float(data[2]) / 255.0f, float(data[3]) / 255.0f);
    default: // should never happen
        HYP_LOG(Texture, Error, "Unsupported number of components: {}", num_components);

        return Vec4f::Zero();
    }
}

#pragma endregion Texture

} // namespace hyperion
