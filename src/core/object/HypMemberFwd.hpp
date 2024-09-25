/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_MEMBER_FWD_HPP
#define HYPERION_CORE_HYP_MEMBER_FWD_HPP

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/TypeID.hpp>

namespace hyperion {

class IHypMember
{
public:
    virtual ~IHypMember() = default;

    virtual Name GetName() const = 0;
    virtual TypeID GetTypeID() const = 0;
    virtual const String *GetAttribute(UTF8StringView key) const = 0;
};

} // namespace hyperion

#endif