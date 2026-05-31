/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/Struct.hpp>

#include <Core/name/Name.hpp>

#include <Core/reflection/GenericArrayWrapper.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <DotNET/ManagedObject.hpp>
#include <DotNET/ManagedClass.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void BoxedValue_Construct(ValueStorage<BoxedValue>* pBoxed)
    {
        Assert(pBoxed != nullptr);

        pBoxed->Construct();
    }

    HYP_EXPORT void BoxedValue_Destruct(ValueStorage<BoxedValue>* pBoxed)
    {
        Assert(pBoxed != nullptr);

        pBoxed->Destruct();
    }

    HYP_EXPORT void BoxedValue_GetTypeId(const BoxedValue* pBoxed, TypeId* outTypeId)
    {
        if (!pBoxed || !outTypeId)
        {
            return;
        }

        *outTypeId = pBoxed->GetTypeId();
    }

    HYP_EXPORT const TypeInfo* BoxedValue_GetTypeInfo(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return nullptr;
        }

        return pBoxed->GetTypeInfo();
    }

    HYP_EXPORT const void* BoxedValue_GetPointer(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return nullptr;
        }

        return pBoxed->ToRef().GetPointer();
    }

    HYP_EXPORT int8 BoxedValue_IsNull(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return true;
        }

        return pBoxed->IsNull();
    }

    HYP_EXPORT void BoxedValue_Reset(BoxedValue* pBoxed)
    {
        if (pBoxed)
        {
            pBoxed->Reset();
        }
    }

#define HYP_DEFINE_BOXED_VALUE_GET(type, name)                                                    \
    HYP_EXPORT int8 BoxedValue_Get##name(const BoxedValue* pBoxed, int8 strict, type* pOutValue) \
    {                                                                                         \
        if (!pBoxed || !pOutValue)                                                            \
        {                                                                                     \
            return false;                                                                     \
        }                                                                                     \
                                                                                              \
        if (pBoxed->Is<type>(bool(strict)))                                                   \
        {                                                                                     \
            *pOutValue = pBoxed->Get<type>();                                                 \
                                                                                              \
            return true;                                                                      \
        }                                                                                     \
                                                                                              \
        return false;                                                                         \
    }

#define HYP_DEFINE_BOXED_VALUE_IS(type, name)                                   \
    HYP_EXPORT int8 BoxedValue_Is##name(const BoxedValue* pBoxed, int8 strict) \
    {                                                                       \
        if (!pBoxed)                                                        \
        {                                                                   \
            return false;                                                   \
        }                                                                   \
                                                                            \
        return pBoxed->Is<type>(bool(strict));                              \
    }

