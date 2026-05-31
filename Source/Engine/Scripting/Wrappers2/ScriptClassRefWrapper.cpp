#include <HyperionPch.hpp>

#ifdef HYP_SCRIPT

#include <Lang/vm/Value.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

ENGINE_API const Class* g_clsClassRef = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(ClassRef, -1, 0, {})
    Method(NAME("IsValid"), +[](const ClassRef& classRef) -> bool
        {
            return classRef.IsValid();
        }),
    Method(NAME("GetName"), +[](const ClassRef& classRef) -> Name
        {
            if (!classRef.IsValid())
            {
                return Name();
            }

            return classRef->GetName();
        }),
    Method(NAME("GetSize"), +[](const ClassRef& classRef) -> uint64
        {
            if (!classRef.IsValid())
            {
                return 0;
            }

            return uint64(classRef->GetSize());
        }),
    Method(NAME("GetAlignment"), +[](const ClassRef& classRef) -> uint64
        {
            if (!classRef.IsValid())
            {
                return 0;
            }

            return uint64(classRef->GetAlignment());
        }),
    Method(NAME("GetParent"), +[](const ClassRef& classRef) -> BoxedValue
        {
            if (!classRef.IsValid())
            {
                return BoxedValue();
            }

            const Class* parent = classRef->GetParent();
            if (!parent)
            {
                return BoxedValue();
            }

            return BoxedValue(ClassRef(parent));
        }),
    Method(NAME("IsClassType"), +[](const ClassRef& classRef) -> bool
        {
            return classRef.IsValid() && classRef->IsClassType();
        }),
    Method(NAME("IsStructType"), +[](const ClassRef& classRef) -> bool
        {
            return classRef.IsValid() && classRef->IsStructType();
        }),
    Method(NAME("IsEnumType"), +[](const ClassRef& classRef) -> bool
        {
            return classRef.IsValid() && classRef->IsEnumType();
        }),
    Method(NAME("IsPodType"), +[](const ClassRef& classRef) -> bool
        {
            return classRef.IsValid() && classRef->IsPodType();
        }),
    Method(NAME("IsAbstract"), +[](const ClassRef& classRef) -> bool
        {
            return classRef.IsValid() && classRef->IsAbstract();
        }),
    Method(NAME("IsDynamic"), +[](const ClassRef& classRef) -> bool
        {
            return classRef.IsValid() && classRef->IsDynamic();
        }),
    Method(NAME("IsDerivedFrom"), +[](const ClassRef& classRef, const ClassRef& other) -> bool
        {
            if (!classRef.IsValid() || !other.IsValid())
            {
                return false;
            }

            return classRef->IsDerivedFrom(other.cls);
        }),
    Method(NAME("IsBaseOf"), +[](const ClassRef& classRef, const ClassRef& other) -> bool
        {
            if (!classRef.IsValid() || !other.IsValid())
            {
                return false;
            }

            return classRef->IsBaseOf(other.cls);
        }),
    Method(NAME("CanCreateInstance"), +[](const ClassRef& classRef) -> bool
        {
            return classRef.IsValid() && classRef->CanCreateInstance();
        }),
    Method(NAME("CreateInstance"), +[](const ClassRef& classRef) -> BoxedValue
        {
            if (!classRef.IsValid() || !classRef->CanCreateInstance())
            {
                return BoxedValue();
            }

            BoxedValue result;
            classRef->CreateInstance(result);
            return result;
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(ClassRef);

} // namespace Hyperion

#endif
