#include "ui_text.h"
#include "../shader_manager.h"
#include "../shaders/ui/ui_object_shader.h"
#include "../mesh/mesh_array.h"
#include "../../asset/asset_manager.h"
#include "../../util/mesh_factory.h"
#include "../../util.h"

#include <algorithm>

namespace hyperion {
namespace ui {
FontMap::FontMap(const std::shared_ptr<Texture2D> &texture, int num_chars_per_row, int num_chars_per_col, int char_offset)
    : m_texture(texture),
      m_char_offset(char_offset)
{
    ex_assert(texture != nullptr);

    m_char_size = Vector2(
        m_texture->GetWidth() / num_chars_per_row,
        m_texture->GetHeight() / num_chars_per_col
    );

    int x_position = 0;
    int y_position = 0;

    for (int ch = char_offset; ch < 255; ch++) {
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

    BuildCharMeshes();
}

void FontMap::BuildCharMeshes()
{
    for (int ch = m_char_offset; ch < m_char_meshes.size(); ch++) {
        CharMesh cm;
        cm.quad = MeshFactory::CreateQuad(false);

        const Vector2 offset = GetCharOffset(ch);
        const Vector2 scaling = GetScaling();

        auto vertices = cm.quad->GetVertices();

        for (auto& vert : vertices) {
            vert.SetTexCoord0(offset + (vert.GetTexCoord0() * scaling));
        }

        cm.quad->SetVertices(vertices, cm.quad->GetIndices());

        m_char_meshes[ch] = cm;
    }
}

UIText::UIText(const std::string &name, const std::string &text)
    : UIObject(name),
      m_text(text),
      m_needs_update(true)
{
    m_font_map = new FontMap(
        AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/fonts/courier_new.png"),
        16,
        16,
        32
    );

    m_renderable = std::make_shared<MeshArray>();

    if (!m_text.empty()) {
        UpdateTextTransforms();

        m_material.SetTexture("ColorMap", m_font_map->GetTexture());
        m_renderable->SetShader(ShaderManager::GetInstance()->GetShader<UIObjectShader>(ShaderProperties()));
        m_renderable->SetRenderBucket(Renderable::RB_SCREEN);
    }
}

UIText::~UIText()
{
    delete m_font_map;
}

void UIText::UpdateTextTransforms()
{
    m_flags &= ~UPDATE_TEXT;

    m_char_mesh_transforms.clear();
    m_char_mesh_transforms.resize(m_text.size());

    auto mesh_array = std::dynamic_pointer_cast<MeshArray>(m_renderable);

    ex_assert(mesh_array != nullptr);

    mesh_array->ClearSubmeshes();

    Transform base_transform;
    base_transform.SetTranslation(Vector3(1, -1, 0));
    base_transform.SetRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(180)));

    Vector3 character_placement;

    for (size_t i = 0; i < m_text.size(); i++) {
        unsigned char ch = m_text[i];

        if (ch == '\n') {
            character_placement.x = 0;
            character_placement.y -= 1.5;
            continue;
        }

        Submesh sm;
        sm.mesh = m_font_map->m_char_meshes[ch].quad;
        sm.transform.SetTranslation(character_placement);
        sm.transform *= base_transform;

        mesh_array->AddSubmesh(sm);

        character_placement.x += 1.5;
    }
}

void UIText::SetText(const std::string& text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    m_flags |= UPDATE_TEXT | UPDATE_TRANSFORM;
}

void UIText::UpdateTransform()
{
    UIObject::UpdateTransform();

    if (m_flags & UPDATE_TEXT) {
        UpdateTextTransforms();
    }
}

} // namespace ui
} // namespace hyperion
