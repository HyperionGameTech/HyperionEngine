#include <core/lib/TypeID.hpp>

namespace hyperion {

#pragma region TypeID

const TypeID TypeID::void_type_id = TypeID { void_value };

TypeID TypeID::ForName(struct Name name)
{
    HYP_THROW("This function has been removed");
}

#pragma endregion TypeID

} // namespace hyperion