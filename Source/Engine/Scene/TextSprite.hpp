/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Core/containers/String.hpp>

#include <Core/math/Color.hpp>
#include <Core/math/Transform.hpp>

#include <Core/reflection/Handle.hpp>

#include <Scene/Sprite.hpp>

#include <UI/font/FontAtlas.hpp>

namespace Hyperion {

HYP_CLASS()
class TextSprite final : public Sprite
{
    HYP_OBJECT_BODY(TextSprite);

public:
    TextSprite();
    explicit TextSprite(Name name, const String& text = "");

    TextSprite(const TextSprite& other) = delete;
    TextSprite& operator=(const TextSprite& other) = delete;

    ~TextSprite() override;

    HYP_METHOD(Property = "Text", Serialize, Editor)
    void SetText(const String& text);

    HYP_METHOD(Property = "Text", Serialize, Editor)
    const String& GetText() const
    {
        return m_text;
    }

    HYP_METHOD(Property = "TextColor", Serialize, Editor)
    void SetTextColor(Color color);

    HYP_METHOD(Property = "TextColor", Serialize, Editor)
    Color GetTextColor() const
    {
        return m_textColor;
    }

    HYP_METHOD(Property = "TextSize", Serialize, Editor)
    void SetTextSize(float textSize);

    HYP_METHOD(Property = "TextSize", Serialize, Editor)
    float GetTextSize() const
    {
        return m_textSize;
    }

    HYP_METHOD(Property = "FontAtlas", Serialize, Editor)
    void SetFontAtlas(const Handle<FontAtlas>& fontAtlas);

    HYP_METHOD(Property = "FontAtlas", Serialize, Editor)
    const Handle<FontAtlas>& GetFontAtlas() const
    {
        return m_fontAtlas;
    }

    const Handle<Texture>& GetFontAtlasTexture() const
    {
        return m_currentFontAtlasTexture;
    }

    const Handle<FontAtlas>& GetFontAtlasOrDefault() const;

protected:
    void UpdateTextAABB();
    void UpdateFontAtlasTexture();

    void OnAddedToWorld(World* world) override;

    void UpdateRenderProxy(class RenderProxySprite* proxy) override;

    String m_text;
    Color m_textColor = Color::White();
    float m_textSize = 1.0f;
    Handle<FontAtlas> m_fontAtlas;
    Handle<Texture> m_currentFontAtlasTexture;

    BoundingBox m_textAabb;
};

} // namespace Hyperion
