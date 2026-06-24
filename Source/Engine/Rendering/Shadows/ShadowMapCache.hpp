/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/Memory/Pimpl.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Light;
class View;
class ShadowMap;

enum ShadowMapType : uint32;

struct ShadowMapCacheKey
{
    union
    {
        uint64 hash;

        struct
        {
            uint32 lightHash : 32;
            uint32 cameraHash : 31;
            uint32 isCameraDependent : 1;
        };
    };

    HYP_FORCE_INLINE bool IsCameraDependent() const
    {
        return isCameraDependent != 0;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const ShadowMapCacheKey& other) const
    {
        return hash == other.hash;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const ShadowMapCacheKey& other) const
    {
        return hash != other.hash;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const ShadowMapCacheKey& other) const
    {
        return hash < other.hash;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode(HashCode::ValueType(hash));
    }
};

static_assert(sizeof(ShadowMapCacheKey) == sizeof(uint64) && alignof(ShadowMapCacheKey) == alignof(uint64));

extern ShadowMapCacheKey MakeShadowMapCacheKey(Light* light, View* view);

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
        View*& outShadowViewDynamic,
        View*& outShadowViewStatic) const;

    bool Remove(const ShadowMapCacheKey& key);

    // call on simulation thread
    void Update();

private:
    Pimpl<class ShadowMapCacheImpl> m_impl;
};

} // namespace Hyperion
