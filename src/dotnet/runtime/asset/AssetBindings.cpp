#include <asset/Assets.hpp>

#include <dotnet/runtime/scene/ManagedNode.hpp>
#include <dotnet/runtime/ManagedHandle.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    ManagedNode Asset_GetNode(EnqueuedAsset *asset)
    {
        AssertThrow(asset != nullptr);

        if (!asset->result) {
            return { };
        }
        
        auto value = asset->ExtractAs<Node>();

        return CreateManagedNodeFromNodeProxy(std::move(value));
    }

    ManagedHandle Asset_GetTexture(EnqueuedAsset *asset)
    {
        AssertThrow(asset != nullptr);

        if (!asset->result) {
            return { };
        }

        auto value = asset->ExtractAs<Texture>();

        return CreateManagedHandleFromHandle(std::move(value));
    }

    // @TODO Others
}