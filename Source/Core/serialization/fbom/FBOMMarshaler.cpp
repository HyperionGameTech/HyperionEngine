/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Core/serialization/fbom/FBOM.hpp>
#include <Core/serialization/fbom/FBOMMarshaler.hpp>

namespace Hyperion::serialization {

FBOMMarshalerRegistrationBase::FBOMMarshalerRegistrationBase(TypeId typeId, ANSIStringView name, UniquePtr<FBOMMarshalerBase>&& marshal)
{
    FBOM::GetInstance().RegisterLoader(typeId, name, std::move(marshal));
}

} // namespace Hyperion::serialization
