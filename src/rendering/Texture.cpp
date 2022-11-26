#include "Texture.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

using renderer::Result;

class Texture;

struct RENDER_COMMAND(CreateTexture) : RenderCommandBase2
{
    Texture *texture;
    renderer::ResourceState initial_state;
    renderer::Image *image;
    renderer::ImageView *image_view;
    renderer::Sampler *sampler;

    RENDER_COMMAND(CreateTexture)(
        Texture *texture,
        renderer::ResourceState initial_state,
        renderer::Image *image,
        renderer::ImageView *image_view,
        renderer::Sampler *sampler
    ) : texture(texture),
        initial_state(initial_state),
        image(image),
        image_view(image_view),
        sampler(sampler)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(image->Create(Engine::Get()->GetGPUDevice(), Engine::Get()->GetGPUInstance(), initial_state));
        HYPERION_BUBBLE_ERRORS(image_view->Create(Engine::Get()->GetGPUInstance()->GetDevice(), image));
        HYPERION_BUBBLE_ERRORS(sampler->Create(Engine::Get()->GetGPUInstance()->GetDevice()));

#if HYP_FEATURES_BINDLESS_TEXTURES
        Engine::Get()->GetRenderData()->textures.AddResource(texture);
#endif

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyTexture) : RenderCommandBase2
{
    Texture::ID id;
    renderer::Image *image;
    renderer::ImageView *image_view;
    renderer::Sampler *sampler;

    RENDER_COMMAND(DestroyTexture)(
        Texture::ID id,
        renderer::Image *image,
        renderer::ImageView *image_view,
        renderer::Sampler *sampler
    ) : id(id),
        image(image),
        image_view(image_view),
        sampler(sampler)
    {
    }

    virtual Result operator()()
    {
#if HYP_FEATURES_BINDLESS_TEXTURES
        Engine::Get()->GetRenderData()->textures.RemoveResource(id);
#endif

        HYPERION_BUBBLE_ERRORS(sampler->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()));
        HYPERION_BUBBLE_ERRORS(image_view->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()));
        HYPERION_BUBBLE_ERRORS(image->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()));

        HYPERION_RETURN_OK;
    }
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
    m_image(std::move(image)),
    m_image_view(),
    m_sampler(filter_mode, wrap_mode)
{
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
    Teardown();
}

void Texture::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    RenderCommands::Push<RENDER_COMMAND(CreateTexture)>(
        this,
        renderer::ResourceState::SHADER_RESOURCE,
        &m_image,
        &m_image_view,
        &m_sampler
    );

    SetReady(true);
    
    OnTeardown([this]() {
        SetReady(false);

        RenderCommands::Push<RENDER_COMMAND(DestroyTexture)>(
            m_id,
            &m_image,
            &m_image_view,
            &m_sampler
        );
        
        HYP_FLUSH_RENDER_QUEUE();
    });
}

} // namespace hyperion::v2
