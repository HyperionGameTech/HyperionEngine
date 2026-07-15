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

#if defined(HYP_SCRIPT) && defined(HYP_DEBUG_MODE)
BoxedValue::~BoxedValue()
{
    AssertDebug(extData.gcIndex == INVALID_GC_INDEX,
                "BoxedValue being destroyed while still registered with the GC (index = {})",
                uint32(extData.gcIndex));

    extData.gcIndex = GARBAGE_GC_INDEX;
}
#endif // defined(HYP_SCRIPT) && defined(HYP_DEBUG_MODE)


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
