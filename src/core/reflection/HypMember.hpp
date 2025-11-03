/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/HypMethod.hpp>
#include <core/reflection/HypProperty.hpp>
#include <core/reflection/Field.hpp>
#include <core/reflection/HypConstant.hpp>
#include <core/reflection/HypMemberFwd.hpp>

#include <core/reflection/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

namespace hyperion {

struct HypMember
{
    IHypMember* internal;

    HypMember()
        : internal(nullptr)
    {
    }

    HypMember(HypProperty&& property)
        : internal(new HypProperty(std::move(property)))
    {
    }

    HypMember(HypMethod&& method)
        : internal(new HypMethod(std::move(method)))
    {
    }

    HypMember(Field&& field)
        : internal(new Field(std::move(field)))
    {
    }

    HypMember(HypConstant&& field)
        : internal(new HypConstant(std::move(field)))
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

} // namespace hyperion
