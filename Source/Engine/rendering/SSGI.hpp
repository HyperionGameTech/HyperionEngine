/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderObject.hpp>

#include <Core/config/Config.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/utilities/EnumFlags.hpp>

namespace Hyperion {

class GBuffer;
class View;

class SSGI : public FullScreenPass
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    explicit SSGI(GBuffer* gbuffer);

    ~SSGI() override;

    HYP_FORCE_INLINE const Handle<Texture>& GetResultTexture() const
    {
        return m_resultTexture;
    }

    const Handle<Texture>& GetFinalResultTexture() const;

    void Create() override;
    void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    static constexpr uint32 NumDownsamplePasses = 4;
    
    Handle<Texture> m_resultTexture;

    Handle<Texture> m_downsampleTextures[NumDownsamplePasses];
    UniquePtr<FullScreenPass> m_upsamplePasses[NumDownsamplePasses];

    UniquePtr<TemporalBlending> m_temporalBlending;
};

} // namespace Hyperion
