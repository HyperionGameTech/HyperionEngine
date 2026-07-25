/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/IndexAllocator.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct ShadowMapAtlasElement;
class FullScreenPass;

HYP_ENUM()
enum ShadowMapType : uint8
{
    SMT_DIRECTIONAL,
    SMT_SPOT,
    SMT_OMNI
};

class ShadowMap
{
public:
    ShadowMap(
        ShadowMapType type,
        const ShadowMapAtlasElement& atlasElement,
        const GpuImageViewRef& imageView);

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    ~ShadowMap();

    HYP_FORCE_INLINE ShadowMapType GetShadowMapType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE ShadowMapAtlasElement* GetAtlasElement() const
    {
        return m_atlasElement;
    }

    HYP_FORCE_INLINE const GpuImageViewRef& GetImageView() const
    {
        return m_imageView;
    }

private:
    ShadowMapType m_type;
    ShadowMapAtlasElement* m_atlasElement;

    GpuImageViewRef m_imageView;
};

} // namespace Hyperion
