#include <core/lib/TypeID.hpp>

namespace hyperion {

HYP_API TypeIDNameMap TypeIDGeneratorBase::name_map = { };
HYP_API const TypeID TypeID::void_type_id = { };

TypeID TypeID::ForName(struct Name name)
{
    HYP_THROW("This function has been removed");
}

} // namespace hyperion