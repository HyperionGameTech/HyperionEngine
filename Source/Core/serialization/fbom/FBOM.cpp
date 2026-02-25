/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/serialization/fbom/FBOM.hpp>
#include <Core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/reflection/ClassRegistry.hpp>
#include <Core/reflection/Class.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <Core/filesystem/FsUtil.hpp>

namespace Hyperion::serialization {

static constexpr bool MarshalParentClasses = true;

FBOM& FBOM::GetInstance()
{
    static FBOM s_instance;

    return s_instance;
}

FBOM::FBOM()
    : m_objectMarshal(MakeUnique<ObjectMarshal>())
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
    if (MarshalParentClasses)
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
        AssertDebug(m_objectMarshal != nullptr);
        return m_objectMarshal.Get();
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
    if (MarshalParentClasses)
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
        AssertDebug(m_objectMarshal != nullptr);
        return m_objectMarshal.Get();
    }

    return nullptr;
}

} // namespace Hyperion::serialization