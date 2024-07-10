/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_IMAGE_HPP
#define HYPERION_UI_IMAGE_HPP

#include <ui/UIObject.hpp>

#include <rendering/Texture.hpp>

namespace hyperion {

class UIStage;

#pragma region UIImage

class HYP_API UIImage : public UIObject
{
public:
    UIImage(UIStage *stage, NodeProxy node_proxy);
    UIImage(const UIImage &other)                   = delete;
    UIImage &operator=(const UIImage &other)        = delete;
    UIImage(UIImage &&other) noexcept               = delete;
    UIImage &operator=(UIImage &&other) noexcept    = delete;
    virtual ~UIImage() override                     = default;

    virtual void Init() override;

    /*! \brief Gets the texture of the image.
     * 
     * \return A handle to the texture of the image. */
    HYP_FORCE_INLINE const Handle<Texture> &GetTexture() const
        { return m_texture; }

    /*! \brief Sets the texture of the image.
     * 
     * \param texture A handle to the texture to set. */
    void SetTexture(Handle<Texture> texture);

protected:
    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual Material::ParameterTable GetMaterialParameters() const override;
    virtual Material::TextureSet GetMaterialTextures() const override;

    Handle<Texture> m_texture;
};

#pragma endregion UIImage

} // namespace hyperion

#endif