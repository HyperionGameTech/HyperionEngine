#include "Texture.hpp"
#include <Engine.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

using renderer::Result;

class Texture;


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
    SamplerRef sampler;

    RENDER_COMMAND(CreateTexture)(
        Texture *texture,
        renderer::ResourceState initial_state,
        const ImageRef &image,
        const ImageViewRef &image_view,
        const SamplerRef &sampler
    ) : texture(texture),
        initial_state(initial_state),
        image(image),
        image_view(image_view),
        sampler(sampler)
    {
        AssertThrow(image.IsValid());
        AssertThrow(image_view.IsValid());
        AssertThrow(sampler.IsValid());
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(image->Create(Engine::Get()->GetGPUDevice(), Engine::Get()->GetGPUInstance(), initial_state));
        HYPERION_BUBBLE_ERRORS(image_view->Create(Engine::Get()->GetGPUInstance()->GetDevice(), image.Get()));
        HYPERION_BUBBLE_ERRORS(sampler->Create(Engine::Get()->GetGPUInstance()->GetDevice()));
        
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
    SamplerRef sampler;

    RENDER_COMMAND(DestroyTexture)(
        ID<Texture> id,
        ImageRef &&image,
        ImageViewRef &&image_view,
        SamplerRef &&sampler
    ) : id(id),
        image(std::move(image)),
        image_view(std::move(image_view)),
        sampler(std::move(sampler))
    {
    }

    virtual Result operator()()
    {
        if (Engine::Get()->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            Engine::Get()->GetRenderData()->textures.RemoveResource(id);
        }

        SafeRelease(std::move(image));
        SafeRelease(std::move(image_view));
        SafeRelease(std::move(sampler));

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

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
    m_sampler(RenderObjects::Make<Sampler>(filter_mode, wrap_mode))
{
    AssertThrow(m_image.GetRefCount() != 0);
}

Texture::Texture(Texture &&other) noexcept
    : EngineComponentBase(std::move(other)),
      m_image(std::move(other.m_image)),
      m_image_view(std::move(other.m_image_view)),
      m_sampler(std::move(other.m_sampler))
{
}

// Texture &Texture::operator=(Texture &&other) noexcept
// {
//     EngineComponentBase::operator=(std::move(other));

//     m_image = std::move(other.m_image),
//     m_image_view = std::move(other.m_image_view),
//     m_sampler = std::move(other.m_sampler);

//     return *this;
// }

Texture::~Texture()
{
    SetReady(false);

    if (IsInitCalled()) {
        PUSH_RENDER_COMMAND(
            DestroyTexture,
            m_id,
            std::move(m_image),
            std::move(m_image_view),
            std::move(m_sampler)
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
        m_image_view,
        m_sampler
    );

    SetReady(true);
}

} // namespace hyperion::v2
