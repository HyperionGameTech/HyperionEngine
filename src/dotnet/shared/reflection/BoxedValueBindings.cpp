/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/Struct.hpp>

#include <core/Name.hpp>

#include <core/reflection/GenericArrayWrapper.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>

#include <core/serialization/fbom/FBOM.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void HypData_Construct(ValueStorage<BoxedValue>* pBoxed)
    {
        Assert(pBoxed != nullptr);

        pBoxed->Construct();
    }

    HYP_EXPORT void HypData_Destruct(ValueStorage<BoxedValue>* pBoxed)
    {
        Assert(pBoxed != nullptr);

        pBoxed->Destruct();
    }

    HYP_EXPORT void HypData_GetTypeId(const BoxedValue* pBoxed, TypeId* outTypeId)
    {
        if (!pBoxed || !outTypeId)
        {
            return;
        }

        *outTypeId = pBoxed->GetTypeId();
    }

    HYP_EXPORT const TypeInfo* HypData_GetTypeInfo(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return nullptr;
        }

        return pBoxed->GetTypeInfo();
    }

    HYP_EXPORT const void* HypData_GetPointer(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return nullptr;
        }

        return pBoxed->ToRef().GetPointer();
    }

    HYP_EXPORT int8 HypData_IsNull(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return true;
        }

        return pBoxed->IsNull();
    }

    HYP_EXPORT void HypData_Reset(BoxedValue* pBoxed)
    {
        if (pBoxed)
        {
            pBoxed->Reset();
        }
    }

#define HYP_DEFINE_HYPDATA_GET(type, name)                                                    \
    HYP_EXPORT int8 HypData_Get##name(const BoxedValue* pBoxed, int8 strict, type* pOutValue) \
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

#define HYP_DEFINE_HYPDATA_IS(type, name)                                   \
    HYP_EXPORT int8 HypData_Is##name(const BoxedValue* pBoxed, int8 strict) \
    {                                                                       \
        if (!pBoxed)                                                        \
        {                                                                   \
            return false;                                                   \
        }                                                                   \
                                                                            \
        return pBoxed->Is<type>(bool(strict));                              \
    }

#define HYP_DEFINE_HYPDATA_SET(type, name)                            \
    HYP_EXPORT int8 HypData_Set##name(BoxedValue* pBoxed, type value) \
    {                                                                 \
        if (!pBoxed)                                                  \
        {                                                             \
            return false;                                             \
        }                                                             \
                                                                      \
        *pBoxed = BoxedValue(value);                                  \
        return true;                                                  \
    }

    HYP_DEFINE_HYPDATA_GET(int8, Int8)
    HYP_DEFINE_HYPDATA_GET(int16, Int16)
    HYP_DEFINE_HYPDATA_GET(int32, Int32)
    HYP_DEFINE_HYPDATA_GET(int64, Int64)
    HYP_DEFINE_HYPDATA_GET(uint8, UInt8)
    HYP_DEFINE_HYPDATA_GET(uint16, UInt16)
    HYP_DEFINE_HYPDATA_GET(uint32, UInt32)
    HYP_DEFINE_HYPDATA_GET(uint64, UInt64)
    HYP_DEFINE_HYPDATA_GET(float, Float)
    HYP_DEFINE_HYPDATA_GET(double, Double)
    HYP_DEFINE_HYPDATA_GET(bool, Bool)
    HYP_DEFINE_HYPDATA_GET(void*, IntPtr)

    HYP_DEFINE_HYPDATA_SET(int8, Int8)
    HYP_DEFINE_HYPDATA_SET(int16, Int16)
    HYP_DEFINE_HYPDATA_SET(int32, Int32)
    HYP_DEFINE_HYPDATA_SET(int64, Int64)
    HYP_DEFINE_HYPDATA_SET(uint8, UInt8)
    HYP_DEFINE_HYPDATA_SET(uint16, UInt16)
    HYP_DEFINE_HYPDATA_SET(uint32, UInt32)
    HYP_DEFINE_HYPDATA_SET(uint64, UInt64)
    HYP_DEFINE_HYPDATA_SET(float, Float)
    HYP_DEFINE_HYPDATA_SET(double, Double)
    HYP_DEFINE_HYPDATA_SET(bool, Bool)
    HYP_DEFINE_HYPDATA_SET(void*, IntPtr)

    HYP_DEFINE_HYPDATA_IS(int8, Int8)
    HYP_DEFINE_HYPDATA_IS(int16, Int16)
    HYP_DEFINE_HYPDATA_IS(int32, Int32)
    HYP_DEFINE_HYPDATA_IS(int64, Int64)
    HYP_DEFINE_HYPDATA_IS(uint8, UInt8)
    HYP_DEFINE_HYPDATA_IS(uint16, UInt16)
    HYP_DEFINE_HYPDATA_IS(uint32, UInt32)
    HYP_DEFINE_HYPDATA_IS(uint64, UInt64)
    HYP_DEFINE_HYPDATA_IS(float, Float)
    HYP_DEFINE_HYPDATA_IS(double, Double)
    HYP_DEFINE_HYPDATA_IS(bool, Bool)
    HYP_DEFINE_HYPDATA_IS(void*, IntPtr)