#define HYP_DEFINE_BOXED_VALUE_SET(type, name)                            \
    HYP_EXPORT int8 BoxedValue_Set##name(BoxedValue* pBoxed, type value) \
    {                                                                 \
        if (!pBoxed)                                                  \
        {                                                             \
            return false;                                             \
        }                                                             \
                                                                      \
        *pBoxed = BoxedValue(value);                                  \
        return true;                                                  \
    }

    HYP_DEFINE_BOXED_VALUE_GET(int8, Int8)
    HYP_DEFINE_BOXED_VALUE_GET(int16, Int16)
    HYP_DEFINE_BOXED_VALUE_GET(int32, Int32)
    HYP_DEFINE_BOXED_VALUE_GET(int64, Int64)
    HYP_DEFINE_BOXED_VALUE_GET(uint8, UInt8)
    HYP_DEFINE_BOXED_VALUE_GET(uint16, UInt16)
    HYP_DEFINE_BOXED_VALUE_GET(uint32, UInt32)
    HYP_DEFINE_BOXED_VALUE_GET(uint64, UInt64)
    HYP_DEFINE_BOXED_VALUE_GET(float, Float)
    HYP_DEFINE_BOXED_VALUE_GET(double, Double)
    HYP_DEFINE_BOXED_VALUE_GET(bool, Bool)
    HYP_DEFINE_BOXED_VALUE_GET(void*, IntPtr)

    HYP_DEFINE_BOXED_VALUE_SET(int8, Int8)
    HYP_DEFINE_BOXED_VALUE_SET(int16, Int16)
    HYP_DEFINE_BOXED_VALUE_SET(int32, Int32)
    HYP_DEFINE_BOXED_VALUE_SET(int64, Int64)
    HYP_DEFINE_BOXED_VALUE_SET(uint8, UInt8)
    HYP_DEFINE_BOXED_VALUE_SET(uint16, UInt16)
    HYP_DEFINE_BOXED_VALUE_SET(uint32, UInt32)
    HYP_DEFINE_BOXED_VALUE_SET(uint64, UInt64)
    HYP_DEFINE_BOXED_VALUE_SET(float, Float)
    HYP_DEFINE_BOXED_VALUE_SET(double, Double)
    HYP_DEFINE_BOXED_VALUE_SET(bool, Bool)
    HYP_DEFINE_BOXED_VALUE_SET(void*, IntPtr)

    HYP_DEFINE_BOXED_VALUE_IS(int8, Int8)
    HYP_DEFINE_BOXED_VALUE_IS(int16, Int16)
    HYP_DEFINE_BOXED_VALUE_IS(int32, Int32)
    HYP_DEFINE_BOXED_VALUE_IS(int64, Int64)
    HYP_DEFINE_BOXED_VALUE_IS(uint8, UInt8)
    HYP_DEFINE_BOXED_VALUE_IS(uint16, UInt16)
    HYP_DEFINE_BOXED_VALUE_IS(uint32, UInt32)
    HYP_DEFINE_BOXED_VALUE_IS(uint64, UInt64)
    HYP_DEFINE_BOXED_VALUE_IS(float, Float)
    HYP_DEFINE_BOXED_VALUE_IS(double, Double)
    HYP_DEFINE_BOXED_VALUE_IS(bool, Bool)
    HYP_DEFINE_BOXED_VALUE_IS(void*, IntPtr)

