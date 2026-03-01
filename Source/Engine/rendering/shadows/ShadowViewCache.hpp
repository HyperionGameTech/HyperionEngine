/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/reflection/Handle.hpp>

namespace Hyperion {

class Light;
class View;

struct ShadowViewCacheKey
{
    View* view;
    Light* light;

    HYP_FORCE_INLINE bool operator==(const ShadowViewCacheKey& other) const
    {
        return view == other.view
            && light == other.light;
    }

    HYP_FORCE_INLINE bool operator!=(const ShadowViewCacheKey& other) const
    {
        return !(operator==(other));
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(view)
            .Combine(light);
    }
};

class ShadowViewCache
{
public:
    ShadowViewCache();
    ~ShadowViewCache();

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

private:
    Pimpl<class ShadowViewCacheImpl> m_impl;
};

} // namespace Hyperion
