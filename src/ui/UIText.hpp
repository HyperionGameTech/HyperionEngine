#ifndef HYPERION_V2_UI_TEXT_H
#define HYPERION_V2_UI_TEXT_H

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/Handle.hpp>

#include <ui/UIScene.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Texture.hpp>

#include <math/Transform.hpp>
#include <math/Vector2.hpp>

#include <scene/Scene.hpp>

#include <core/Containers.hpp>
#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {

class SystemEvent;
class InputManager;

} // namespace hyperion

namespace hyperion::v2 {

using renderer::Extent2D;

class FontMap
{
public:
    FontMap(
        const Handle<Texture> &texture,
        Extent2D char_size
    )
        : m_texture(texture),
          m_char_size(char_size)
    {
        int x_position = 0;
        int y_position = 0;

        const UInt num_chars_per_row = NumCharsPerRow();
        const UInt num_chars_per_col = NumCharsPerCol();

        for (int ch = 16; ch < 255; ch++) {
            m_char_texture_coords[ch] = Vector2(
                x_position,
                y_position
            );

            if (x_position == num_chars_per_row - 1) {
                x_position = 0;

                if (y_position == num_chars_per_col - 1) {
                    break;
                }

                y_position++;
            } else {
                x_position++;
            }
        }
    }

    FontMap(const FontMap &other)
        : m_texture(other.m_texture),
          m_char_size(other.m_char_size),
          m_char_texture_coords(other.m_char_texture_coords)
    {
    }

    FontMap &operator=(const FontMap &other)
    {
        m_texture = other.m_texture;
        m_char_size = other.m_char_size;
        m_char_texture_coords = other.m_char_texture_coords;

        return *this;
    }

    FontMap(FontMap &&other) noexcept
        : m_texture(std::move(other.m_texture)),
          m_char_size(other.m_char_size),
          m_char_texture_coords(std::move(other.m_char_texture_coords))
    {
    }

    FontMap &operator=(FontMap &&other) noexcept
    {
        m_texture = std::move(other.m_texture);
        m_char_size = other.m_char_size;
        m_char_texture_coords = std::move(other.m_char_texture_coords);

        return *this;
    }

    ~FontMap() = default;

    Handle<Texture> &GetTexture()
        { return m_texture; }

    const Handle<Texture> &GetTexture() const
        { return m_texture; }

    UInt NumCharsPerRow() const
    {
        if (!m_texture || m_char_size.width == 0) {
            return 0;
        }

        return m_texture->GetExtent().width / m_char_size.width;
    }

    UInt NumCharsPerCol() const
    {
        if (!m_texture || m_char_size.height == 0) {
            return 0;
        }

        return m_texture->GetExtent().height / m_char_size.height;
    }

    Vector2 GetCharOffset(char ch) const
    {
        return m_char_texture_coords[ch] / Vector2(NumCharsPerCol(), NumCharsPerRow());
    }

    Vector2 GetScaling() const
    {
        if (!m_texture) {
            return Vector2::zero;
        }

        return Vector2(m_char_size) / Vector2(m_texture->GetExtent());
    }

private:
    Handle<Texture> m_texture;
    Extent2D m_char_size;
    FixedArray<Vector2, 255> m_char_texture_coords;
};

class UIText
{
public:
    static Handle<Mesh> BuildTextMesh(const FontMap &font_map, const String &text);
};

} // namespace hyperion::v2

#endif