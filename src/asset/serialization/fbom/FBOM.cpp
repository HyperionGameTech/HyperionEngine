/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <asset/ByteWriter.hpp>

#include <core/object/HypClassRegistry.hpp>

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

FBOMMarshalerBase *FBOM::GetMarshal(TypeID type_id) const
{
    for (const auto &it : m_marshals) {
        if (!it.second) {
            continue;
        }

        if (it.second->GetTypeID() == type_id) {
            return it.second.Get();
        }
    }

    // No custom marshal found.

    // If the type has a HypClass defined, then use the default HypClass instance marshal
    if (const HypClass *hyp_class = GetClass(type_id)) {
        AssertThrow(m_hyp_class_instance_marshal != nullptr);
        return m_hyp_class_instance_marshal.Get();
    }

    return nullptr;
}

FBOMMarshalerBase *FBOM::GetMarshal(const ANSIStringView &type_name) const
{
    const auto it = m_marshals.FindAs(type_name);

    if (it != m_marshals.End()) {
        return it->second.Get();
    }

    if (const HypClass *hyp_class = HypClassRegistry::GetInstance().GetClass(type_name)) {
        AssertThrow(m_hyp_class_instance_marshal != nullptr);
        return m_hyp_class_instance_marshal.Get();
    }

    return nullptr;
}

} // namespace hyperion::fbom