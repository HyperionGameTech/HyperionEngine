/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/Method.hpp>
#include <core/reflection/Property.hpp>
#include <core/reflection/Field.hpp>
#include <core/reflection/StaticField.hpp>
#include <core/reflection/HypMemberFwd.hpp>

#include <core/reflection/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

namespace Hyperion {

struct HypMember
{
    IHypMember* internal;

    HypMember()
        : internal(nullptr)
    {
    }

    HypMember(Property&& property)
        : internal(new Property(std::move(property)))
    {
    }

    HypMember(Method&& method)
        : internal(new Method(std::move(method)))
    {
    }

    HypMember(Field&& field)
        : internal(new Field(std::move(field)))
    {
    }

    HypMember(StaticField&& field)
        : internal(new StaticField(std::move(field)))
    {
    }

    HypMember(const HypMember& other) = delete;
    HypMember& operator=(const HypMember& other) = delete;

    HypMember(HypMember&& other) noexcept
        : internal(other.internal)
    {
        other.internal = nullptr;
    }

    HypMember& operator=(HypMember&& other) noexcept
    {
        if (this != &other)
        {
            delete internal;

            internal = other.internal;
            other.internal = nullptr;
        }

        return *this;
    }

    ~HypMember()
    {
        if (internal != nullptr)
        {
            delete internal;
            internal = nullptr;
        }
    }
};

} // namespace Hyperion
