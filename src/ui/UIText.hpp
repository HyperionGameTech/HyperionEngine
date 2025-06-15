/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_TEXT_HPP
#define HYPERION_UI_TEXT_HPP

#include <ui/UIObject.hpp>
#include <ui/UIStage.hpp>

#include <core/Base.hpp>
#include <core/containers/String.hpp>
#include <core/Handle.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Vector2.hpp>

#include <scene/Scene.hpp>

#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {
namespace sys {
class SystemEvent;
} // namespace sys

using sys::SystemEvent;

class InputManager;

} // namespace hyperion

namespace hyperion {

class Texture;

struct UITextOptions
{
    float line_height = 1.0f;
};

struct UITextCharacter
{
    Matrix4 transform;
    Vec2f texcoord_start;
    Vec2f texcoord_end;
};

HYP_CLASS()
class HYP_API UIText : public UIObject
{
    HYP_OBJECT_BODY(UIText);

public:
    UIText();
    UIText(const UIText& other) = delete;
    UIText& operator=(const UIText& other) = delete;
    UIText(UIText&& other) noexcept = delete;
    UIText& operator=(UIText&& other) noexcept = delete;
    virtual ~UIText() override;

    /*! \brief Sets the text to render.
     *
     * \param text The text to set. */
    virtual void SetText(const String& text) override;

    /*! \brief Gets the font atlas used for rendering the text, if any.
     * \note If the font atlas is null, the default font atlas from the parent UIStage is used, but not returned from this function.
     *
     * \return The font atlas used for rendering the text. */
    HYP_FORCE_INLINE const RC<FontAtlas>& GetFontAtlas() const
    {
        return m_font_atlas;
    }

    /*! \brief Sets the font atlas to use for rendering the text.
     *  If the font atlas is null, the default font atlas from the parent UIStage is used.
     *
     * \param font_atlas The font atlas to set. */
    void SetFontAtlas(const RC<FontAtlas>& font_atlas);

    /*! \brief Gets the options for rendering the text.
     *
     * \return The options for rendering the text. */
    HYP_FORCE_INLINE const UITextOptions& GetOptions() const
    {
        return m_options;
    }

    /*! \brief Sets the options for rendering the text.
     *
     * \param options The options to set. */
    HYP_FORCE_INLINE void SetOptions(const UITextOptions& options)
    {
        m_options = options;
    }

    HYP_METHOD()
    Vec2f GetCharacterOffset(int character_index) const;

    /*! \brief Overriden from UIObject to return false as text is not focusable
     *
     * \return False */
    virtual bool AcceptsFocus() const override
    {
        return false;
    }

protected:
    virtual void Init() override;

    virtual BoundingBox CalculateInnerAABB_Internal() const override;

    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual Material::ParameterTable GetMaterialParameters() const override;
    virtual Material::TextureSet GetMaterialTextures() const override;

    virtual void UpdateSize_Internal(bool update_children) override;

    virtual void OnFontAtlasUpdate_Internal() override;

    virtual void UpdateMeshData_Internal() override;

    virtual bool Repaint_Internal() override;

    void UpdateTextAABB();
    void UpdateRenderData();

    virtual void Update_Internal(GameCounter::TickUnit delta) override;

    const RC<FontAtlas>& GetFontAtlasOrDefault() const;

    RC<FontAtlas> m_font_atlas;

    UITextOptions m_options;

private:
    virtual void OnTextSizeUpdate_Internal() override;

    Vec2i GetParentBounds() const;

    BoundingBox m_text_aabb_with_bearing;
    BoundingBox m_text_aabb_without_bearing;

    Handle<Texture> m_current_font_atlas_texture;

    Array<Vec2f> m_character_offsets;
};

} // namespace hyperion

#endif