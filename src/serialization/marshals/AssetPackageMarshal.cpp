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

namespace hyperion::fbom {

template <>
class FBOMMarshaler<AssetPackage> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject &out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out)) {
            return err;
        }

        const AssetPackage &in_object = in.Get<AssetPackage>();

        for (const Handle<AssetPackage> &package : in_object.GetSubpackages()) {
            if (!package.IsValid()) {
                continue;
            }

            if (FBOMResult err = out.AddChild(*package)) {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        Handle<AssetPackage> asset_package_handle = CreateObject<AssetPackage>();

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(in, AssetPackage::Class(), AnyRef(*asset_package_handle))) {
            return err;
        }

        AssetPackageSet packages;

        for (FBOMObject &subobject : *in.nodes) {
            if (subobject.GetType().IsOrExtends("AssetPackage")) {
                const Handle<AssetPackage> &asset_package = subobject.m_deserialized_object->Get<Handle<AssetPackage>>();

                if (!asset_package) {
                    continue;
                }

                packages.Set(asset_package);

                continue;
            }
        }

        asset_package_handle->SetSubpackages(packages);

        out = HypData(std::move(asset_package_handle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(AssetPackage, FBOMMarshaler<AssetPackage>);

} // namespace hyperion::fbom