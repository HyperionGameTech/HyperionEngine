#include <HyperionPch.hpp>

#ifdef HYP_SCRIPT

#include <Core/reflection/Field.hpp>
#include <Core/reflection/Class.hpp>
#include <Core/reflection/ClassUtils.hpp>

namespace Hyperion {

HYP_API const Class* g_clsField = nullptr;

// @TODO GetValue(), SetValue(), etc.

// clang-format off
HYP_BEGIN_STRUCT(Field, -1, 0, {})
    Method(NAME("GetName"), +[](const Field& field) -> Name
        {
            return field.GetName();
        }),
    Method(NAME("GetValue"), +[](const Field& field, const BoxedValue& target) -> BoxedValue
        {
            return field.Get(target);
        }),
    Method(NAME("SetValue"), +[](const Field& field, BoxedValue& target, const BoxedValue& value)
        {
            field.Set(target, value);
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(Field);

} // namespace Hyperion

#endif
