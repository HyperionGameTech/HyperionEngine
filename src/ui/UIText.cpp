/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIText.hpp>
#include <ui/UIStage.hpp>

#include <rendering/ShaderGlobals.hpp>
#include <rendering/Texture.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>

#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

static float GetWindowDPIFactor()
{
    return g_engine->GetAppContext()->GetMainWindow()->IsHighDPI()
        ? 2.0f
        : 1.0f;
}

struct FontAtlasCharacterIterator
{
    Vec2f           placement;
    Vec2f           atlas_pixel_size;
    Vec2f           cell_dimensions;

    utf::u32char    char_value;

    Vec2i           char_offset;

    Vec2f           glyph_dimensions;
    Vec2f           glyph_scaling;

    float           bearing_y;
    float           char_width;
};

template <class Callback>
static void ForEachCharacter(const FontAtlas &font_atlas, const String &text, const Vec2i &parent_bounds, float text_size, Callback &&callback)
{
    HYP_SCOPE;

    Vec2f placement;

    const SizeType length = text.Length();
    
    const Vec2f cell_dimensions = Vec2f(font_atlas.GetCellDimensions()) / 64.0f;
    AssertThrowMsg(cell_dimensions.x * cell_dimensions.y != 0.0f, "Cell dimensions are invalid");

    const float cell_dimensions_ratio = cell_dimensions.x / cell_dimensions.y;

    const Handle<Texture> &main_texture_atlas = font_atlas.GetAtlasTextures().GetMainAtlas();
    AssertThrowMsg(main_texture_atlas.IsValid(), "Main texture atlas is invalid");

    const Vec2f atlas_pixel_size = Vec2f::One() / Vec2f(main_texture_atlas->GetExtent().GetXY());
    
    struct
    {
        Array<FontAtlasCharacterIterator>   chars;
    } current_word;

    const auto IterateCurrentWord = [&current_word, &callback]()
    {
        for (const FontAtlasCharacterIterator &character_iterator : current_word.chars) {
            callback(character_iterator);
        }

        current_word = { };
    };

    for (SizeType i = 0; i < length; i++) {
        UITextCharacter character { };

        const utf::u32char ch = text.GetChar(i);

        if (ch == utf::u32char(' ')) {
            if (current_word.chars.Any() && parent_bounds.x != 0 && (current_word.chars.Back().placement.x + current_word.chars.Back().char_width) * text_size >= float(parent_bounds.x)) {
                // newline
                placement.x = 0.0f;
                placement.y += cell_dimensions.y;

                const Vec2f placement_origin = placement;
                const Vec2f placement_offset = current_word.chars.Front().placement - placement_origin;

                for (FontAtlasCharacterIterator &character_iterator : current_word.chars) {
                    character_iterator.placement -= placement_offset;
                }
            } else {
                // add room for space
                placement.x += cell_dimensions.x * 0.5f;
            }

            IterateCurrentWord();

            continue;
        }

        if (ch == utf::u32char('\n')) {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y += cell_dimensions.y;

            IterateCurrentWord();

            continue;
        }

        Optional<Glyph::Metrics> glyph_metrics = font_atlas.GetGlyphMetrics(ch);

        if (!glyph_metrics.HasValue()) {
            // @TODO Add a placeholder character for missing glyphs
            continue;
        }

        if (glyph_metrics->metrics.width == 0 || glyph_metrics->metrics.height == 0) {
            // empty width or height will cause issues
            continue;
        }

        FontAtlasCharacterIterator &character_iterator = current_word.chars.EmplaceBack();
        character_iterator.char_value = ch;
        character_iterator.placement = placement;
        character_iterator.atlas_pixel_size = atlas_pixel_size;
        character_iterator.cell_dimensions = cell_dimensions;
        character_iterator.char_offset = glyph_metrics->image_position;
        character_iterator.glyph_dimensions = Vec2f { float(glyph_metrics->metrics.width), float(glyph_metrics->metrics.height) } / 64.0f;
        character_iterator.glyph_scaling = Vec2f(character_iterator.glyph_dimensions) / (Vec2f(cell_dimensions));
        character_iterator.bearing_y = float(glyph_metrics->metrics.height - glyph_metrics->metrics.bearing_y) / 64.0f;
        character_iterator.char_width = float(glyph_metrics->metrics.advance / 64) / 64.0f;

        placement.x += character_iterator.char_width;
    }

    IterateCurrentWord();
}

