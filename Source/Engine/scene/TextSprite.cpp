/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <scene/TextSprite.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <ui/font/FontAtlas.hpp>

#include <Core/utilities/GlobalContext.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>

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

    Array<FontAtlasCharacterIterator> currentWordChars;

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

    ForEachCharacter(fontAtlas, text, textSize, [textSize, &aabb](const FontAtlasCharacterIterator& iter)
        {
            BoundingBox characterAabb = BoundingBox::Zero();

            const float offsetY = (iter.cellDimensions.y - iter.glyphDimensions.y) + iter.bearingY;

            characterAabb = characterAabb.Union(Vec3f(iter.placement.x, iter.placement.y + offsetY, 0.0f));
            characterAabb = characterAabb.Union(Vec3f(iter.placement.x + iter.glyphDimensions.x, iter.placement.y + offsetY + iter.cellDimensions.y, 0.0f));

            aabb = aabb.Union(characterAabb);
        });

    return aabb * Vec3f(textSize, textSize, 1.0f);
}

TextSprite::TextSprite()
    : Sprite(Name::Invalid(), SpriteType::Text)
{
}

TextSprite::TextSprite(Name name, const String& text)
    : Sprite(name, SpriteType::Text),
      m_text(text)
{
    auto result = CreateFontAtlas();
    if (result.HasValue())
    {
        m_fontAtlas = result.GetValue();
    }
    else if (result.HasError())
    {
        HYP_LOG(Scene, Error, "Failed to set FontAtlas for TextSprite {}: {}", GetName(), result.GetError().GetMessage());
    }
}

TextSprite::~TextSprite()
{
}

void TextSprite::Init()
{
    HYP_SCOPE;

    Sprite::Init();

    auto& bem = GetComponent<BoundingBoxComponent>();
    bem.worldAabb = m_textAabb;

    UpdateFontAtlasTexture();
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

void TextSprite::SetText(const String& text)
{
    m_text = text;
    UpdateTextAABB();
    UpdateFontAtlasTexture();

    auto& bem = GetComponent<BoundingBoxComponent>();
    bem.worldAabb = m_textAabb;

    SetNeedsRenderProxyUpdate();
}

void TextSprite::SetTextColor(Color color)
{
    m_textColor = color;
    SetNeedsRenderProxyUpdate();
}

void TextSprite::SetTextSize(float textSize)
{
    m_textSize = textSize;
    UpdateTextAABB();
    UpdateFontAtlasTexture();

    auto& bem = GetComponent<BoundingBoxComponent>();
    bem.worldAabb = m_textAabb;

    SetNeedsRenderProxyUpdate();
}

void TextSprite::SetFontAtlas(const Handle<FontAtlas>& fontAtlas)
{
    m_fontAtlas = fontAtlas;
    UpdateTextAABB();

    auto& bem = GetComponent<BoundingBoxComponent>();
    bem.worldAabb = m_textAabb;

    SetNeedsRenderProxyUpdate();
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