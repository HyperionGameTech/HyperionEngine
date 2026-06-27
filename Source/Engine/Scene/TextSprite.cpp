/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/TextSprite.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <UI/Font/FontAtlas.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Rendering/RenderProxy.hpp>
#include <Rendering/Texture.hpp>

#include <System/AppContext.hpp>

#include <Framework/EngineDriver.hpp>

#include <TextSprite.generated.inl>

namespace Hyperion {

struct FontAtlasCharacterIterator
{
    Vec2f placement;
    Vec2f atlasPixelSize;
    Vec2f cellDimensions;

    Vec2i charOffset;

    Vec2f glyphDimensions;

    float bearingY;
    float charWidth;
};

static TResult<Handle<FontAtlas>> CreateFontAtlas()
{
    // we check if it exists in the registry before creating.
    // some platforms build without freetype support so the atlas must already exist in the registry.

    AssetRegistry& registry = *GetEngineAssetRegistry();

    GlobalContextScope contextScope { AssetRegistryContext { MakeStrongRef(&registry) } };

    Handle<FontAtlas> fontAtlas = DynamicCast<FontAtlas>(registry.GetAsset(AssetBuckets::FontAtlases, "Roboto_Regular"_sh));

    if (fontAtlas.IsValid())
    {
        return fontAtlas;
    }

    auto fontFaceAsset = g_assetManager->Load<RC<FontFace>>("Fonts/Roboto/Roboto-Regular.ttf");

    if (fontFaceAsset.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to load font face! Error: {}", fontFaceAsset.GetError().GetMessage());
    }

    // create new font atlas
    fontAtlas = MakeHandle<FontAtlas>(NAME("Roboto_Regular"), std::move(fontFaceAsset->Result()));

    // render atlas textures.
    if (Result renderAtlasResult = fontAtlas->RenderAtlasTextures(1.0f, 2.0f, 0.1f); renderAtlasResult.HasError())
    {
        return renderAtlasResult.GetError();
    }

    registry.PutAssetsDeep(fontAtlas);
    registry.SaveDirtyAssets();

    // need to move in return since return type is wrapped result
    return fontAtlas;
}

template <class Callback>
static void ForEachCharacter(
    const FontAtlas& fontAtlas,
    const String& text,
    float textSize,
    Callback&& callback)
{
    HYP_SCOPE;

    Vec2f placement = Vec2f::Zero();

    const size_t length = text.Length();

    const Vec2f cellDimensions = Vec2f(fontAtlas.GetCellDimensions()) / 64.0f;

    const Handle<Texture>& mainTextureAtlas = fontAtlas.GetAtlasTextures().GetMainAtlas();
    AssertDebug(mainTextureAtlas.IsValid(), "Main texture atlas is invalid");

    if (!mainTextureAtlas.IsValid())
    {
        return;
    }

    Vec2f atlasPixelSize;

    if (mainTextureAtlas->GetExtent().Volume() != 0)
    {
        atlasPixelSize = Vec2f::One() / Vec2f(mainTextureAtlas->GetExtent().GetXY());
    }

    Array<FontAtlasCharacterIterator, ThreadAllocator> currentWordChars;

    const auto iterateCurrentWord = [&currentWordChars, &callback]()
    {
        for (const FontAtlasCharacterIterator& characterIterator : currentWordChars)
        {
            callback(characterIterator);
        }

        currentWordChars.Clear();
    };

    for (size_t i = 0; i < length; i++)
    {
        const utf::Char32 ch = text.GetChar(i);

        if (ch == utf::Char32(' '))
        {
            placement.x += cellDimensions.x * 0.5f;
            iterateCurrentWord();
            continue;
        }

        if (ch == utf::Char32('\n'))
        {
            placement.x = 0.0f;
            placement.y += cellDimensions.y;
            iterateCurrentWord();
            continue;
        }

        Optional<const GlyphMetrics&> glyphMetrics = fontAtlas.GetGlyphMetricsForChar(ch);

        if (!glyphMetrics.HasValue() || (glyphMetrics->width == 0 || glyphMetrics->height == 0))
        {
            HYP_LOG_ONCE(Scene, Warning, "Ensure how to render char: {}", (int)ch);
            continue;
        }

        FontAtlasCharacterIterator& characterIterator = currentWordChars.EmplaceBack();
        characterIterator.placement = placement;
        characterIterator.atlasPixelSize = atlasPixelSize;
        characterIterator.cellDimensions = cellDimensions;
        characterIterator.charOffset = glyphMetrics->imagePosition;
        characterIterator.glyphDimensions = Vec2f(float(glyphMetrics->width), float(glyphMetrics->height)) / 64.0f;
        characterIterator.bearingY = float(glyphMetrics->height - glyphMetrics->bearingY) / 64.0f;
        characterIterator.charWidth = float(glyphMetrics->advance / 64) / 64.0f;

        placement.x += characterIterator.charWidth;
    }

    iterateCurrentWord();
}

static BoundingBox CalculateTextAABB(const FontAtlas& fontAtlas, const String& text, float textSize)
{
    HYP_SCOPE;

    BoundingBox aabb = BoundingBox::Zero();

    Vec2f totalSize = Vec2f::Zero();
    Vec2f placement = Vec2f::Zero();
    const Vec2f cellDimensions = Vec2f(fontAtlas.GetCellDimensions()) / 64.0f;

    const size_t length = text.Length();
    for (size_t i = 0; i < length; i++)
    {
        const utf::Char32 ch = text.GetChar(i);

        if (ch == utf::Char32(' '))
        {
            placement.x += cellDimensions.x * 0.5f;
            continue;
        }

        if (ch == utf::Char32('\n'))
        {
            totalSize.x = MathUtil::Max(totalSize.x, placement.x);
            placement.x = 0.0f;
            placement.y += cellDimensions.y;
            continue;
        }

        Optional<const GlyphMetrics&> glyphMetrics = fontAtlas.GetGlyphMetricsForChar(ch);
        if (!glyphMetrics.HasValue() || (glyphMetrics->width == 0 || glyphMetrics->height == 0))
        {
            continue;
        }

        const Vec2f glyphDimensions = Vec2f(float(glyphMetrics->width), float(glyphMetrics->height)) / 64.0f;
        const float charWidth = float(glyphMetrics->advance / 64) / 64.0f;
        const float bearingY = float(glyphMetrics->height - glyphMetrics->bearingY) / 64.0f;

        const float charHeight = glyphDimensions.y + bearingY;
        totalSize.y = MathUtil::Max(totalSize.y, placement.y + charHeight);

        placement.x += charWidth;
    }

    totalSize.x = MathUtil::Max(totalSize.x, placement.x);

    ForEachCharacter(fontAtlas, text, textSize, [textSize, &aabb, &totalSize](const FontAtlasCharacterIterator& iter)
                     {
                         BoundingBox characterAabb = BoundingBox::Zero();

                         const float offsetY = -iter.bearingY;

                         characterAabb = characterAabb.Union(Vec3f(iter.placement.x - totalSize.x * 0.5f, iter.placement.y + offsetY - totalSize.y * 0.5f, 0.0f));
                         characterAabb = characterAabb.Union(Vec3f(iter.placement.x + iter.glyphDimensions.x - totalSize.x * 0.5f, iter.placement.y + offsetY + iter.glyphDimensions.y - totalSize.y * 0.5f, 0.0f));

                         aabb = aabb.Union(characterAabb);
                     });

    return aabb * Vec3f(textSize, textSize, 1.0f);
}

TextSprite::TextSprite()
    : TextSprite(Name::Invalid(), String::empty)
{
}

TextSprite::TextSprite(Name name, const String& text)
    : Sprite(name, SpriteType::Text),
      m_text(text)
{
    auto fontAtlasResult = CreateFontAtlas();
    if (fontAtlasResult.HasValue())
    {
        m_fontAtlas = fontAtlasResult.GetValue();
    }
    else if (fontAtlasResult.HasError())
    {
        HYP_LOG(Scene, Error, "Failed to set FontAtlas for TextSprite {}: {}", GetName(), fontAtlasResult.GetError().GetMessage());
    }
}

TextSprite::~TextSprite()
{
}

void TextSprite::UpdateFontAtlasTexture()
{
    if (const Handle<FontAtlas>& fontAtlas = GetFontAtlasOrDefault())
    {
        m_currentFontAtlasTexture = fontAtlas->GetAtlasTextures().GetAtlasForPixelSize(m_textSize);
    }
    else
    {
        m_currentFontAtlasTexture = nullptr;
    }
}

void TextSprite::OnAddedToWorld(World* world)
{
    Sprite::OnAddedToWorld(world);

    auto& bem = GetComponent<BoundingBoxComponent>();
    bem.worldAabb = m_textAabb;

    UpdateFontAtlasTexture();
}

void TextSprite::SetText(const String& text)
{
    m_text = text;

    if (GetWorld() != nullptr)
    {
        UpdateTextAABB();
        UpdateFontAtlasTexture();

        auto& bem = GetComponent<BoundingBoxComponent>();
        bem.worldAabb = m_textAabb;

        SetNeedsRenderProxyUpdate();
    }
}

void TextSprite::SetTextColor(Color color)
{
    m_textColor = color;

    if (GetWorld() != nullptr)
    {
        SetNeedsRenderProxyUpdate();
    }
}

void TextSprite::SetTextSize(float textSize)
{
    m_textSize = textSize;

    if (GetWorld() != nullptr)
    {
        UpdateTextAABB();
        UpdateFontAtlasTexture();

        auto& bem = GetComponent<BoundingBoxComponent>();
        bem.worldAabb = m_textAabb;

        SetNeedsRenderProxyUpdate();
    }
}

void TextSprite::SetFontAtlas(const Handle<FontAtlas>& fontAtlas)
{
    m_fontAtlas = fontAtlas;

    if (GetWorld() != nullptr)
    {
        UpdateTextAABB();

        auto& bem = GetComponent<BoundingBoxComponent>();
        bem.worldAabb = m_textAabb;

        SetNeedsRenderProxyUpdate();
    }
}

const Handle<FontAtlas>& TextSprite::GetFontAtlasOrDefault() const
{
    return m_fontAtlas;
}

void TextSprite::UpdateRenderProxy(RenderProxySprite* proxy)
{
    Sprite::UpdateRenderProxy(proxy);

    proxy->text = m_text;
    proxy->textColor = m_textColor;
    proxy->textSize = m_textSize;
    proxy->textAabb = m_textAabb;
    proxy->fontAtlas = m_fontAtlas.Get();
    proxy->texture = m_currentFontAtlasTexture.Get();
}

void TextSprite::UpdateTextAABB()
{
    if (const Handle<FontAtlas>& fontAtlas = GetFontAtlasOrDefault())
    {
        if (!m_text.Empty())
        {
            m_textAabb = CalculateTextAABB(*fontAtlas, m_text, m_textSize);
        }
        else
        {
            m_textAabb = BoundingBox::Zero();
        }
    }
    else
    {
        HYP_LOG(Scene, Verbose, "No font atlas set for TextSprite {}", GetName());
    }
}

} // namespace Hyperion
