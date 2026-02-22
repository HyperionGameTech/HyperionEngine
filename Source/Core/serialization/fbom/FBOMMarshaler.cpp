/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>

namespace Hyperion::serialization {

FBOMMarshalerRegistrationBase::FBOMMarshalerRegistrationBase(TypeId typeId, ANSIStringView name, UniquePtr<FBOMMarshalerBase>&& marshal)
{
    FBOM::GetInstance().RegisterLoader(typeId, name, std::move(marshal));
}

} // namespace Hyperion::serialization