#undef HYP_DEFINE_HYPDATA_GET
#undef HYP_DEFINE_HYPDATA_IS
#undef HYP_DEFINE_HYPDATA_SET

    HYP_EXPORT int8 HypData_IsArray(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->IsArray();
    }

    HYP_EXPORT int8 HypData_GetArraySize(const BoxedValue* pBoxed, int32* pOutSize)
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

    HYP_EXPORT int8 HypData_GetArrayElem(BoxedValue* pBoxed, int32 index, BoxedValue* pOutArrayElem)
    {
        if (!pBoxed || !pOutArrayElem)
        {
            return false;
        }

        if (pBoxed->IsArray())
        {
            GenericArrayWrapper& arrayWrapper = pBoxed->Get<GenericArrayWrapper>();

            AnyRef ref = arrayWrapper.GetElementAt(SizeType(index));

            if (!ref.HasValue())
            {
                return false;
            }

            // construct it
            new (pOutArrayElem) BoxedValue(ref);

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetArray(BoxedValue* pBoxed, const Class* pClass, BoxedValue* pElements, uint32 size)
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

    HYP_EXPORT int8 HypData_IsString(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->Is<String>();
    }

    HYP_EXPORT int8 HypData_GetString(const BoxedValue* pBoxed, const char** ppOutStringValue)
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

    HYP_EXPORT int8 HypData_SetString(BoxedValue* pBoxed, const char* pStringValue)
    {
        if (!pBoxed || !pStringValue)
        {
            return false;
        }

        *pBoxed = BoxedValue(String(pStringValue));

        return true;
    }

    HYP_EXPORT int8 HypData_IsId(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->Is<ObjIdBase>();
    }

    HYP_EXPORT int8 HypData_GetId(const BoxedValue* pBoxed, ObjIdBase* pOutId)
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

    HYP_EXPORT int8 HypData_SetId(BoxedValue* pBoxed, ObjIdBase* pId)
    {
        if (!pBoxed || !pId)
        {
            return false;
        }

        *pBoxed = BoxedValue(*pId);

        return true;
    }

    HYP_EXPORT int8 HypData_IsName(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->Is<Name>();
    }

    HYP_EXPORT int8 HypData_GetName(const BoxedValue* pBoxed, Name* pOutName)
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

    HYP_EXPORT int8 HypData_SetName(BoxedValue* pBoxed, Name nameValue)
    {
        if (!pBoxed)
        {
            return false;
        }

        *pBoxed = BoxedValue(nameValue);

        return true;
    }

    HYP_EXPORT int8 HypData_GetObject(const BoxedValue* pBoxed, dotnet::ObjectReference* pOutObjectReference)
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

        dotnet::ObjectReference objectReference;

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

    HYP_EXPORT int8 HypData_SetObject(BoxedValue* pBoxed, const Class* pClass, void* address)
    {
        if (!pBoxed || !pClass || !address)
        {
            return false;
        }

        const TypeId typeId = pClass->GetTypeId();

        if (pClass->IsClassType())
        {
            return pClass->ToHypData(ByteView(reinterpret_cast<ubyte*>(address), pClass->GetSize()), *pBoxed);
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetNullObject(BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        *pBoxed = BoxedValue(Handle<ObjectBase>::Null());

        return true;
    }

    HYP_EXPORT int8 HypData_GetStruct(const BoxedValue* pBoxed, dotnet::ObjectReference* pOutObjectReference)
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

    HYP_EXPORT int8 HypData_SetStruct(BoxedValue* pBoxed, const Class* pClass, uint32 size, void* pStructData)
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

        return pStruct->ToHypData(ByteView(reinterpret_cast<ubyte*>(pStructData), size), *pBoxed);
    }

    HYP_EXPORT int8 HypData_IsByteBuffer(const BoxedValue* pBoxed)
    {
        if (!pBoxed)
        {
            return false;
        }

        return pBoxed->Is<ByteBuffer>();
    }

    HYP_EXPORT int8 HypData_GetByteBuffer(const BoxedValue* pBoxed, const void** outPtr, uint32* outSize)
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

    HYP_EXPORT int8 HypData_SetByteBuffer(BoxedValue* pBoxed, const void* ptr, uint32 size)
    {
        if (!pBoxed || !ptr)
        {
            return false;
        }

        *pBoxed = BoxedValue(ByteBuffer(size, ptr));

        return true;
    }

} // extern "C"
