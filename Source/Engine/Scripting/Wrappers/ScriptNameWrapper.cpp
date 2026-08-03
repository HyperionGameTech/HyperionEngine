/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#ifdef HYP_SCRIPT

#include <Lang/VM/Value.hpp>
#include <Core/Name/Name.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

namespace Hyperion {

ENGINE_API const Class* g_clsName = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Name, -1, 0, {})
    Method(NAME("ToString"), +[](const Name& name) -> String
        {
            return name.ToString();
        }),
    Method(NAME("FromString"), +[](const String& str) -> Name
        {
            return CreateNameFromDynamicString(str);
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(Name);

} // namespace Hyperion

#endif
