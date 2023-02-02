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

} // namespace hyperion::v2
