/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::fbom {

FBOM &FBOM::GetInstance()
{
    static FBOM instance;

    return instance;
}

FBOM::FBOM()
    : m_hyp_class_instance_marshal(new HypClassInstanceMarshal())
{
}

FBOM::~FBOM()
{
}

void FBOM::RegisterLoader(TypeID type_id, UniquePtr<FBOMMarshalerBase> &&marshal)
{
    AssertThrow(marshal != nullptr);

    const auto &name = marshal->GetObjectType().name;

    HYP_LOG(Serialization, LogLevel::INFO, "Registered FBOM loader {}", name);

    m_marshals.Set(name, std::move(marshal));
}

FBOMMarshalerBase *FBOM::GetMarshal(const TypeAttributes &type_attributes) const
{
    for (const auto &it : m_marshals) {
        if (!it.second) {
            continue;
        }

        if (it.second->GetTypeID() == type_attributes.id) {
            return it.second.Get();
        }
    }

    // No custom marshal found.

    // If the type has a HypClass defined, then use the default HypClass instance marshal
    if (type_attributes.HasHypClass()) {
        return m_hyp_class_instance_marshal.Get();
    }

    if (type_attributes.IsPOD()) {
        // @TODO Use POD marshaller to serialize/deserialize struct
    }

    return nullptr;
}

FBOMMarshalerBase *FBOM::GetMarshal(const ANSIStringView &type_name) const
{
    const auto it = m_marshals.FindAs(type_name);

    if (it != m_marshals.End()) {
        return it->second.Get();
    }

    if (const HypClass *hyp_class = GetClass(type_name)) {
        return m_hyp_class_instance_marshal.Get();
    }

    return nullptr;
}

} // namespace hyperion::fbom