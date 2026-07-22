/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class GBuffer;
class SSRPass;

enum CubemapType : uint32
{
    CMT_DEFAULT = 0,
    CMT_PARALLAX_CORRECTED,

    CMT_MAX
};

class ReflectionsPass final : public FullScreenPass
{
public:
    ReflectionsPass(Vec2u extent, GBuffer* gbuffer);
    ReflectionsPass(const ReflectionsPass& other) = delete;
    ReflectionsPass& operator=(const ReflectionsPass& other) = delete;
    virtual ~ReflectionsPass() override;


    bool ShouldRenderSSR() const;

    virtual void Create() override;
    virtual void Render(Frame* frame, const RenderSetup& rs) override;

    UniquePtr<SSRPass> ssrPass;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderCheckerboarded() const override
    {
        return false;
    }

    void CreateSSRPass();

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual void Resize_Internal(Vec2u newSize) override;

    bool m_isFirstFrame;
};

} // namespace Hyperion
