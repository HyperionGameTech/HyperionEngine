#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "ui_object.h"
#include "../../util.h"
#include "../../math/vector2.h"
#include "../texture_2D.h"

#include <array>
#include <memory>

namespace hyperion {
class Mesh;
namespace ui {
struct CharMesh {
    std::shared_ptr<Mesh> quad;
};

class FontMap {
public:
    std::shared_ptr<Texture2D> m_texture;
    std::array<Vector2, 255> m_char_texture_coords;
    std::array<CharMesh, 255> m_char_meshes;
    Vector2 m_char_size;
    int m_char_offset;

    FontMap(const std::shared_ptr<Texture2D> &texture,
        int num_chars_per_row,
        int num_chars_per_col,
        int char_offset = 0);
    ~FontMap() = default;

    void BuildCharMeshes();

    inline const std::shared_ptr<Texture2D> &GetTexture() const { return m_texture; }

    inline Vector2 GetCharOffset(unsigned char ch) const
    {
        return m_char_texture_coords[ch] / Vector2(NumCharsPerRow(), NumCharsPerCol());
    }

    inline Vector2 GetScaling() const
    {
        AssertThrow(m_texture != nullptr);

        return m_char_size / Vector2(m_texture->GetWidth(), m_texture->GetHeight());
    }

    inline int NumCharsPerRow() const
    {
        AssertThrow(m_texture != nullptr);

        return m_texture->GetWidth() / int(m_char_size.x);
    }

    inline int NumCharsPerCol() const
    {
        AssertThrow(m_texture != nullptr);

        return m_texture->GetHeight() / int(m_char_size.y);
    }
};

class UIText : public UIObject {
public:
    UIText(const std::string &name = "", const std::string &text = "");
    virtual ~UIText();

    inline const std::string &GetText() const { return m_text; }
    void SetText(const std::string &text);

    virtual void UpdateTransform() override;

    enum UpdateFlags {
        UPDATE_TEXT = 64
    };

protected:
    std::string m_text;
    FontMap *m_font_map;
    std::vector<std::pair<CharMesh, Transform>> m_char_mesh_transforms;
    bool m_needs_update;

    void UpdateTextTransforms();
};
} // namespace ui
} // namespace hyperion


#endif
