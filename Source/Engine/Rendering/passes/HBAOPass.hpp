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

    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

protected:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return true;
    }

    virtual void Resize_Internal(Vec2u newSize) override;

private:
    DescriptorSetRef m_descriptorSet;
    GpuBufferRef m_cbuffer;
};

} // namespace Hyperion
