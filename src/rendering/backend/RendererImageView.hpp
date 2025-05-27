/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_IMAGE_VIEW_HPP
#define HYPERION_BACKEND_RENDERER_IMAGE_VIEW_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

class ImageViewBase : public RenderObject<ImageViewBase>
{
public:
    virtual ~ImageViewBase() override = default;

    HYP_FORCE_INLINE const ImageRef& GetImage() const
    {
        return m_image;
    }

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

protected:
    ImageViewBase(
        const ImageRef& image)
        : m_image(image),
          m_mip_index(0),
          m_num_mips(0),
          m_face_index(0),
          m_num_faces(0)
    {
    }

    ImageViewBase(
        const ImageRef& image,
        uint32 mip_index,
        uint32 num_mips,
        uint32 face_index,
        uint32 num_faces)
        : m_image(image),
          m_mip_index(mip_index),
          m_num_mips(num_mips),
          m_face_index(face_index),
          m_num_faces(num_faces)
    {
    }

    ImageRef m_image;
    uint32 m_mip_index;
    uint32 m_num_mips;
    uint32 m_face_index;
    uint32 m_num_faces;
};

} // namespace renderer
} // namespace hyperion

#endif