#include "Texture.hpp"
#include <Engine.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

using renderer::Result;
using renderer::Frame;

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

struct RENDER_COMMAND(CreateTexture) : RenderCommand
{
    Texture *texture;
    renderer::ResourceState initial_state;
    ImageRef image;
    ImageViewRef image_view;

    RENDER_COMMAND(CreateTexture)(
        Texture *texture,
        renderer::ResourceState initial_state,
        const ImageRef &image,
        const ImageViewRef &image_view
    ) : texture(texture),
        initial_state(initial_state),
        image(image),
        image_view(image_view)
    {
        AssertThrow(image.IsValid());
        AssertThrow(image_view.IsValid());
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(image->Create(Engine::Get()->GetGPUDevice(), Engine::Get()->GetGPUInstance(), initial_state));
        HYPERION_BUBBLE_ERRORS(image_view->Create(Engine::Get()->GetGPUInstance()->GetDevice(), image.Get()));
        
        if (Engine::Get()->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            Engine::Get()->GetRenderData()->textures.AddResource(texture);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyTexture) : RenderCommand
{
    ID<Texture> id;
    ImageRef image;
    ImageViewRef image_view;

    RENDER_COMMAND(DestroyTexture)(
        ID<Texture> id,
        ImageRef &&image,
        ImageViewRef &&image_view
    ) : id(id),
        image(std::move(image)),
        image_view(std::move(image_view))
    {
    }

    virtual Result operator()()
    {
        if (Engine::Get()->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            Engine::Get()->GetRenderData()->textures.RemoveResource(id);
        }

        SafeRelease(std::move(image));
        SafeRelease(std::move(image_view));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateMipImageView) : RenderCommand
{
    ImageRef src_image;
    ImageViewRef mip_image_view;
    UInt mip_level;

    RENDER_COMMAND(CreateMipImageView)(
        ImageRef src_image,
        ImageViewRef mip_image_view,
        UInt mip_level
    ) : src_image(std::move(src_image)),
        mip_image_view(std::move(mip_image_view)),
        mip_level(mip_level)
    {
    }

    virtual Result operator()() override
    {
        return mip_image_view->Create(
            Engine::Get()->GetGPUDevice(),
            src_image.Get(),
            mip_level, 1,
            0, src_image->NumFaces()
        );
    }
};

struct RENDER_COMMAND(CreateMipDescriptorSet) : RenderCommand
{
    DescriptorSetRef descriptor_set;

    RENDER_COMMAND(CreateMipDescriptorSet)(
        DescriptorSetRef descriptor_set
    ) : descriptor_set(std::move(descriptor_set))
    {
    }

    virtual Result operator()() override
    {
        return descriptor_set->Create(
            Engine::Get()->GetGPUDevice(),
            &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
        );
    }
};

struct RENDER_COMMAND(RenderTextureMipmapLevels) : RenderCommand
{
    ImageRef m_image;
    ImageViewRef m_image_view;
    Array<ImageViewRef> m_mip_image_views;
    Array<DescriptorSetRef> m_descriptor_sets;
    Array<RC<FullScreenPass>> m_passes;

    RENDER_COMMAND(RenderTextureMipmapLevels)(
        ImageRef image,
        ImageViewRef image_view,
        Array<ImageViewRef> mip_image_views,
        Array<DescriptorSetRef> descriptor_sets,
        Array<RC<FullScreenPass>> passes
    ) : m_image(std::move(image)),
        m_image_view(std::move(image_view)),
        m_mip_image_views(std::move(mip_image_views)),
        m_descriptor_sets(std::move(descriptor_sets)),
        m_passes(std::move(passes))
    {
        AssertThrow(m_image.IsValid());
        AssertThrow(m_image_view.IsValid());

        AssertThrow(m_descriptor_sets.Size() == m_mip_image_views.Size());
        AssertThrow(m_passes.Size() == m_mip_image_views.Size());

        for (SizeType index = 0; index < m_mip_image_views.Size(); index++) {
            AssertThrow(m_mip_image_views[index].IsValid());
            AssertThrow(m_descriptor_sets[index].IsValid());
            AssertThrow(m_passes[index] != nullptr);
        }
    }

    virtual Result operator()() override
    {
        // draw a quad for each level
        auto commands = Engine::Get()->GetGPUInstance()->GetSingleTimeCommands();

        commands.Push([this](const CommandBufferRef &command_buffer) {
            const Extent3D extent = m_image->GetExtent();

            UInt32 mip_width = extent.width,
                mip_height = extent.height;

            ImageRef &dst_image = m_image;

            for (UInt mip_level = 0; mip_level < UInt(m_mip_image_views.Size()); mip_level++) {
                auto &pass = m_passes[mip_level];
                AssertThrow(pass != nullptr);

                const UInt32 prev_mip_width = mip_width,
                    prev_mip_height = mip_height;

                mip_width = MathUtil::Max(1u, extent.width >> (mip_level));
                mip_height = MathUtil::Max(1u, extent.height >> (mip_level));

                struct alignas(128) {
                    ShaderVec4<UInt32> dimensions;
                    ShaderVec4<UInt32> prev_dimensions;
                    UInt32 mip_level;
                } push_constants;

                push_constants.dimensions = { mip_width, mip_height, 0, 0 };
                push_constants.prev_dimensions = { prev_mip_width, prev_mip_height, 0, 0 };
                push_constants.mip_level = mip_level;

                {
                    Frame temp_frame = Frame::TemporaryFrame(command_buffer);

                    pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
                    pass->Begin(&temp_frame);
                    
                    pass->GetCommandBuffer(temp_frame.GetFrameIndex())->BindDescriptorSet(
                        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                        pass->GetRenderGroup()->GetPipeline(),
                        m_descriptor_sets[mip_level].Get(), 0
                    );
                    
                    pass->GetQuadMesh()->Render(pass->GetCommandBuffer(temp_frame.GetFrameIndex()));
                    pass->End(&temp_frame);
                }

                const ImageRef &src_image = pass->GetAttachmentUsage(0)->GetAttachment()->GetImage();

                // Blit into mip level
                dst_image->GetGPUImage()->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::COPY_DST
                );

                src_image->GetGPUImage()->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::COPY_SRC
                );

                HYPERION_BUBBLE_ERRORS(
                    dst_image->Blit(
                        command_buffer,
                        src_image.Get(),
                        renderer::Rect {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = src_image->GetExtent().width,
                            .y1 = src_image->GetExtent().height
                        },
                        renderer::Rect {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = dst_image->GetExtent().width,
                            .y1 = dst_image->GetExtent().height
                        }
                    )
                );

                src_image->GetGPUImage()->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::SHADER_RESOURCE
                );

                // put this mip into readable state
                dst_image->GetGPUImage()->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::SHADER_RESOURCE
                );
            }

            // all mip levels have been transitioned into this state
            dst_image->GetGPUImage()->SetResourceState(
                renderer::ResourceState::SHADER_RESOURCE
            );

            HYPERION_RETURN_OK;
        });

