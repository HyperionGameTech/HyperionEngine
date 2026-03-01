/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/reflection/Handle.hpp>

namespace Hyperion {

class Light;
class View;

class ShadowViewCache
{
public:
    ShadowViewCache();
    ~ShadowViewCache();

    void GetOrCreateShadowView(
        View* view,
        Light* light,
        uint32 cascadeIndex,
        bool isStatic,
        View*& outView) const;

private:
    Pimpl<class ShadowViewCacheImpl> m_impl;
};

} // namespace Hyperion
