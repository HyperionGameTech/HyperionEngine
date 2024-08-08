/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/HypClass.hpp>
#include <core/HypClassProperty.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion::fbom {

FBOMResult HypClassInstanceMarshal::Serialize(ConstAnyRef in, FBOMObject &out) const
{
    const HypClass *hyp_class = GetClass(in.GetTypeID());

    if (!hyp_class) {
        return { FBOMResult::FBOM_ERR, "Cannot serialize object using HypClassInstanceMarshal, object has no associated HypClass" };
    }

    out = fbom::FBOMObject(FBOMObjectType(hyp_class->GetName().LookupString()));

    const HashCode hash_code = hyp_class->GetInstanceHashCode(in);
    out.m_unique_id = UniqueID(hash_code);

    for (HypClassProperty *property : hyp_class->GetProperties()) {
        AssertThrow(property != nullptr);

        if (!property->HasGetter()) {
            continue;
        }

        fbom::FBOMData property_value = property->InvokeGetter(in);
        out.SetProperty(property->name, std::move(property_value));
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize(const FBOMObject &in, Any &out) const
{
    const HypClass *hyp_class = in.GetHypClass();

    if (!hyp_class) {
        return { FBOMResult::FBOM_ERR, "Cannot serialize object using HypClassInstanceMarshal, object has no associated HypClass" };
    }

    hyp_class->CreateInstance(out);
    
    for (const KeyValuePair<Name, FBOMData> &it : in.GetProperties()) {
        if (const HypClassProperty *property = hyp_class->GetProperty(it.first)) {
            if (!property->HasSetter()) {
                HYP_LOG(Serialization, LogLevel::WARNING, "Property {} on HypClass {} has no setter", it.first, hyp_class->GetName());

                continue;
            }

            property->InvokeSetter(out, it.second);
        } else {
            HYP_LOG(Serialization, LogLevel::WARNING, "No property {} on HypClass {}", it.first, hyp_class->GetName());
        }
    }

    return { FBOMResult::FBOM_OK };
}

} // namespace hyperion::fbom