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

void FBOM::RegisterLoader(TypeID type_id, ANSIStringView name, UniquePtr<FBOMMarshalerBase> &&marshal)
{
    AssertThrow(marshal != nullptr);

    HYP_LOG(Serialization, LogLevel::INFO, "Registered FBOM loader {}", name);

    m_marshals.Set(type_id, Pair<ANSIString, UniquePtr<FBOMMarshalerBase>> { name, std::move(marshal) });
}

FBOMMarshalerBase *FBOM::GetMarshal(TypeID type_id) const
{
    const auto it = m_marshals.Find(type_id);

    if (it != m_marshals.End()) {
        return it->second.second.Get();
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
    // const auto it = m_marshals.FindIf([&type_name](const auto &pair)
    // {
    //     return pair.second.first == type_name;
    // });

    // if (it != m_marshals.End()) {
    //     return it->second.second.Get();
    // }

    for (const auto &pair : m_marshals) {
        if (pair.second.first == type_name) {
            return pair.second.second.Get();
        }
    }

    if (const HypClass *hyp_class = HypClassRegistry::GetInstance().GetClass(type_name)) {
        AssertThrow(m_hyp_class_instance_marshal != nullptr);
        return m_hyp_class_instance_marshal.Get();
    }

    return nullptr;
}

} // namespace hyperion::fbom