/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/ObjectPool.hpp>
#include <Core/Reflection/Class.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion {

CORE_API extern const Class* g_clsObjectBase;

CORE_API const TypeInfo& Class_GetTypeInfo(const Class& cls)
{
    return *cls.GetTypeInfo();
}

#pragma region BoxedValue

HYP_NODISCARD AnyRef BoxedValue::ToRef()
{
    if (!IsValid())
    {
        return AnyRef();
    }

    if (value.Is<ObjectBase*>())
    {
        ObjectBase* object = value.GetUnchecked<ObjectBase*>();
        
        // Null object
        if (!object)
        {
            return AnyRef(&Class_GetTypeInfo(*g_clsObjectBase), nullptr);
        }
        
        return AnyRef(&Class_GetTypeInfo(*object->InstanceClass()), object);
    }

    if (Handle<ObjectBase>* pHandle = value.TryGet<Handle<ObjectBase>>())
    {
        return pHandle->ToRef();
    }

    if (SharedPtr<void>* pShared = value.TryGet<SharedPtr<void>>())
    {
        return pShared->ToRef();
    }

    if (Any* pAny = value.TryGet<Any>())
    {
        return pAny->ToRef();
    }

    if (AnyRef* pAnyRef = value.TryGet<AnyRef>())
    {
        return *pAnyRef;
    }

    return AnyRef(value.GetCurrentTypeInfo(), value.GetPointer());
}

#pragma endregion BoxedValue

} // namespace Hyperion
