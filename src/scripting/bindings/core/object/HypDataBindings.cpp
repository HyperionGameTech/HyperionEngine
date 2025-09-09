/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>

#include <core/Name.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

#include <core/serialization/fbom/FBOM.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void HypData_Construct(ValueStorage<HypData>* hypDataStorage)
    {
        Assert(hypDataStorage != nullptr);

        hypDataStorage->Construct();
    }

    HYP_EXPORT void HypData_Destruct(ValueStorage<HypData>* hypDataStorage)
    {
        Assert(hypDataStorage != nullptr);

        hypDataStorage->Destruct();
    }

    HYP_EXPORT void HypData_GetTypeId(const HypData* hypData, TypeId* outTypeId)
    {
        if (!hypData || !outTypeId)
        {
            return;
        }

        *outTypeId = hypData->GetTypeId();
    }

    HYP_EXPORT const void* HypData_GetPointer(const HypData* hypData)
    {
        if (!hypData)
        {
            return nullptr;
        }

        return hypData->ToRef().GetPointer();
    }

    HYP_EXPORT int8 HypData_IsNull(const HypData* hypData)
    {
        if (!hypData)
        {
            return true;
        }

        return hypData->IsNull();
    }

    HYP_EXPORT void HypData_Reset(HypData* hypData)
    {
        if (hypData)
        {
            hypData->Reset();
        }
    }

#define HYP_DEFINE_HYPDATA_GET(type, name)                                                 \
    HYP_EXPORT int8 HypData_Get##name(const HypData* hypData, int8 strict, type* outValue) \
    {                                                                                      \
        if (!hypData || !outValue)                                                         \
        {                                                                                  \
            return false;                                                                  \
        }                                                                                  \
                                                                                           \
        if (hypData->Is<type>(bool(strict)))                                               \
        {                                                                                  \
            *outValue = hypData->Get<type>();                                              \
                                                                                           \
            return true;                                                                   \
        }                                                                                  \
                                                                                           \
        return false;                                                                      \
    }

#define HYP_DEFINE_HYPDATA_IS(type, name)                                 \
    HYP_EXPORT int8 HypData_Is##name(const HypData* hypData, int8 strict) \
    {                                                                     \
        if (!hypData)                                                     \
        {                                                                 \
            return false;                                                 \
        }                                                                 \
                                                                          \
        return hypData->Is<type>(bool(strict));                           \
    }

