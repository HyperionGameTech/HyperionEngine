/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/Method.hpp>
#include <core/reflection/Property.hpp>
#include <core/reflection/Field.hpp>
#include <core/reflection/StaticField.hpp>
#include <core/reflection/Member.hpp>

#include <core/reflection/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

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
