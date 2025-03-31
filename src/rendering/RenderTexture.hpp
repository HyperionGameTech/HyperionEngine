/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_TEXTURE_HPP
#define HYPERION_RENDER_TEXTURE_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/containers/Bitset.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/RenderResource.hpp>

#include <Types.hpp>

namespace hyperion {

class Texture;

class TextureRenderResource final : public RenderResourceBase
{
public:
    TextureRenderResource(Texture *texture);
    virtual ~TextureRenderResource() override;

    HYP_FORCE_INLINE Texture *GetTexture() const
        { return m_texture; }

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    Texture *m_texture;
};

} // namespace hyperion

#endif