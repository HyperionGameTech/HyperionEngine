/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <ui/UIObject.hpp>
#include <ui/UIStage.hpp>

#include <core/containers/String.hpp>

#include <core/reflection/Handle.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Vector2.hpp>

#include <rendering/Shared.hpp>

#include <ui/font/FontAtlas.hpp>

#include <scene/Scene.hpp>

#include <core/Types.hpp>

namespace Hyperion {

class Event;
class InputManager;

class Texture;

struct UITextOptions
{
    float lineHeight = 1.0f;
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
    HYP_FORCE_INLINE const Handle<FontAtlas>& GetFontAtlas() const
    {
        return m_fontAtlas;
    }

    /*! \brief Sets the font atlas to use for rendering the text.
     *  If the font atlas is null, the default font atlas from the parent UIStage is used.
     *
     * \param fontAtlas The font atlas to set. */
    void SetFontAtlas(const Handle<FontAtlas>& fontAtlas);

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
    Vec2f GetCharacterOffset(int characterIndex) const;

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
    virtual MaterialParameters GetMaterialParameters() const override;
    virtual MaterialTextures GetMaterialTextures() const override;

    virtual void UpdateSize_Internal(bool updateChildren) override;

    virtual void OnFontAtlasUpdate_Internal() override;

    virtual void UpdateMeshData_Internal() override;

    virtual void Update_Internal(float delta) override;

    const Handle<FontAtlas>& GetFontAtlasOrDefault() const;

    void UpdateTextAABB();

    Handle<FontAtlas> m_fontAtlas;

    UITextOptions m_options;

private:
    virtual void OnTextSizeUpdate_Internal() override;
    virtual void UpdateMaterial_Internal() override;

    Vec2i GetParentBounds() const;

    BoundingBox m_textAabbWithBearing;
    BoundingBox m_textAabbWithoutBearing;

    Handle<Texture> m_currentFontAtlasTexture;

    Array<Vec2f> m_characterOffsets;
};

} // namespace Hyperion
