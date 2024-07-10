/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_TEXT_HPP
#define HYPERION_UI_TEXT_HPP

#include <ui/UIObject.hpp>
#include <ui/UIStage.hpp>

#include <core/Base.hpp>
#include <core/containers/String.hpp>
#include <core/Handle.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Texture.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <math/Transform.hpp>
#include <math/Vector2.hpp>

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

struct UITextOptions
{
    float line_height = 1.0f;
};

class HYP_API UIText : public UIObject
{
public:
    UIText(UIStage *stage, NodeProxy node_proxy);
    UIText(const UIText &other)                 = delete;
    UIText &operator=(const UIText &other)      = delete;
    UIText(UIText &&other) noexcept             = delete;
    UIText &operator=(UIText &&other) noexcept  = delete;
    virtual ~UIText() override                  = default;

    virtual void Init() override;

    /*! \brief Gets the text to render.
     * 
     * \return The text to render. */
    HYP_FORCE_INLINE const String &GetText() const
        { return m_text; }

    /*! \brief Sets the text to render.
     * 
     * \param text The text to set. */
    void SetText(const String &text);

    /*! \brief Gets the font atlas used for rendering the text, if any.
     * \note If the font atlas is null, the default font atlas from the parent UIStage is used, but not returned from this function.
     * 
     * \return The font atlas used for rendering the text. */
    HYP_FORCE_INLINE const RC<FontAtlas> &GetFontAtlas() const
        { return m_font_atlas; }

    /*! \brief Sets the font atlas to use for rendering the text.
     *  If the font atlas is null, the default font atlas from the parent UIStage is used.
     * 
     * \param font_atlas The font atlas to set. */
    void SetFontAtlas(RC<FontAtlas> font_atlas);

    /*! \brief Gets the options for rendering the text.
     * 
     * \return The options for rendering the text. */
    HYP_FORCE_INLINE const UITextOptions &GetOptions() const
        { return m_options; }

    /*! \brief Sets the options for rendering the text.
     * 
     * \param options The options to set. */
    HYP_FORCE_INLINE void SetOptions(const UITextOptions &options)
        { m_options = options; }

    /*! \brief Overriden from UIObject to return false as text is not focusable
     * 
     * \return False */
    virtual bool AcceptsFocus() const override
        { return false; }

protected:
    virtual BoundingBox CalculateAABB() const override;
    
    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual Material::ParameterTable GetMaterialParameters() const override;
    virtual Material::TextureSet GetMaterialTextures() const override;

    virtual void UpdateSize(bool update_children = true) override;

    void UpdateMesh();

    FontAtlas *GetFontAtlasOrDefault() const;

    String          m_text;
    RC<FontAtlas>   m_font_atlas;

    UITextOptions   m_options;

private:
    BoundingBox     m_text_aabb;
};

} // namespace hyperion

#endif