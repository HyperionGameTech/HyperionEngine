#include <core/lib/TypeID.hpp>

namespace hyperion {

// TypeIDGeneratorBase

TypeIDNameMap TypeIDGeneratorBase::name_map = { };

TypeIDNameMap &TypeIDGeneratorBase::GetNameMap()
{
    return name_map;
}

// TypeID
const TypeID TypeID::void_type_id = { };

TypeID TypeID::Void()
{
    return void_type_id;
}

TypeID TypeID::ForName(struct Name name)
{
    HYP_THROW("This function has been removed");
}

} // namespace hyperion