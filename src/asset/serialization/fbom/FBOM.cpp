/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <asset/ByteWriter.hpp>

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::fbom {

static constexpr bool g_marshal_parent_classes = false;

FBOM &FBOM::GetInstance()
{
    static FBOM instance;

    return instance;
}

FBOM::FBOM()
    : m_hyp_class_instance_marshal(MakeUnique<HypClassInstanceMarshal>())
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

FBOMMarshalerBase *FBOM::GetMarshal(TypeID type_id, bool allow_fallback) const
{
    const HypClass *hyp_class = GetClass(type_id);

    auto FindMarshalForTypeID = [this](TypeID type_id) -> FBOMMarshalerBase *
    {
        const auto it = m_marshals.Find(type_id);

        if (it != m_marshals.End()) {
            return it->second.second.Get();
        }

        return nullptr;
    };

    if (FBOMMarshalerBase *marshal = FindMarshalForTypeID(type_id)) {
        return marshal;
    }

    if (!hyp_class) {
        return nullptr;
    }

    // Find marshal for parent classes
    if constexpr (g_marshal_parent_classes) {
        const HypClass *parent_hyp_class = hyp_class->GetParent();
        
        while (parent_hyp_class) {
            if (FBOMMarshalerBase *marshal = FindMarshalForTypeID(parent_hyp_class->GetTypeID())) {
                return marshal;
            }

            parent_hyp_class = parent_hyp_class->GetParent();
        }
    }

    // No custom marshal found

    if (allow_fallback) {
        // If the type has a HypClass defined, then use the default HypClass instance marshal
        AssertThrow(m_hyp_class_instance_marshal != nullptr);
        return m_hyp_class_instance_marshal.Get();
    }

    return nullptr;
}

FBOMMarshalerBase *FBOM::GetMarshal(ANSIStringView type_name, bool allow_fallback) const
{
    const HypClass *hyp_class = HypClassRegistry::GetInstance().GetClass(type_name);

    auto FindMarshalForTypeName = [this](ANSIStringView type_name) -> FBOMMarshalerBase *
    {
        const auto it = m_marshals.FindIf([&type_name](const auto &pair)
        {
            return pair.second.first == type_name;
        });

        if (it != m_marshals.End()) {
            return it->second.second.Get();
        }

        return nullptr;
    };

    auto FindMarshalForTypeID = [this](TypeID type_id) -> FBOMMarshalerBase *
    {
        const auto it = m_marshals.Find(type_id);

        if (it != m_marshals.End()) {
            return it->second.second.Get();
        }

        return nullptr;
    };

    if (FBOMMarshalerBase *marshal = FindMarshalForTypeName(type_name)) {
        return marshal;
    }

    if (!hyp_class) {
        return nullptr;
    }

    // Find marshal for parent classes
    if constexpr (g_marshal_parent_classes) {
        const HypClass *parent_hyp_class = hyp_class->GetParent();

        while (parent_hyp_class) {
            if (FBOMMarshalerBase *marshal = FindMarshalForTypeID(parent_hyp_class->GetTypeID())) {
                return marshal;
            }

            parent_hyp_class = parent_hyp_class->GetParent();
        }
    }

    if (allow_fallback) {
        AssertThrow(m_hyp_class_instance_marshal != nullptr);
        return m_hyp_class_instance_marshal.Get();
    }

    return nullptr;
}

} // namespace hyperion::fbom