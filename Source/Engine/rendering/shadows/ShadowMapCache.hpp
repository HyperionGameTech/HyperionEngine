/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/reflection/Handle.hpp>

namespace Hyperion {

class Light;
class View;
class ShadowMap;

enum ShadowMapType : uint32;

class ShadowMapCache
{
public:
    ShadowMapCache();
    ~ShadowMapCache();

    void Initialize();
    void Shutdown();

    GpuImage* GetAtlasImage() const;
    GpuImageView* GetAtlasImageView() const;

    GpuImage* GetPointLightShadowMapImage() const;
    GpuImageView* GetPointLightShadowMapImageView() const;

    HYP_NODISCARD View* GetOrCreateShadowView(
        View* view,
        Light* light,
        uint32 cascadeIndex,
        bool isStatic) const;

    View* TryGetShadowView(
        View* view,
        Light* light,
        uint32 cascadeIndex,
        bool isStatic) const;

    ShadowMap* GetShadowMap(
        Light* light,
        View* view,
        uint32 cascadeIndex,
        Span<View*>& outShadowViewsDynamic,
        Span<View*>& outShadowViewsStatic) const;

    bool Remove(Light* light, View* view);

private:
    Pimpl<class ShadowMapCacheImpl> m_impl;
};

} // namespace Hyperion
