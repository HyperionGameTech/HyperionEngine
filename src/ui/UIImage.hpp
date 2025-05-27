/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_IMAGE_HPP
#define HYPERION_UI_IMAGE_HPP

#include <ui/UIObject.hpp>

namespace hyperion {

class UIStage;
class Texture;

#pragma region UIImage

HYP_CLASS()

class HYP_API UIImage : public UIObject
{
    HYP_OBJECT_BODY(UIImage);

public:
    UIImage();
    UIImage(const UIImage& other) = delete;
    UIImage& operator=(const UIImage& other) = delete;
    UIImage(UIImage&& other) noexcept = delete;
    UIImage& operator=(UIImage&& other) noexcept = delete;
    virtual ~UIImage() override = default;

    virtual void Init() override;

    /*! \brief Gets the texture of the image.
     *
     * \return A handle to the texture of the image. */
    HYP_FORCE_INLINE const Handle<Texture>& GetTexture() const
    {
        return m_texture;
    }

    /*! \brief Sets the texture of the image.
     *
     * \param texture A handle to the texture to set. */
    void SetTexture(const Handle<Texture>& texture);

protected:
    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual Material::TextureSet GetMaterialTextures() const override;

    Handle<Texture> m_texture;
};

#pragma endregion UIImage

} // namespace hyperion

#endif