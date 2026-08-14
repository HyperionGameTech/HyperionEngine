/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Scene.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class GBuffer;
class Texture;
struct RenderSetup;

class TAAPass
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    TAAPass(const GpuImageViewRef& inputImageView, const Vec2u& extent, GBuffer* gbuffer);
    TAAPass(const TAAPass& other) = delete;
    TAAPass& operator=(const TAAPass& other) = delete;
    ~TAAPass();

    HYP_FORCE_INLINE const GpuImageViewRef& GetInputImageView() const
    {
        return m_inputImageView;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetResultTexture() const
    {
        return m_pingPongIndex == 0 ? m_historyTexture : m_resultTexture;
    }

    void Create();
    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    void CreateTextures();

    Vec2u m_extent;

    GpuImageViewRef m_inputImageView;
    GBuffer* m_gbuffer;

    Handle<Texture> m_resultTexture;
    Handle<Texture> m_historyTexture;

    uint8 m_pingPongIndex : 1;

    DelegateHandler m_onGbufferResolutionChanged;

    bool m_isInitialized;
};

} // namespace Hyperion