static BoundingBox CalculateTextAABB(const FontAtlas &font_atlas, const String &text, const Vec2i &parent_bounds, float text_size, bool include_bearing)
{
    HYP_SCOPE;

    BoundingBox aabb;

    ForEachCharacter(font_atlas, text, parent_bounds, text_size, [include_bearing, &aabb](const FontAtlasCharacterIterator &iter)
    {
        BoundingBox character_aabb;

        if (include_bearing) {
            const float offset_y = (iter.cell_dimensions.y - iter.glyph_dimensions.y) + iter.bearing_y;

            character_aabb = character_aabb
                .Union(Vec3f(iter.placement.x, iter.placement.y + offset_y, 0.0f))
                .Union(Vec3f(iter.placement.x + iter.glyph_dimensions.x, iter.placement.y + offset_y + iter.cell_dimensions.y, 0.0f));
        } else {
            character_aabb = character_aabb
                .Union(Vec3f(iter.placement.x, iter.placement.y, 0.0f))
                .Union(Vec3f(iter.placement.x + iter.glyph_dimensions.x, iter.placement.y + iter.cell_dimensions.y, 0.0f));
        }

        aabb = aabb.Union(character_aabb);
    });

    return aabb;
}

#pragma region UIText

UIText::UIText()
    : UIObject(UIObjectType::TEXT)
{
    m_text_color = Color(Vec4f::One());
}

void UIText::Init()
{
    HYP_SCOPE;

    UIObject::Init();
}

