/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

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

FBOM::FBOM()    = default;
FBOM::~FBOM()   = default;

void FBOM::RegisterLoader(TypeID type_id, UniquePtr<FBOMMarshalerBase> &&marshal)
{
    AssertThrow(marshal != nullptr);

    const String &name = marshal->GetObjectType().name;

    HYP_LOG(Serialization, LogLevel::INFO, "Registered FBOM loader {}", name);

    m_marshals.Set(name, std::move(marshal));
}

FBOMMarshalerBase *FBOM::GetLoader(TypeID object_type_id) const
{
    for (const auto &it : m_marshals) {
        if (!it.second) {
            continue;
        }

        if (it.second->GetTypeID() == object_type_id) {
            return it.second.Get();
        }
    }

    return nullptr;
}

FBOMMarshalerBase *FBOM::GetLoader(const ANSIStringView &type_name) const
{
    const auto it = m_marshals.FindAs(type_name);

    if (it == m_marshals.End()) {
        return nullptr;
    }

    return it->second.Get();
}

} // namespace hyperion::fbom