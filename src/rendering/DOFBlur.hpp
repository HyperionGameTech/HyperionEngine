/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOF_BLUR_HPP
#define HYPERION_DOF_BLUR_HPP

#include <rendering/FullScreenPass.hpp>

namespace hyperion {

class Engine;
class GBuffer;

class DOFBlur
{
public:
    DOFBlur(const Vec2u& extent, GBuffer* gbuffer);
    DOFBlur(const DOFBlur& other) = delete;
    DOFBlur& operator=(const DOFBlur& other) = delete;
    ~DOFBlur();

    const UniquePtr<FullScreenPass>& GetHorizontalBlurPass() const
    {
        return m_blur_horizontal_pass;
    }

    const UniquePtr<FullScreenPass>& GetVerticalBlurPass() const
    {
        return m_blur_vertical_pass;
    }

    const UniquePtr<FullScreenPass>& GetCombineBlurPass() const
    {
        return m_blur_mix_pass;
    }

    void Create();
    void Destroy();

    void Render(FrameBase* frame, RenderView* view);

private:
    GBuffer* m_gbuffer;

    Vec2u m_extent;

    UniquePtr<FullScreenPass> m_blur_horizontal_pass;
    UniquePtr<FullScreenPass> m_blur_vertical_pass;
    UniquePtr<FullScreenPass> m_blur_mix_pass;
};

} // namespace hyperion

#endif