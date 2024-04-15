/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/Assets.hpp>

#include <dotnet/runtime/scene/ManagedNode.hpp>
#include <dotnet/runtime/ManagedHandle.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT void Asset_GetNode(EnqueuedAsset *asset, ManagedNode *node)
{
    AssertThrow(asset != nullptr);

    if (!asset->result) {
        return;
    }
    
    auto value = asset->ExtractAs<Node>();

    *node = CreateManagedNodeFromNodeProxy(std::move(value));
}

HYP_EXPORT void Asset_GetTexture(EnqueuedAsset *asset, ManagedHandle *handle)
{
    AssertThrow(asset != nullptr);

    if (!asset->result) {
        return;
    }

    auto value = asset->ExtractAs<Texture>();

    *handle = CreateManagedHandleFromHandle(std::move(value));
}

} // extern "C"