#include <system/Debug.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT void TypeID_FromString(const char *type_name, TypeID *out_type_id)
{
    *out_type_id = TypeID::FromString(type_name);
}
} // extern "C"