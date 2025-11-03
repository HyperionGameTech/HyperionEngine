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
    Variant<HypProperty, HypMethod, Field, HypConstant> value;

    HypMember() = default;

    HypMember(HypProperty&& property)
        : value(std::move(property))
    {
    }

    HypMember(HypMethod&& method)
        : value(std::move(method))
    {
    }

    HypMember(Field&& field)
        : value(std::move(field))
    {
    }

    HypMember(HypConstant&& field)
        : value(std::move(field))
    {
    }

    HypMember(const HypMember& other) = delete;
    HypMember& operator=(const HypMember& other) = delete;

    HypMember(HypMember&& other) noexcept = default;
    HypMember& operator=(HypMember&& other) noexcept = default;

    ~HypMember() = default;
};

} // namespace hyperion
