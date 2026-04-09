/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <ui/UIObject.hpp>

namespace Hyperion {

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
    virtual void Init() override;

    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual MaterialTextures GetMaterialTextures() const override;

    Handle<Texture> m_texture;
};

#pragma endregion UIImage

} // namespace Hyperion
