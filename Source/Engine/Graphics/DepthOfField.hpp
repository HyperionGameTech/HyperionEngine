/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <rendering/FullScreenPass.hpp>

namespace Hyperion {

class GBuffer;

class DOFBlur
{
public:
    DOFBlur(const Vec2u& extent, GBuffer* gbuffer);
    DOFBlur(const DOFBlur& other) = delete;
    DOFBlur& operator=(const DOFBlur& other) = delete;
    ~DOFBlur();

    FullScreenPass* GetHorizontalBlurPass() const
    {
        return m_blurHorizontalPass.Get();
    }

    FullScreenPass* GetVerticalBlurPass() const
    {
        return m_blurVerticalPass.Get();
    }

    FullScreenPass* GetCombineBlurPass() const
    {
        return m_blurMixPass.Get();
    }

    void Create();
    void Destroy();

    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    GBuffer* m_gbuffer;

    Vec2u m_extent;

    UniquePtr<FullScreenPass> m_blurHorizontalPass;
    UniquePtr<FullScreenPass> m_blurVerticalPass;
    UniquePtr<FullScreenPass> m_blurMixPass;
};

} // namespace Hyperion
