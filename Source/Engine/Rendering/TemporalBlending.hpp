/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Constants.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class GBuffer;
class Texture;
struct RenderSetup;

enum class TextureFormat : uint8;

static constexpr double DefaultTemporalBlendingFeedback = 0.9;

enum class TemporalBlendTechnique
{
    TECHNIQUE_0,
    TECHNIQUE_1,
    TECHNIQUE_2,
    TECHNIQUE_3,

    TECHNIQUE_4 // Progressive blending for path tracing
};

class TemporalBlending
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    TemporalBlending(
        const Vec2u& extent,
        TemporalBlendTechnique technique,
        double feedback,
        const GpuImageViewRef& inputImageView,
        GBuffer* gbuffer);

    TemporalBlending(
        const Vec2u& extent,
        TextureFormat imageFormat,
        TemporalBlendTechnique technique,
        double feedback,
        const FramebufferRef& inputFramebuffer,
        GBuffer* gbuffer);

    TemporalBlending(
        const Vec2u& extent,
        TextureFormat imageFormat,
        TemporalBlendTechnique technique,
        double feedback,
        const GpuImageViewRef& inputImageView,
        GBuffer* gbuffer);

    TemporalBlending(const TemporalBlending& other) = delete;
    TemporalBlending& operator=(const TemporalBlending& other) = delete;
    ~TemporalBlending();

    HYP_FORCE_INLINE TemporalBlendTechnique GetTechnique() const
    {
        return m_technique;
    }

    HYP_FORCE_INLINE double GetFeedback() const
    {
        return m_feedback;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetResultTexture() const
    {
        return m_resultTexture;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetHistoryTexture() const
    {
        return m_historyTexture;
    }

    void ResetProgressiveBlending();

    void Create();
    void Render(Frame* frame, const RenderSetup& renderSetup);

    void Resize(Vec2u newSize);

private:
    void Resize_Internal(Vec2u newSize);

    void GetShaderProperties(struct ShaderPropertySet& outProperties) const;

    void CreateImages();

    Vec2u m_extent;
    TextureFormat m_imageFormat;
    TemporalBlendTechnique m_technique;
    double m_feedback;
    GBuffer* m_gbuffer;

    uint16 m_blendingFrameCounter;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_cbuffers;

    GpuImageViewRef m_inputImageView;
    FramebufferRef m_inputFramebuffer;

    Handle<Texture> m_resultTexture;
    Handle<Texture> m_historyTexture;

    DelegateHandler m_onGbufferResolutionChanged;

    bool m_isInitialized;
};

} // namespace Hyperion