void UIText::SetText(const String &text)
{
    HYP_SCOPE;

    UIObject::SetText(text);

    if (!IsInit()) {
        return;
    }

    SetNeedsRepaintFlag(true);

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

const RC<FontAtlas> &UIText::GetFontAtlasOrDefault() const
{
    HYP_SCOPE;

    const UIStage *stage = GetStage();

    if (!stage) {
        return m_font_atlas;
    }

    return m_font_atlas != nullptr
        ? m_font_atlas
        : stage->GetDefaultFontAtlas();
}

void UIText::SetFontAtlas(const RC<FontAtlas> &font_atlas)
{
    HYP_SCOPE;

    m_font_atlas = font_atlas;

    if (!IsInit()) {
        return;
    }

    SetNeedsRepaintFlag(true);

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

void UIText::UpdateTextAABB()
{
    HYP_SCOPE;

    if (const RC<FontAtlas> &font_atlas = GetFontAtlasOrDefault()) {
        const Vec2i parent_bounds = GetParentBounds();
        const float text_size = GetTextSize();

        m_text_aabb_with_bearing = CalculateTextAABB(*font_atlas, m_text, parent_bounds, text_size, true);
        m_text_aabb_without_bearing = CalculateTextAABB(*font_atlas, m_text, parent_bounds, text_size, false);
    } else {
        HYP_LOG_ONCE(UI, Warning, "No font atlas for UIText {}", GetName());
    }
}

void UIText::UpdateRenderData()
{
    HYP_SCOPE;

    if (m_locked_updates & UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA | UIObjectUpdateType::UPDATE_CHILDREN_TEXT_RENDER_DATA);

    // Only update render data if computed visibility is true (visibile)
    // When this changes to be true, UpdateRenderData will be called - no need to update it if we are not visible
    if (!GetComputedVisibility()) {
        return;
    }

    HYP_LOG(UI, Debug, "Update render data for text '{}'", GetText());

    if (const RC<FontAtlas> &font_atlas = GetFontAtlasOrDefault()) {
        const float text_size = GetTextSize();

        m_current_font_atlas_texture = font_atlas->GetAtlasTextures().GetAtlasForPixelSize(text_size);

        if (!m_current_font_atlas_texture.IsValid()) {
            HYP_LOG_ONCE(UI, Warning, "No font atlas texture for text size {}", text_size);
        }

        UpdateMaterial(false);
        UpdateMeshData(false);

        SetNeedsRepaintFlag(true);
    }
}

void UIText::UpdateMeshData_Internal()
{
    HYP_SCOPE;

    UIObject::UpdateMeshData_Internal();

    const RC<FontAtlas> &font_atlas = GetFontAtlasOrDefault();

    if (!font_atlas) {
        HYP_LOG_ONCE(UI, Warning, "No font atlas, cannot update text mesh data");

        return;
    }

    BoundingBox parent_aabb_clamped;

    if (UIObject *parent = GetParentUIObject()) {
        parent_aabb_clamped = parent->GetAABBClamped();
    }

    MeshComponent &mesh_component = GetScene()->GetEntityManager()->GetComponent<MeshComponent>(GetEntity());

    const Vec2f position = GetAbsolutePosition();
    const float text_size = GetTextSize();

    Array<Matrix4> instance_transforms;
    Array<Vec4f> instance_texcoords;
    Array<Vec4f> instance_offsets;
    Array<Vec4f> instance_sizes;

    ForEachCharacter(*font_atlas, m_text, GetParentBounds(), text_size, [&](const FontAtlasCharacterIterator &iter)
    {
        Transform character_transform;
        character_transform.SetScale(Vec3f(iter.glyph_dimensions.x * text_size, iter.glyph_dimensions.y * text_size, 1.0f));
        character_transform.GetTranslation().y += (iter.cell_dimensions.y - iter.glyph_dimensions.y) * text_size;
        character_transform.GetTranslation().y += iter.bearing_y * text_size;
        character_transform.GetTranslation() += Vec3f(iter.placement.x, iter.placement.y, 0.0f) * text_size;
        character_transform.UpdateMatrix();

        BoundingBox character_aabb = BoundingBox(Vec3f::Zero(), Vec3f::One()) * character_transform;
        character_aabb.min += Vec3f(position, 0.0f);
        character_aabb.max += Vec3f(position, 0.0f);

        BoundingBox character_aabb_clamped = character_aabb.Intersection(parent_aabb_clamped);

        Matrix4 instance_transform;
        instance_transform[0][0] = character_aabb_clamped.max.x - character_aabb_clamped.min.x;
        instance_transform[1][1] = character_aabb_clamped.max.y - character_aabb_clamped.min.y;
        instance_transform[2][2] = 1.0f;
        instance_transform[0][3] = character_aabb_clamped.min.x;
        instance_transform[1][3] = character_aabb_clamped.min.y;
        instance_transform[2][3] = 0.0f;

        instance_transforms.PushBack(instance_transform);

        const Vec2f size = character_aabb.GetExtent().GetXY();
        const Vec2f clamped_size = character_aabb_clamped.GetExtent().GetXY();
        const Vec2f clamped_offset = character_aabb.min.GetXY() - character_aabb_clamped.min.GetXY();

        Vec2f texcoord_start = Vec2f(iter.char_offset) * iter.atlas_pixel_size;
        Vec2f texcoord_end = (Vec2f(iter.char_offset) + (iter.glyph_dimensions * 64.0f)) * iter.atlas_pixel_size;

        instance_texcoords.PushBack(Vec4f(texcoord_start, texcoord_end));
        instance_offsets.PushBack(Vec4f(clamped_offset, 0.0f, 0.0f));
        instance_sizes.PushBack(Vec4f(size, clamped_size));
    });

    mesh_component.instance_data.num_instances = instance_transforms.Size();
    mesh_component.instance_data.SetBufferData(0, instance_transforms.Data(), instance_transforms.Size());
    mesh_component.instance_data.SetBufferData(1, instance_texcoords.Data(), instance_texcoords.Size());
    mesh_component.instance_data.SetBufferData(2, instance_offsets.Data(), instance_offsets.Size());
    mesh_component.instance_data.SetBufferData(3, instance_sizes.Data(), instance_sizes.Size());

    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

bool UIText::Repaint_Internal()
{
    Vec2i size = GetActualSize();

    if (size.x * size.y <= 0) {
        HYP_LOG_ONCE(UI, Warning, "Cannot repaint UI text for element: {}, size <= zero", GetName());

        return false;
    }

    size = MathUtil::Max(size, Vec2i::One());

    HYP_LOG(UI, Debug, "Repaint text '{}', {}", GetText(), m_text_aabb_without_bearing);

    return true;
}

MaterialAttributes UIText::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

Material::ParameterTable UIText::GetMaterialParameters() const
{
    return {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(GetTextColor()) }
    };
}

Material::TextureSet UIText::GetMaterialTextures() const
{
    if (!m_current_font_atlas_texture.IsValid()) {
        return UIObject::GetMaterialTextures();
    }

    return {
        { MaterialTextureKey::ALBEDO_MAP, m_current_font_atlas_texture }
    };
}

void UIText::Update_Internal(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    if (m_deferred_updates) {
        if (m_deferred_updates & UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA) {
            UpdateRenderData();
        }
    }

    UIObject::Update_Internal(delta);
}

void UIText::UpdateSize_Internal(bool update_children)
{
    HYP_SCOPE;

    UpdateTextAABB();

    UIObject::UpdateSize_Internal(update_children);

    const Vec3f extent_with_bearing = m_text_aabb_with_bearing.GetExtent();
    const Vec3f extent_without_bearing = m_text_aabb_without_bearing.GetExtent();

    m_actual_size.y *= extent_with_bearing.y / extent_without_bearing.y;
}

BoundingBox UIText::CalculateInnerAABB_Internal() const
{
    return m_text_aabb_without_bearing * Vec3f(Vec2f(GetTextSize()), 1.0f);
}

void UIText::OnComputedVisibilityChange_Internal()
{
    HYP_SCOPE;

    UIObject::OnComputedVisibilityChange_Internal();

    if (GetComputedVisibility()) {
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
    }
}

void UIText::OnFontAtlasUpdate_Internal()
{
    HYP_SCOPE;

    UIObject::OnFontAtlasUpdate_Internal();

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

void UIText::OnTextSizeUpdate_Internal()
{
    HYP_SCOPE;

    UIObject::OnTextSizeUpdate_Internal();

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

Vec2i UIText::GetParentBounds() const
{
    Vec2i parent_bounds;

    if (const UIObject *parent = GetParentUIObject()) {
        const UIObjectSize parent_size = parent->GetSize();

        if (!(parent_size.GetFlagsX() & UIObjectSize::AUTO)) {
            parent_bounds.x = parent->GetActualSize().x;
        }

        if (!(parent_size.GetFlagsY() & UIObjectSize::AUTO)) {
            parent_bounds.y = parent->GetActualSize().y;
        }
    }

    return parent_bounds;
}

#pragma endregion UIText

} // namespace hyperion