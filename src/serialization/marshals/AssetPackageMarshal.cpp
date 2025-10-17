/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/reflection/HypData.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <asset/AssetRegistry.hpp>

#include <HyperionEngine.hpp>

namespace hyperion::serialization {

class AssetPackageMarshal : public HypClassInstanceMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        const AssetPackage& inObject = in.Get<AssetPackage>();

        FBOMResult result;

        inObject.ForEachSubpackage([&](const Handle<AssetPackage>& subpackage)
            {
                if (!subpackage.IsValid())
                {
                    return IterationResult::CONTINUE;
                }

                if (FBOMResult err = out.AddChild(*subpackage))
                {
                    result = err;
                    return IterationResult::STOP;
                }

                return IterationResult::CONTINUE;
            });

        return result;
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Handle<AssetPackage> assetPackage = CreateObject<AssetPackage>();
        out = HypData(assetPackage);

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, AssetPackage::Class(), out))
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

                continue;
            }
        }

        assetPackage->SetSubpackages(packages);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(AssetPackage, AssetPackageMarshal);

} // namespace hyperion::serialization