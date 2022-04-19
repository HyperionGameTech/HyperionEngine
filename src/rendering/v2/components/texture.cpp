#include "texture.h"
#include "../engine.h"

namespace hyperion::v2 {

Texture::Texture(
    Extent3D extent,
    Image::InternalFormat format,
    Image::Type type,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode,
    const unsigned char *bytes)
    : EngineComponent(extent, format, type, filter_mode, bytes),
      m_image_view(std::make_unique<ImageView>()),
      m_sampler(std::make_unique<Sampler>(filter_mode, wrap_mode))
{
}

Texture::~Texture()
{
    Teardown();
}

void Texture::DoOperation(Engine *engine, TextureOperation &op)
{
    /* Hope this ages well */
    switch (op.type) {
        case TextureOperation::Type::BLIT:
            this->BlitTexture(
                engine,
                op.values.blit.dst_rect,
                std::move(op.other),
                op.values.blit.src_rect
            );
            break;
        default:
            break;
    }
}

void Texture::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    m_enqueued_operations.reserve(4);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_TEXTURES, [this](Engine *engine) {
        EngineComponent::Create(
                engine,
                engine->GetInstance(),
                Image::LayoutTransferState<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL>{},
                Image::LayoutTransferState<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>{}
        );

        HYPERION_ASSERT_RESULT(m_image_view->Create(engine->GetInstance()->GetDevice(), &m_wrapped));
        HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetInstance()->GetDevice(), m_image_view.get()));

        engine->shader_globals->textures.AddResource(this);

        for (auto &operation : m_enqueued_operations) {
            this->DoOperation(engine, operation);
        }

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_TEXTURES, [this](Engine *engine) {
            AssertThrow(m_image_view != nullptr);
            AssertThrow(m_sampler != nullptr);

            engine->shader_globals->textures.RemoveResource(this);

            HYPERION_ASSERT_RESULT(m_sampler->Destroy(engine->GetInstance()->GetDevice()));
            m_sampler.reset();

            HYPERION_ASSERT_RESULT(m_image_view->Destroy(engine->GetInstance()->GetDevice()));
            m_image_view.reset();

            EngineComponent::Destroy(engine);
        }), engine);
    }));
}

void Texture::BlitTexture(Engine *engine, Image::Rect dst_rect, Ref<Texture> &&src, Image::Rect src_rect) {
    if (!IsInit()) {
        m_enqueued_operations.push_back({
            .type = TextureOperation::Type::BLIT,
            .values={
                    .blit={
                            .src_rect = src_rect,
                            .dst_rect = dst_rect
                    },
            },
            .other = std::move(src)
        });
        return;
    }
    m_wrapped.BlitImage(engine->GetInstance(), dst_rect, &src->m_wrapped, src_rect);
}

} // namespace hyperion::v2