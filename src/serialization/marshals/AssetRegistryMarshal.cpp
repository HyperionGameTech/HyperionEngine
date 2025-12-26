/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <asset/AssetRegistry.hpp>

#include <HyperionEngine.hpp>

namespace Hyperion::serialization {

class AssetRegistryMarshal : public ObjectMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = ObjectMarshal::Serialize(in, out))
        {
            return err;
        }

        const AssetRegistry& inObject = in.Get<AssetRegistry>();

        for (const Handle<AssetPackage>& package : inObject.GetPackages())
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

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override
    {
        Handle<AssetRegistry> assetRegistry = CreateObject<AssetRegistry>();
        out = BoxedValue(assetRegistry);

        if (FBOMResult err = ObjectMarshal::Deserialize_Internal(context, in, AssetRegistry::StaticClass(), out))
        {
            return err;
        }

        AssetPackageSet packages;

        for (const FBOMObject& child : in.GetChildren())
        {
            if (child.GetType().IsOrExtends("AssetPackage"))
            {
                const Handle<AssetPackage>& assetPackage = child.m_deserializedObject->Get<Handle<AssetPackage>>();

                if (!assetPackage)
                {
                    continue;
                }

                packages.Set(assetPackage);
            }
        }

        assetRegistry->SetPackages(packages);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(AssetRegistry, AssetRegistryMarshal);

} // namespace Hyperion::serialization
