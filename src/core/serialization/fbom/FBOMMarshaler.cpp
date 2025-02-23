/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>

namespace hyperion::fbom {

namespace detail {

FBOMMarshalerRegistrationBase::FBOMMarshalerRegistrationBase(TypeID type_id, ANSIStringView name, UniquePtr<FBOMMarshalerBase> &&marshal)
{
    FBOM::GetInstance().RegisterLoader(type_id, name, std::move(marshal));
}

} // namespace detail

} // namespace hyperion::fbom