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

static const bool g_marshalParentClasses = true;

FBOM& FBOM::GetInstance()
{
    static FBOM instance;

    return instance;
}

FBOM::FBOM()
    : m_hypClassInstanceMarshal(MakeUnique<HypClassInstanceMarshal>())
{
}

FBOM::~FBOM()
{
}

void FBOM::RegisterLoader(TypeId typeId, ANSIStringView name, UniquePtr<FBOMMarshalerBase>&& marshal)
{
    HYP_CORE_ASSERT(marshal != nullptr);

    m_marshals.Set(typeId, Pair<ANSIString, UniquePtr<FBOMMarshalerBase>> { name, std::move(marshal) });
}

FBOMMarshalerBase* FBOM::GetMarshal(TypeId typeId, bool allowFallback) const
{
    const HypClass* hypClass = GetClass(typeId);

    // Check if HypClass disallows serialization
    if (hypClass && !hypClass->CanSerialize())
    {
        return nullptr;
    }

    auto findMarshalForTypeId = [this](TypeId typeId) -> FBOMMarshalerBase*
    {
        const auto it = m_marshals.Find(typeId);

        if (it != m_marshals.End())
        {
            return it->second.second.Get();
        }

        return nullptr;
    };

    if (hypClass == nullptr || hypClass->GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
    {
        if (FBOMMarshalerBase* marshal = findMarshalForTypeId(typeId))
        {
            return marshal;
        }
    }

    if (!hypClass)
    {
        return nullptr;
    }

    // Find marshal for parent classes
    if (g_marshalParentClasses)
    {
        const HypClass* parentHypClass = hypClass->GetParent();

        while (parentHypClass)
        {

            if (parentHypClass->GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            {
                if (FBOMMarshalerBase* marshal = findMarshalForTypeId(parentHypClass->GetTypeId()))
                {
                    return marshal;
                }
            }

            parentHypClass = parentHypClass->GetParent();
        }
    }

    // No custom marshal found

    if (allowFallback && (hypClass->GetSerializationMode() & (HypClassSerializationMode::MEMBERWISE | HypClassSerializationMode::BITWISE)))
    {
        // If the type has a HypClass defined, then use the default HypClass instance marshal
        HYP_CORE_ASSERT(m_hypClassInstanceMarshal != nullptr);
        return m_hypClassInstanceMarshal.Get();
    }

    return nullptr;
}

FBOMMarshalerBase* FBOM::GetMarshal(ANSIStringView typeName, bool allowFallback) const
{
    const HypClass* hypClass = HypClassRegistry::GetInstance().GetClass(typeName);

    // Check if HypClass disallows serialization
    if (hypClass && !hypClass->CanSerialize())
    {
        return nullptr;
    }

    auto findMarshalForTypeName = [this](ANSIStringView typeName) -> FBOMMarshalerBase*
    {
        const auto it = m_marshals.FindIf([&typeName](const auto& pair)
            {
                return pair.second.first == typeName;
            });

        if (it != m_marshals.End())
        {
            return it->second.second.Get();
        }

        return nullptr;
    };

    auto findMarshalForTypeId = [this](TypeId typeId) -> FBOMMarshalerBase*
    {
        const auto it = m_marshals.Find(typeId);

        if (it != m_marshals.End())
        {
            return it->second.second.Get();
        }

        return nullptr;
    };

    if (hypClass == nullptr || hypClass->GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
    {
        if (FBOMMarshalerBase* marshal = findMarshalForTypeName(typeName))
        {
            return marshal;
        }
    }

    if (!hypClass)
    {
        return nullptr;
    }

    // Find marshal for parent classes
    if (g_marshalParentClasses)
    {
        const HypClass* parentHypClass = hypClass->GetParent();

        while (parentHypClass)
        {
            if (parentHypClass->GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            {
                if (FBOMMarshalerBase* marshal = findMarshalForTypeId(parentHypClass->GetTypeId()))
                {
                    return marshal;
                }
            }

            parentHypClass = parentHypClass->GetParent();
        }
    }

    if (allowFallback && (hypClass->GetSerializationMode() & (HypClassSerializationMode::MEMBERWISE | HypClassSerializationMode::BITWISE)))
    {
        HYP_CORE_ASSERT(m_hypClassInstanceMarshal != nullptr);
        return m_hypClassInstanceMarshal.Get();
    }

    return nullptr;
}

} // namespace hyperion::serialization