/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_MEMBER_HPP
#define HYPERION_CORE_HYP_MEMBER_HPP

#include <core/Defines.hpp>

#include <core/object/HypMethod.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

namespace hyperion {

struct HypMember
{
    Variant<HypProperty, HypMethod, HypField, HypConstant> value;

    HypMember(HypProperty&& property)
        : value(std::move(property))
    {
    }

    HypMember(HypMethod&& method)
        : value(std::move(method))
    {
    }

    HypMember(HypField&& field)
        : value(std::move(field))
    {
    }

    HypMember(HypConstant&& field)
        : value(std::move(field))
    {
    }

    HypMember(const HypMember& other) = delete;
    HypMember& operator=(const HypMember& other) = delete;

    HypMember(HypMember&& other) noexcept = delete;
    HypMember& operator=(HypMember&& other) noexcept = delete;

    ~HypMember() = default;
};

} // namespace hyperion

#endif