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

#include <scene/Sprite.hpp>

#include <ui/font/FontAtlas.hpp>

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

    void SetText(const String& text);
    const String& GetText() const
    {
        return m_text;
    }

    void SetTextColor(Color color);
    Color GetTextColor() const
    {
        return m_textColor;
    }

    void SetTextSize(float textSize);
    float GetTextSize() const
    {
        return m_textSize;
    }

    void SetFontAtlas(const Handle<FontAtlas>& fontAtlas);
    const Handle<FontAtlas>& GetFontAtlas() const
    {
        return m_fontAtlas;
    }

    const Handle<FontAtlas>& GetFontAtlasOrDefault() const;

protected:
    void Init() override;

    void UpdateTextAABB();

    void UpdateRenderProxy(class RenderProxySprite* proxy) override;

    String m_text;
    Color m_textColor = Color::White();
    float m_textSize = 1.0f;
    Handle<FontAtlas> m_fontAtlas;
    Handle<Texture> m_currentFontAtlasTexture;

    BoundingBox m_textAabb;
};

} // namespace Hyperion