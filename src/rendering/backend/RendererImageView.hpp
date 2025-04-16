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

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create(const ImageBase *image) = 0;
    HYP_API virtual RendererResult Create(
        const ImageBase *image,
        uint32 mipmap_layer,
        uint32 num_mipmaps,
        uint32 face_layer,
        uint32 num_faces
    ) = 0;

    HYP_API virtual RendererResult Destroy() = 0;
};

} // namespace renderer
} // namespace hyperion

#endif