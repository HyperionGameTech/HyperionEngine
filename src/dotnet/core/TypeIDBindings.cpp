#include <system/Debug.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT uint32 TypeID_ForDynamicType(const char *type_name)
{
    HYP_THROW("This function has been removed");
}
} // extern "C"