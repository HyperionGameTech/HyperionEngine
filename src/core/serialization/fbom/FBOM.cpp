/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/filesystem/FsUtil.hpp>

namespace hyperion::serialization {

static constexpr bool g_marshal_parent_classes = false;

FBOM& FBOM::GetInstance()
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

void FBOM::RegisterLoader(TypeID type_id, ANSIStringView name, UniquePtr<FBOMMarshalerBase>&& marshal)
{
    AssertThrow(marshal != nullptr);

    HYP_LOG(Serialization, Info, "Registered FBOM loader {}", name);

    m_marshals.Set(type_id, Pair<ANSIString, UniquePtr<FBOMMarshalerBase>> { name, std::move(marshal) });
}

FBOMMarshalerBase* FBOM::GetMarshal(TypeID type_id, bool allow_fallback) const
{
    const HypClass* hyp_class = GetClass(type_id);

    // Check if HypClass disallows serialization
    if (hyp_class && !hyp_class->CanSerialize())
    {
        return nullptr;
    }

    auto find_marshal_for_type_id = [this](TypeID type_id) -> FBOMMarshalerBase*
    {
        const auto it = m_marshals.Find(type_id);

        if (it != m_marshals.End())
        {
            return it->second.second.Get();
        }

        return nullptr;
    };

    if (hyp_class == nullptr || hyp_class->GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
    {
        if (FBOMMarshalerBase* marshal = find_marshal_for_type_id(type_id))
        {
            return marshal;
        }
    }

    if (!hyp_class)
    {
        return nullptr;
    }

    // Find marshal for parent classes
    if constexpr (g_marshal_parent_classes)
    {
        const HypClass* parent_hyp_class = hyp_class->GetParent();

        while (parent_hyp_class)
        {

            if (parent_hyp_class->GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            {
                if (FBOMMarshalerBase* marshal = find_marshal_for_type_id(parent_hyp_class->GetTypeID()))
                {
                    return marshal;
                }
            }

            parent_hyp_class = parent_hyp_class->GetParent();
        }
    }

    // No custom marshal found

    if (allow_fallback && (hyp_class->GetSerializationMode() & (HypClassSerializationMode::MEMBERWISE | HypClassSerializationMode::BITWISE)))
    {
        // If the type has a HypClass defined, then use the default HypClass instance marshal
        AssertThrow(m_hyp_class_instance_marshal != nullptr);
        return m_hyp_class_instance_marshal.Get();
    }

    return nullptr;
}

FBOMMarshalerBase* FBOM::GetMarshal(ANSIStringView type_name, bool allow_fallback) const
{
    const HypClass* hyp_class = HypClassRegistry::GetInstance().GetClass(type_name);

    // Check if HypClass disallows serialization
    if (hyp_class && !hyp_class->CanSerialize())
    {
        return nullptr;
    }

    auto find_marshal_for_type_name = [this](ANSIStringView type_name) -> FBOMMarshalerBase*
    {
        const auto it = m_marshals.FindIf([&type_name](const auto& pair)
            {
                return pair.second.first == type_name;
            });

        if (it != m_marshals.End())
        {
            return it->second.second.Get();
        }

        return nullptr;
    };

    auto find_marshal_for_type_id = [this](TypeID type_id) -> FBOMMarshalerBase*
    {
        const auto it = m_marshals.Find(type_id);

        if (it != m_marshals.End())
        {
            return it->second.second.Get();
        }

        return nullptr;
    };

    if (hyp_class == nullptr || hyp_class->GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
    {
        if (FBOMMarshalerBase* marshal = find_marshal_for_type_name(type_name))
        {
            return marshal;
        }
    }

    if (!hyp_class)
    {
        return nullptr;
    }

    // Find marshal for parent classes
    if constexpr (g_marshal_parent_classes)
    {
        const HypClass* parent_hyp_class = hyp_class->GetParent();

        while (parent_hyp_class)
        {
            if (parent_hyp_class->GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            {
                if (FBOMMarshalerBase* marshal = find_marshal_for_type_id(parent_hyp_class->GetTypeID()))
                {
                    return marshal;
                }
            }

            parent_hyp_class = parent_hyp_class->GetParent();
        }
    }

    if (allow_fallback && (hyp_class->GetSerializationMode() & (HypClassSerializationMode::MEMBERWISE | HypClassSerializationMode::BITWISE)))
    {
        AssertThrow(m_hyp_class_instance_marshal != nullptr);
        return m_hyp_class_instance_marshal.Get();
    }

    return nullptr;
}

} // namespace hyperion::serialization