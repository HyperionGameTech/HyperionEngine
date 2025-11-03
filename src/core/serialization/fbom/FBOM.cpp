/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/reflection/ClassRegistry.hpp>
#include <core/reflection/Class.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/filesystem/FsUtil.hpp>

namespace hyperion::serialization {

static const bool g_marshalParentClasses = true;

FBOM& FBOM::GetInstance()
{
    static FBOM s_instance;

    return s_instance;
}

FBOM::FBOM()
    : m_ObjectMarshal(MakeUnique<ObjectMarshal>())
{
}

FBOM::~FBOM()
{
}

void FBOM::RegisterLoader(TypeId typeId, ANSIStringView name, UniquePtr<FBOMMarshalerBase>&& marshal)
{
    AssertDebug(marshal != nullptr);

    m_marshals.Set(typeId, Pair<ANSIString, UniquePtr<FBOMMarshalerBase>> { name, std::move(marshal) });
}

FBOMMarshalerBase* FBOM::GetMarshal(TypeId typeId, bool allowFallback) const
{
    const Class* cls = GetClass(typeId);

    // Check if Class disallows serialization
    if (cls && !cls->CanSerialize())
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

    if (cls == nullptr || cls->GetSerializationMode() & ClassSerializationMode::USE_MARSHAL_CLASS)
    {
        if (FBOMMarshalerBase* marshal = findMarshalForTypeId(typeId))
        {
            return marshal;
        }
    }

    if (!cls)
    {
        return nullptr;
    }

    // Find marshal for parent classes
    if (g_marshalParentClasses)
    {
        const Class* parentClass = cls->GetParent();

        while (parentClass)
        {

            if (parentClass->GetSerializationMode() & ClassSerializationMode::USE_MARSHAL_CLASS)
            {
                if (FBOMMarshalerBase* marshal = findMarshalForTypeId(parentClass->GetTypeId()))
                {
                    return marshal;
                }
            }

            parentClass = parentClass->GetParent();
        }
    }

    // No custom marshal found

    if (allowFallback && (cls->GetSerializationMode() & (ClassSerializationMode::MEMBERWISE | ClassSerializationMode::BITWISE)))
    {
        // If the type has a Class defined, then use the default Class instance marshal
        AssertDebug(m_ObjectMarshal != nullptr);
        return m_ObjectMarshal.Get();
    }

    return nullptr;
}

FBOMMarshalerBase* FBOM::GetMarshal(ANSIStringView typeName, bool allowFallback) const
{
    const Class* cls = ClassRegistry::GetInstance().GetClass(typeName);

    // Check if Class disallows serialization
    if (cls && !cls->CanSerialize())
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

    if (cls == nullptr || cls->GetSerializationMode() & ClassSerializationMode::USE_MARSHAL_CLASS)
    {
        if (FBOMMarshalerBase* marshal = findMarshalForTypeName(typeName))
        {
            return marshal;
        }
    }

    if (!cls)
    {
        return nullptr;
    }

    // Find marshal for parent classes
    if (g_marshalParentClasses)
    {
        const Class* parentClass = cls->GetParent();

        while (parentClass)
        {
            if (parentClass->GetSerializationMode() & ClassSerializationMode::USE_MARSHAL_CLASS)
            {
                if (FBOMMarshalerBase* marshal = findMarshalForTypeId(parentClass->GetTypeId()))
                {
                    return marshal;
                }
            }

            parentClass = parentClass->GetParent();
        }
    }

    if (allowFallback && (cls->GetSerializationMode() & (ClassSerializationMode::MEMBERWISE | ClassSerializationMode::BITWISE)))
    {
        AssertDebug(m_ObjectMarshal != nullptr);
        return m_ObjectMarshal.Get();
    }

    return nullptr;
}

} // namespace hyperion::serialization