/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <core/Defines.hpp>

namespace hyperion {
class GpuImageViewBase : public RenderObject<GpuImageViewBase>
{
public:
    virtual ~GpuImageViewBase() override = default;

    HYP_FORCE_INLINE const GpuImageRef& GetImage() const
    {
        return m_image;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;
    virtual RendererResult Destroy() = 0;

protected:
    GpuImageViewBase(
        const GpuImageRef& image)
        : m_image(image),
          m_mipIndex(0),
          m_numMips(0),
          m_faceIndex(0),
          m_numFaces(0)
    {
    }

    GpuImageViewBase(
        const GpuImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 faceIndex,
        uint32 numFaces)
        : m_image(image),
          m_mipIndex(mipIndex),
          m_numMips(numMips),
          m_faceIndex(faceIndex),
          m_numFaces(numFaces)
    {
    }

    GpuImageRef m_image;
    uint32 m_mipIndex;
    uint32 m_numMips;
    uint32 m_faceIndex;
    uint32 m_numFaces;
};

} // namespace hyperion
