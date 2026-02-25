/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/Method.hpp>
#include <Core/reflection/Property.hpp>
#include <Core/reflection/Field.hpp>
#include <Core/reflection/StaticField.hpp>
#include <Core/reflection/Member.hpp>

#include <Core/reflection/TypeId.hpp>
#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/Variant.hpp>

namespace Hyperion {

struct MemberVariant
{
    IMember* internal;

    MemberVariant()
        : internal(nullptr)
    {
    }

    MemberVariant(Property&& property)
        : internal(new Property(std::move(property)))
    {
    }

    MemberVariant(Method&& method)
        : internal(new Method(std::move(method)))
    {
    }

    MemberVariant(Field&& field)
        : internal(new Field(std::move(field)))
    {
    }

    MemberVariant(StaticField&& field)
        : internal(new StaticField(std::move(field)))
    {
    }

    MemberVariant(const MemberVariant& other) = delete;
    MemberVariant& operator=(const MemberVariant& other) = delete;

    MemberVariant(MemberVariant&& other) noexcept
        : internal(other.internal)
    {
        other.internal = nullptr;
    }

    MemberVariant& operator=(MemberVariant&& other) noexcept
    {
        if (this != &other)
        {
            delete internal;

            internal = other.internal;
            other.internal = nullptr;
        }

        return *this;
    }

    ~MemberVariant()
    {
        if (internal != nullptr)
        {
            delete internal;
            internal = nullptr;
        }
    }
};

} // namespace Hyperion