        return commands.Execute(Engine::Get()->GetGPUInstance()->GetDevice());
    }
};

#pragma endregion


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
        const UInt num_mip_levels = m_image->NumMipmaps();

        m_mip_image_views.Resize(num_mip_levels);
        m_descriptor_sets.Resize(num_mip_levels);
        m_passes.Resize(num_mip_levels);

        const Extent3D extent = m_image->GetExtent();

        for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            const UInt32 mip_width = MathUtil::Max(1u, extent.width >> (mip_level));
            const UInt32 mip_height = MathUtil::Max(1u, extent.height >> (mip_level));

            ImageViewRef mip_image_view = RenderObjects::Make<ImageView>();

            PUSH_RENDER_COMMAND(CreateMipImageView, m_image, mip_image_view, mip_level);

            {
                // create a descriptor set for the shader to use for this mip level
                DescriptorSetRef descriptor_set = RenderObjects::Make<DescriptorSet>();

                // input image
                if (mip_level == 0) {
                    descriptor_set
                        ->AddDescriptor<renderer::ImageDescriptor>(0)
                        ->SetElementSRV(0, m_image_view.Get());
                } else {
                    descriptor_set
                        ->AddDescriptor<renderer::ImageDescriptor>(0)
                        ->SetElementSRV(0, m_mip_image_views[mip_level - 1].Get());
                }

                // linear sampler
                descriptor_set
                    ->AddDescriptor<renderer::SamplerDescriptor>(1)
                    ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerLinear());

                // nearest / point sampler
                descriptor_set
                    ->AddDescriptor<renderer::SamplerDescriptor>(2)
                    ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

                PUSH_RENDER_COMMAND(CreateMipDescriptorSet, descriptor_set);

                m_descriptor_sets[mip_level] = std::move(descriptor_set);
            }

            m_mip_image_views[mip_level] = std::move(mip_image_view);

            {
                auto shader = Engine::Get()->GetShaderManager().GetOrCreate(
                    HYP_NAME(GenerateMipmaps),
                    ShaderProperties()
                );

                InitObject(shader);

                RC<FullScreenPass> pass(new FullScreenPass(
                    shader,
                    Array<const DescriptorSet *> { m_descriptor_sets.Front().Get() },
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
        SafeRelease(std::move(m_descriptor_sets));
    }

    Result Render()
    {
        PUSH_RENDER_COMMAND(
            RenderTextureMipmapLevels,
            m_image,
            m_image_view,
            m_mip_image_views,
            m_descriptor_sets,
            m_passes
        );

        HYP_SYNC_RENDER();
    }

private:
    ImageRef m_image;
    ImageViewRef m_image_view;
    Array<ImageViewRef> m_mip_image_views;
    Array<DescriptorSetRef> m_descriptor_sets;
    Array<RC<FullScreenPass>> m_passes;
};


Texture::Texture(
    Extent3D extent,
    InternalFormat format,
    ImageType type,
    FilterMode filter_mode,
    WrapMode wrap_mode,
    const UByte *bytes
) : Texture(
        TextureImage(
            extent,
            format,
            type,
            filter_mode,
            bytes
        ),
        filter_mode,
        wrap_mode
    )
{
}

Texture::Texture(
    Image &&image,
    FilterMode filter_mode,
    WrapMode wrap_mode
) : EngineComponentBase(),
    m_image(RenderObjects::Make<Image>(std::move(image))),
    m_image_view(RenderObjects::Make<ImageView>()),
    m_filter_mode(filter_mode),
    m_wrap_mode(wrap_mode)
{
    AssertThrow(m_image.GetRefCount() != 0);
}

Texture::Texture(Texture &&other) noexcept
    : EngineComponentBase(std::move(other)),
      m_image(std::move(other.m_image)),
      m_image_view(std::move(other.m_image_view)),
      m_filter_mode(other.m_filter_mode),
      m_wrap_mode(other.m_wrap_mode)
{
}

Texture::~Texture()
{
    SetReady(false);

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

    EngineComponentBase::Init();

    PUSH_RENDER_COMMAND(
        CreateTexture,
        this,
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
    HYP_SYNC_RENDER();

    mipmap_renderer.Destroy();
}

} // namespace hyperion::v2
