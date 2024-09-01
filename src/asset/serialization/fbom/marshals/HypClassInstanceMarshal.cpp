/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion::fbom {

FBOMResult HypClassInstanceMarshal::Serialize(ConstAnyRef in, FBOMObject &out) const
{
    const HypClass *hyp_class = GetClass(in.GetTypeID());

    if (!hyp_class) {
        return { FBOMResult::FBOM_ERR, "Cannot serialize object using HypClassInstanceMarshal, object has no associated HypClass" };
    }

    if (!hyp_class->GetAttribute("serializable")) {
        return { FBOMResult::FBOM_ERR, "Cannot serialize object using HypClassInstanceMarshal, HypClass does not have the \"serializable\" attribute" };
    }

    for (HypProperty *property : hyp_class->GetProperties()) {
        AssertThrow(property != nullptr);

        if (!property->HasGetter()) {
            continue;
        }

        out.SetProperty(property->name, property->InvokeGetter_Serialized(in));
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize(const FBOMObject &in, HypData &out) const
{
    const HypClass *hyp_class = in.GetHypClass();

    if (!hyp_class) {
        return { FBOMResult::FBOM_ERR, "Cannot serialize object using HypClassInstanceMarshal, object has no associated HypClass" };
    }

    hyp_class->CreateInstance(out);
    
    for (const KeyValuePair<Name, FBOMData> &it : in.GetProperties()) {
        if (const HypProperty *property = hyp_class->GetProperty(it.first)) {
            if (!property->HasSetter()) {
                HYP_LOG(Serialization, LogLevel::WARNING, "Property {} on HypClass {} has no setter", it.first, hyp_class->GetName());

                continue;
            }

            property->InvokeSetter_Serialized(out.ToRef(), it.second);
        } else {
            HYP_LOG(Serialization, LogLevel::WARNING, "No property {} on HypClass {}", it.first, hyp_class->GetName());
        }
    }

    return { FBOMResult::FBOM_OK };
}

} // namespace hyperion::fbom