#undef HYP_DEFINE_BOXED_VALUE_GET
#undef HYP_DEFINE_BOXED_VALUE_IS
#undef HYP_DEFINE_BOXED_VALUE_SET

    HYP_EXPORT int8 BoxedValue_IsArray(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->IsArray();
    }

    HYP_EXPORT int8 BoxedValue_GetArraySize(const BoxedValue* pBoxed, int32* pOutSize)
    {
        if (!pBoxed || !pOutSize)
        {
            return false;
        }

        if (pBoxed->IsArray())
        {
            const GenericArrayWrapper& arrayWrapper = pBoxed->Get<GenericArrayWrapper>();

            *pOutSize = int32(arrayWrapper.Size());

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 BoxedValue_GetArrayElem(BoxedValue* pBoxed, int32 index, BoxedValue* pOutArrayElem)
    {
        if (!pBoxed || !pOutArrayElem)
        {
            return false;
        }

        if (pBoxed->IsArray())
        {
            GenericArrayWrapper& arrayWrapper = pBoxed->Get<GenericArrayWrapper>();

            if (!arrayWrapper.CanGetElementByIndex())
            {
                return false;
            }

            if (index >= arrayWrapper.Size())
            {
                return false;
            }

            BoxedValue tmp;
            if (!arrayWrapper.GetElementAt(size_t(index), tmp))
            {
                return false;
            }

            new (pOutArrayElem) BoxedValue(std::move(tmp));

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 BoxedValue_SetArray(BoxedValue* pBoxed, const Class* pClass, BoxedValue* pElements, uint32 size)
    {
        if (!pBoxed || !pClass || !pElements)
        {
            return false;
        }

        if (!pClass->CanCreateInstance())
        {
            return false;
        }

        return pClass->CreateInstanceArray(Span<BoxedValue>(pElements, pElements + size), *pBoxed, /* allowAbstract */ false);
    }

    HYP_EXPORT int8 BoxedValue_IsString(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->Is<String>();
    }

    HYP_EXPORT int8 BoxedValue_GetString(const BoxedValue* pBoxed, const char** ppOutStringValue)
    {
        if (!pBoxed || !ppOutStringValue)
        {
            return false;
        }

        if (pBoxed->Is<String>())
        {
            const String& str = pBoxed->Get<String>();

            *ppOutStringValue = str.Data();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 BoxedValue_SetString(BoxedValue* pBoxed, const char* pStringValue)
    {
        if (!pBoxed || !pStringValue)
        {
            return false;
        }

        *pBoxed = BoxedValue(String(pStringValue));

        return true;
    }

    HYP_EXPORT int8 BoxedValue_IsId(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->Is<ObjIdBase>();
    }

    HYP_EXPORT int8 BoxedValue_GetId(const BoxedValue* pBoxed, ObjIdBase* pOutId)
    {
        if (!pBoxed || !pOutId)
        {
            return false;
        }

        if (pBoxed->Is<ObjIdBase>())
        {
            *pOutId = pBoxed->Get<ObjIdBase>();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 BoxedValue_SetId(BoxedValue* pBoxed, ObjIdBase* pId)
    {
        if (!pBoxed || !pId)
        {
            return false;
        }

        *pBoxed = BoxedValue(*pId);

        return true;
    }

    HYP_EXPORT int8 BoxedValue_IsName(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->Is<Name>();
    }

    HYP_EXPORT int8 BoxedValue_GetName(const BoxedValue* pBoxed, Name* pOutName)
    {
        if (!pBoxed || !pOutName)
        {
            return false;
        }

        if (pBoxed->Is<Name>())
        {
            *pOutName = pBoxed->Get<Name>();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 BoxedValue_SetName(BoxedValue* pBoxed, Name nameValue)
    {
        if (!pBoxed)
        {
            return false;
        }

        *pBoxed = BoxedValue(nameValue);

        return true;
    }

    HYP_EXPORT int8 BoxedValue_GetObject(const BoxedValue* pBoxed, dotnet::ObjectReference* pOutObjectReference)
    {
#ifdef HYP_DOTNET
        if (!pBoxed || !pOutObjectReference)
        {
            return false;
        }

        if (pBoxed->IsNull())
        {
            HYP_LOG(Object, Error, "Cannot get Object from null BoxedValue");

            return false;
        }

        const Class* cls = GetClass(pBoxed->GetTypeId());

        if (!cls)
        {
            return false;
        }

        if (!cls->IsClassType())
        {
            return false;
        }

        if (!pBoxed->ToRef().HasValue())
        {
            // Null BoxedValue refs still return true - null handling happens on managed side
            return true;
        }

        dotnet::ObjectReference objectReference {};

        if (cls->GetManagedObject(pBoxed->ToRef().GetPointer(), objectReference))
        {
            *pOutObjectReference = objectReference;

            return true;
        }

        HYP_LOG(Object, Error, "Failed to get managed object for {} instance", cls->GetName());

        return false;
#else
        return false;
#endif
    }

    HYP_EXPORT int8 BoxedValue_SetObject(BoxedValue* pBoxed, const Class* pClass, void* address)
    {
        if (!pBoxed || !pClass || !address)
        {
            return false;
        }

        const TypeId typeId = pClass->GetTypeId();

        if (pClass->IsClassType())
        {
            return pClass->ToBoxed(ByteView(reinterpret_cast<ubyte*>(address), pClass->GetSize()), *pBoxed);
        }

        return false;
    }

    HYP_EXPORT int8 BoxedValue_SetNullObject(BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        *pBoxed = BoxedValue(Handle<ObjectBase>::Null());

        return true;
    }

    HYP_EXPORT int8 BoxedValue_GetStruct(const BoxedValue* pBoxed, dotnet::ObjectReference* pOutObjectReference)
    {
#ifdef HYP_DOTNET
        if (!pBoxed || !pOutObjectReference)
        {
            return false;
        }

        ConstAnyRef ref = pBoxed->ToRef();

        if (!ref.HasValue())
        {
            return false;
        }

        /// \todo Implement for dynamic struct types

        const Class* cls = GetClass(pBoxed->GetTypeId());

        if (!cls)
        {
            return false;
        }

        if (!cls->IsStructType())
        {
            return false;
        }

        if (RC<dotnet::ManagedClass> managedClass = cls->GetManagedClass())
        {
            Assert(managedClass->GetMarshalObjectFunction() != nullptr);

            *pOutObjectReference = managedClass->GetMarshalObjectFunction()(ref.GetPointer(), uint32(cls->GetSize()));

            return true;
        }

        return false;
#else
        return false;
#endif
    }

    HYP_EXPORT int8 BoxedValue_SetStruct(BoxedValue* pBoxed, const Class* pClass, uint32 size, void* pStructData)
    {
        if (!pBoxed || !pClass || !pStructData)
        {
            return false;
        }

        if (!pClass->IsStructType())
        {
            HYP_LOG(Object, Error, "Class {} is not a struct type", pClass->GetName());

            return false;
        }

        const Struct* pStruct = static_cast<const Struct*>(pClass);

        if (size != pStruct->GetSize())
        {
            HYP_LOG(Object, Error, "Given a buffer size of {} but Class {} has a size of {}",
                size, pClass->GetName(), pStruct->GetSize());

            return false;
        }

        return pStruct->ToBoxed(ByteView(reinterpret_cast<ubyte*>(pStructData), size), *pBoxed);
    }

    HYP_EXPORT int8 BoxedValue_IsByteBuffer(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->Is<ByteBuffer>();
    }

    HYP_EXPORT int8 BoxedValue_GetByteBuffer(const BoxedValue* pBoxed, const void** outPtr, uint32* outSize)
    {
        if (!pBoxed || !outPtr || !outSize)
        {
            return false;
        }

        if (pBoxed->Is<ByteBuffer>())
        {
            const ByteBuffer& byteBuffer = pBoxed->Get<ByteBuffer>();

            *outPtr = byteBuffer.Data();
            *outSize = uint32(byteBuffer.Size());

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 BoxedValue_SetByteBuffer(BoxedValue* pBoxed, const void* ptr, uint32 size)
    {
        if (!pBoxed || !ptr)
        {
            return false;
        }

        *pBoxed = BoxedValue(ByteBuffer(size, ptr));

        return true;
    }

    HYP_EXPORT const TypeInfo* BoxedValue_GetArrayElemTypeInfo(const BoxedValue* pBoxed)
    {
        if (!pBoxed || !pBoxed->IsArray())
        {
            return nullptr;
        }

        return pBoxed->Get<GenericArrayWrapper>().elementTypeInfo;
    }

    HYP_EXPORT int8 BoxedValue_SetArrayElem(BoxedValue* pBoxed, int32 index, BoxedValue* pElem)
    {
        if (!pBoxed || !pElem)
        {
            return false;
        }

        if (!pBoxed->IsArray())
        {
            return false;
        }

        GenericArrayWrapper& arrayWrapper = pBoxed->Get<GenericArrayWrapper>();

        return arrayWrapper.SetElementAt(size_t(index), std::move(*pElem)) ? 1 : 0;
    }

    HYP_EXPORT int8 BoxedValue_PushBackArrayElem(BoxedValue* pBoxed, BoxedValue* pElem)
    {
        if (!pBoxed || !pElem)
        {
            return false;
        }

        if (!pBoxed->IsArray())
        {
            return false;
        }

        GenericArrayWrapper& arrayWrapper = pBoxed->Get<GenericArrayWrapper>();

        if (!arrayWrapper.CanPushBack())
        {
            return false;
        }

        arrayWrapper.PushBack(std::move(*pElem));

        return true;
    }

    HYP_EXPORT int8 BoxedValue_ResizeArray(BoxedValue* pBoxed, int32 newSize)
    {
        if (!pBoxed || newSize < 0)
        {
            return false;
        }

        if (!pBoxed->IsArray())
        {
            return false;
        }

        GenericArrayWrapper& arrayWrapper = pBoxed->Get<GenericArrayWrapper>();

        if (!arrayWrapper.CanResize())
        {
            return false;
        }

        return arrayWrapper.Resize(size_t(newSize)) ? 1 : 0;
    }

} // extern "C"
