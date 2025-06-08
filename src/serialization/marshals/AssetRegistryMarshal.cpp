/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <asset/AssetRegistry.hpp>

#include <HyperionEngine.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<AssetRegistry> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        const AssetRegistry& in_object = in.Get<AssetRegistry>();

        for (const Handle<AssetPackage>& package : in_object.GetPackages())
        {
            if (!package.IsValid())
            {
                continue;
            }

            if (FBOMResult err = out.AddChild(*package))
            {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Handle<AssetRegistry> asset_registry_handle = CreateObject<AssetRegistry>();

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, AssetRegistry::Class(), AnyRef(*asset_registry_handle)))
        {
            return err;
        }

        AssetPackageSet packages;

        for (const FBOMObject& child : in.GetChildren())
        {
            if (child.GetType().IsOrExtends("AssetPackage"))
            {
                const Handle<AssetPackage>& asset_package = child.m_deserialized_object->Get<Handle<AssetPackage>>();

                if (!asset_package)
                {
                    continue;
                }

                packages.Set(asset_package);
            }
        }

        asset_registry_handle->SetPackages(packages);

        out = HypData(std::move(asset_registry_handle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(AssetRegistry, FBOMMarshaler<AssetRegistry>);

} // namespace hyperion::serialization