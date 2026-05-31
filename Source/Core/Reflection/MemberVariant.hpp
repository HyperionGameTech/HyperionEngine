/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/Method.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/StaticField.hpp>
#include <Core/Reflection/Member.hpp>

#include <Core/Reflection/TypeId.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Variant.hpp>

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
