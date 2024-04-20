/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOF_BLUR_HPP
#define HYPERION_DOF_BLUR_HPP

#include <rendering/FullScreenPass.hpp>

namespace hyperion {

using renderer::StorageImage;
using renderer::ImageView;
using renderer::Frame;

using renderer::Result;

class Engine;

class DOFBlur
{
public:
    DOFBlur(const Extent2D &extent);
    DOFBlur(const DOFBlur &other) = delete;
    DOFBlur &operator=(const DOFBlur &other) = delete;
    ~DOFBlur();

    const UniquePtr<FullScreenPass> &GetHorizontalBlurPass() const
        { return m_blur_horizontal_pass; }

    const UniquePtr<FullScreenPass> &GetVerticalBlurPass() const
        { return m_blur_vertical_pass; }

    const UniquePtr<FullScreenPass> &GetCombineBlurPass() const
        { return m_blur_mix_pass; }

    void Create();
    void Destroy();
    
    void Render(Frame *frame);

private:
    Extent2D m_extent;

    UniquePtr<FullScreenPass> m_blur_horizontal_pass;
    UniquePtr<FullScreenPass> m_blur_vertical_pass;
    UniquePtr<FullScreenPass> m_blur_mix_pass;
};

} // namespace hyperion

#endif