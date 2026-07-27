/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/TemporalBlending.hpp>
#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class HBAO final : public FullScreenPass
{
public:
    HBAO(Vec2u extent, GBuffer* gbuffer);
    HBAO(const HBAO& other) = delete;
    HBAO& operator=(const HBAO& other) = delete;
    virtual ~HBAO() override;

    virtual void Create() override;
    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

    // Full resolution, bilaterally upsampled result - always use this rather than the
    // raw half-res output from the base FullScreenPass.
    virtual const GpuImageViewRef& GetFinalImageView() const override;

protected:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    // HBAO always renders at half res; the result is upsampled with a bilateral filter
    // in GetFinalImageView() before being consumed elsewhere.
    virtual bool ShouldRenderHalfRes() const override
    {
        return true;
    }

    virtual void Resize_Internal(Vec2u newSize) override;

private:
    UniquePtr<FullScreenPass> m_upsamplePass;
};

} // namespace Hyperion
