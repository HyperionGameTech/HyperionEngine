#include <core/lib/TypeID.hpp>

namespace hyperion {

TypeIDNameMap TypeIDGeneratorBase::name_map = { };

const TypeID TypeID::void_type_id = { };

TypeID TypeID::ForName(struct Name name)
{
    const TypeIDValue id = TypeIDGeneratorBase::name_map.ReverseLookup(name);

    return TypeID(id);
}

} // namespace hyperion