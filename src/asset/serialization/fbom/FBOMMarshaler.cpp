/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <Engine.hpp>

namespace hyperion {
class Engine;
} // namespace hyperion

namespace hyperion::fbom {

namespace detail {

FBOMMarshalerRegistrationBase::FBOMMarshalerRegistrationBase(TypeID type_id, UniquePtr<FBOMMarshalerBase> &&marshal)
{
    FBOM::GetInstance().RegisterLoader(type_id, std::move(marshal));
}

} // namespace detail

} // namespace hyperion::fbom