#define HYP_DEFINE_HYPDATA_SET(type, name)                          \
    HYP_EXPORT int8 HypData_Set##name(HypData* hypData, type value) \
    {                                                               \
        if (!hypData)                                               \
        {                                                           \
            return false;                                           \
        }                                                           \
                                                                    \
        *hypData = HypData(value);                                  \
        return true;                                                \
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

    HYP_EXPORT int8 HypData_IsArray(const HypData* hypData)
    {
        // if (!hypData)
        // {
        //     return false;
        // }

        // return hypData->Is<Array<HypData>>();

        HYP_NOT_IMPLEMENTED();
    }

    HYP_EXPORT int8 HypData_GetArray(HypData* hypData, HypData** outArray, uint32* outSize)
    {
        if (!hypData || !outArray || !outSize)
        {
            return false;
        }

        if (hypData->Is<Array<HypData>>())
        {
            Array<HypData>& array = hypData->Get<Array<HypData>>();

            *outArray = array.Data();
            *outSize = uint32(array.Size());

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetArray(HypData* hypData, const HypClass* hypClass, HypData* elements, uint32 size)
    {
        if (!hypData || !hypClass || !elements)
        {
            return false;
        }

        if (!hypClass->CanCreateInstance())
        {
            return false;
        }

        return hypClass->CreateInstanceArray(Span<HypData>(elements, elements + size), *hypData, /* allowAbstract */ false);
    }

    HYP_EXPORT int8 HypData_IsString(const HypData* hypData)
    {
        if (!hypData)
        {
            return false;
        }

        return hypData->Is<String>();
    }

    HYP_EXPORT int8 HypData_GetString(const HypData* hypData, const char** outStr)
    {
        if (!hypData || !outStr)
        {
            return false;
        }

        if (hypData->Is<String>())
        {
            const String& str = hypData->Get<String>();

            *outStr = str.Data();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetString(HypData* hypData, const char* str)
    {
        if (!hypData || !str)
        {
            return false;
        }

        *hypData = HypData(String(str));

        return true;
    }

    HYP_EXPORT int8 HypData_IsId(const HypData* hypData)
    {
        if (!hypData)
        {
            return false;
        }

        return hypData->Is<ObjIdBase>();
    }

    HYP_EXPORT int8 HypData_GetId(const HypData* hypData, ObjIdBase* outId)
    {
        if (!hypData || !outId)
        {
            return false;
        }

        if (hypData->Is<ObjIdBase>())
        {
            *outId = hypData->Get<ObjIdBase>();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetId(HypData* hypData, ObjIdBase* id)
    {
        if (!hypData || !id)
        {
            return false;
        }

        *hypData = HypData(*id);

        return true;
    }

    HYP_EXPORT int8 HypData_IsName(const HypData* hypData)
    {
        if (!hypData)
        {
            return false;
        }

        return hypData->Is<Name>();
    }

    HYP_EXPORT int8 HypData_GetName(const HypData* hypData, Name* outName)
    {
        if (!hypData || !outName)
        {
            return false;
        }

        if (hypData->Is<Name>())
        {
            *outName = hypData->Get<Name>();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetName(HypData* hypData, Name nameValue)
    {
        if (!hypData)
        {
            return false;
        }

        *hypData = HypData(nameValue);

        return true;
    }

    HYP_EXPORT int8 HypData_GetHypObject(const HypData* hypData, dotnet::ObjectReference* outObjectReference)
    {
#ifdef HYP_DOTNET
        if (!hypData || !outObjectReference)
        {
            return false;
        }

        if (hypData->IsNull())
        {
            HYP_LOG(Object, Error, "Cannot get HypObject from null HypData");

            return false;
        }

        const HypClass* hypClass = GetClass(hypData->GetTypeId());

        if (!hypClass)
        {
            return false;
        }

        if (!hypClass->IsClassType())
        {
            return false;
        }

        if (!hypData->ToRef().HasValue())
        {
            // Null HypData refs still return true - null handling happens on managed side
            return true;
        }

        dotnet::ObjectReference objectReference;

        if (hypClass->GetManagedObject(hypData->ToRef().GetPointer(), objectReference))
        {
            *outObjectReference = objectReference;

            return true;
        }

        HYP_LOG(Object, Error, "Failed to get managed object for instance of HypClass {}", hypClass->GetName());

        return false;
#else
        return false;
#endif
    }

    HYP_EXPORT int8 HypData_SetHypObject(HypData* hypData, const HypClass* hypClass, void* address)
    {
        if (!hypData || !hypClass || !address)
        {
            return false;
        }

        const TypeId typeId = hypClass->GetTypeId();

        if (hypClass->IsClassType())
        {
            return hypClass->ToHypData(ByteView(reinterpret_cast<ubyte*>(address), hypClass->GetSize()), *hypData);
        }

        return false;
    }

    HYP_EXPORT int8 HypData_GetHypStruct(const HypData* hypData, dotnet::ObjectReference* outObjectReference)
    {
#ifdef HYP_DOTNET
        if (!hypData || !outObjectReference)
        {
            return false;
        }

        ConstAnyRef ref = hypData->ToRef();

        if (!ref.HasValue())
        {
            return false;
        }

        // @TODO Implement for dynamic struct types

        const HypClass* hypClass = GetClass(hypData->GetTypeId());

        if (!hypClass)
        {
            return false;
        }

        if (!hypClass->IsStructType())
        {
            return false;
        }

        if (RC<dotnet::Class> managedClass = hypClass->GetManagedClass())
        {
            Assert(managedClass->GetMarshalObjectFunction() != nullptr);

            *outObjectReference = managedClass->GetMarshalObjectFunction()(ref.GetPointer(), uint32(hypClass->GetSize()));

            return true;
        }

        return false;
#else
        return false;
#endif
    }

    HYP_EXPORT int8 HypData_SetHypStruct(HypData* hypData, const HypClass* hypClass, uint32 size, void* objectPtr)
    {
        if (!hypData || !hypClass || !objectPtr)
        {
            return false;
        }

        if (!hypClass->IsStructType())
        {
            HYP_LOG(Object, Error, "HypClass {} is not a struct type", hypClass->GetName());

            return false;
        }

        const HypStruct* hypStruct = dynamic_cast<const HypStruct*>(hypClass);
        Assert(hypStruct != nullptr);
        
        if (size != hypStruct->GetSize())
        {
            HYP_LOG(Object, Error, "Given a buffer size of {} but HypClass {} has a size of {}",
                size, hypClass->GetName(), hypStruct->GetSize());

            return false;
        }

        return hypStruct->ToHypData(ByteView(reinterpret_cast<ubyte*>(objectPtr), size), *hypData);
    }

    HYP_EXPORT int8 HypData_IsByteBuffer(const HypData* hypData)
    {
        if (!hypData)
        {
            return false;
        }

        return hypData->Is<ByteBuffer>();
    }

    HYP_EXPORT int8 HypData_GetByteBuffer(const HypData* hypData, const void** outPtr, uint32* outSize)
    {
        if (!hypData || !outPtr || !outSize)
        {
            return false;
        }

        if (hypData->Is<ByteBuffer>())
        {
            const ByteBuffer& byteBuffer = hypData->Get<ByteBuffer>();

            *outPtr = byteBuffer.Data();
            *outSize = uint32(byteBuffer.Size());

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetByteBuffer(HypData* hypData, const void* ptr, uint32 size)
    {
        if (!hypData || !ptr)
        {
            return false;
        }

        *hypData = HypData(ByteBuffer(size, ptr));

        return true;
    }

} // extern "C"
