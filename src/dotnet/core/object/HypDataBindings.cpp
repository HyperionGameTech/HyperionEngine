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

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void HypData_Construct(ValueStorage<HypData>* hyp_data_storage)
    {
        AssertThrow(hyp_data_storage != nullptr);

        hyp_data_storage->Construct();
    }

    HYP_EXPORT void HypData_Destruct(ValueStorage<HypData>* hyp_data_storage)
    {
        AssertThrow(hyp_data_storage != nullptr);

        hyp_data_storage->Destruct();
    }

    HYP_EXPORT void HypData_GetTypeId(const HypData* hyp_data, TypeId* out_type_id)
    {
        if (!hyp_data || !out_type_id)
        {
            return;
        }

        *out_type_id = hyp_data->GetTypeId();
    }

    HYP_EXPORT const void* HypData_GetPointer(const HypData* hyp_data)
    {
        if (!hyp_data)
        {
            return nullptr;
        }

        return hyp_data->ToRef().GetPointer();
    }

    HYP_EXPORT int8 HypData_IsNull(const HypData* hyp_data)
    {
        if (!hyp_data)
        {
            return true;
        }

        return hyp_data->IsNull();
    }

    HYP_EXPORT void HypData_Reset(HypData* hyp_data)
    {
        if (hyp_data)
        {
            hyp_data->Reset();
        }
    }

#define HYP_DEFINE_HYPDATA_GET(type, name)                                                   \
    HYP_EXPORT int8 HypData_Get##name(const HypData* hyp_data, int8 strict, type* out_value) \
    {                                                                                        \
        if (!hyp_data || !out_value)                                                         \
        {                                                                                    \
            return false;                                                                    \
        }                                                                                    \
                                                                                             \
        if (hyp_data->Is<type>(bool(strict)))                                                \
        {                                                                                    \
            *out_value = hyp_data->Get<type>();                                              \
                                                                                             \
            return true;                                                                     \
        }                                                                                    \
                                                                                             \
        return false;                                                                        \
    }

#define HYP_DEFINE_HYPDATA_IS(type, name)                                  \
    HYP_EXPORT int8 HypData_Is##name(const HypData* hyp_data, int8 strict) \
    {                                                                      \
        if (!hyp_data)                                                     \
        {                                                                  \
            return false;                                                  \
        }                                                                  \
                                                                           \
        return hyp_data->Is<type>(bool(strict));                           \
    }

#define HYP_DEFINE_HYPDATA_SET(type, name)                           \
    HYP_EXPORT int8 HypData_Set##name(HypData* hyp_data, type value) \
    {                                                                \
        if (!hyp_data)                                               \
        {                                                            \
            return false;                                            \
        }                                                            \
                                                                     \
        *hyp_data = HypData(value);                                  \
        return true;                                                 \
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

    HYP_EXPORT int8 HypData_IsArray(const HypData* hyp_data)
    {
        // if (!hyp_data)
        // {
        //     return false;
        // }

        // return hyp_data->Is<Array<HypData>>();

        HYP_NOT_IMPLEMENTED();
    }

    HYP_EXPORT int8 HypData_GetArray(HypData* hyp_data, HypData** out_array, uint32* out_size)
    {
        if (!hyp_data || !out_array || !out_size)
        {
            return false;
        }

        if (hyp_data->Is<Array<HypData>>())
        {
            Array<HypData>& array = hyp_data->Get<Array<HypData>>();

            *out_array = array.Data();
            *out_size = uint32(array.Size());

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetArray(HypData* hyp_data, const HypClass* hyp_class, HypData* elements, uint32 size)
    {
        if (!hyp_data || !hyp_class || !elements)
        {
            return false;
        }

        if (!hyp_class->CanCreateInstance())
        {
            return false;
        }

        return hyp_class->CreateInstanceArray(Span<HypData>(elements, elements + size), *hyp_data, /* allow_abstract */ false);
    }

    HYP_EXPORT int8 HypData_IsString(const HypData* hyp_data)
    {
        if (!hyp_data)
        {
            return false;
        }

        return hyp_data->Is<String>();
    }

    HYP_EXPORT int8 HypData_GetString(const HypData* hyp_data, const char** out_str)
    {
        if (!hyp_data || !out_str)
        {
            return false;
        }

        if (hyp_data->Is<String>())
        {
            const String& str = hyp_data->Get<String>();

            *out_str = str.Data();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetString(HypData* hyp_data, const char* str)
    {
        if (!hyp_data || !str)
        {
            return false;
        }

        *hyp_data = HypData(String(str));

        return true;
    }

    HYP_EXPORT int8 HypData_IsId(const HypData* hyp_data)
    {
        if (!hyp_data)
        {
            return false;
        }

        return hyp_data->Is<ObjIdBase>();
    }

    HYP_EXPORT int8 HypData_GetId(const HypData* hyp_data, ObjIdBase* out_id)
    {
        if (!hyp_data || !out_id)
        {
            return false;
        }

        if (hyp_data->Is<ObjIdBase>())
        {
            *out_id = hyp_data->Get<ObjIdBase>();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetId(HypData* hyp_data, ObjIdBase* id)
    {
        if (!hyp_data || !id)
        {
            return false;
        }

        *hyp_data = HypData(*id);

        return true;
    }

    HYP_EXPORT int8 HypData_IsName(const HypData* hyp_data)
    {
        if (!hyp_data)
        {
            return false;
        }

        return hyp_data->Is<Name>();
    }

    HYP_EXPORT int8 HypData_GetName(const HypData* hyp_data, Name* out_name)
    {
        if (!hyp_data || !out_name)
        {
            return false;
        }

        if (hyp_data->Is<Name>())
        {
            *out_name = hyp_data->Get<Name>();

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetName(HypData* hyp_data, Name name_value)
    {
        if (!hyp_data)
        {
            return false;
        }

        *hyp_data = HypData(name_value);

        return true;
    }

    HYP_EXPORT int8 HypData_GetHypObject(const HypData* hyp_data, dotnet::ObjectReference* out_object_reference)
    {
        if (!hyp_data || !out_object_reference)
        {
            return false;
        }

        if (hyp_data->IsNull())
        {
            HYP_LOG(Object, Error, "Cannot get HypObject from null HypData");

            return false;
        }

        const HypClass* hyp_class = GetClass(hyp_data->GetTypeId());

        if (!hyp_class)
        {
            return false;
        }

        if (!hyp_class->IsClassType())
        {
            return false;
        }

        if (!hyp_data->ToRef().HasValue())
        {
            // Null HypData refs still return true - null handling happens on managed side
            return true;
        }

        dotnet::ObjectReference object_reference;

        if (hyp_class->GetManagedObject(hyp_data->ToRef().GetPointer(), object_reference))
        {
            *out_object_reference = object_reference;

            return true;
        }

        HYP_LOG(Object, Error, "Failed to get managed object for instance of HypClass {}", hyp_class->GetName());

        return false;
    }

    HYP_EXPORT int8 HypData_SetHypObject(HypData* hyp_data, const HypClass* hyp_class, void* address)
    {
        if (!hyp_data || !hyp_class || !address)
        {
            return false;
        }

        const TypeId type_id = hyp_class->GetTypeId();

        if (hyp_class->IsClassType())
        {
            return hyp_class->ToHypData(ByteView(reinterpret_cast<ubyte*>(address), hyp_class->GetSize()), *hyp_data);
        }

        return false;
    }

    HYP_EXPORT int8 HypData_GetHypStruct(const HypData* hyp_data, dotnet::ObjectReference* out_object_reference)
    {
        if (!hyp_data || !out_object_reference)
        {
            return false;
        }

        ConstAnyRef ref = hyp_data->ToRef();

        if (!ref.HasValue())
        {
            return false;
        }

        // @TODO Implement for dynamic struct types

        const HypClass* hyp_class = GetClass(hyp_data->GetTypeId());

        if (!hyp_class)
        {
            return false;
        }

        if (!hyp_class->IsStructType())
        {
            return false;
        }

        if (RC<dotnet::Class> managed_class = hyp_class->GetManagedClass())
        {
            AssertThrow(managed_class->GetMarshalObjectFunction() != nullptr);

            *out_object_reference = managed_class->GetMarshalObjectFunction()(ref.GetPointer(), uint32(hyp_class->GetSize()));

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetHypStruct(HypData* hyp_data, const HypClass* hyp_class, uint32 size, void* object_ptr)
    {
        if (!hyp_data || !hyp_class || !object_ptr)
        {
            return false;
        }

        if (!hyp_class->IsStructType())
        {
            HYP_LOG(Object, Error, "HypClass {} is not a struct type", hyp_class->GetName());

            return false;
        }

        if (size != hyp_class->GetSize())
        {
            HYP_LOG(Object, Error, "Given a buffer size of {} but HypClass {} has a size of {}",
                size, hyp_class->GetName(), hyp_class->GetSize());

            return false;
        }

        const HypStruct* hyp_struct = dynamic_cast<const HypStruct*>(hyp_class);
        AssertThrow(hyp_struct != nullptr);

        return hyp_struct->ToHypData(ByteView(reinterpret_cast<ubyte*>(object_ptr), size), *hyp_data);
    }

    HYP_EXPORT int8 HypData_IsByteBuffer(const HypData* hyp_data)
    {
        if (!hyp_data)
        {
            return false;
        }

        return hyp_data->Is<ByteBuffer>();
    }

    HYP_EXPORT int8 HypData_GetByteBuffer(const HypData* hyp_data, const void** out_ptr, uint32* out_size)
    {
        if (!hyp_data || !out_ptr || !out_size)
        {
            return false;
        }

        if (hyp_data->Is<ByteBuffer>())
        {
            const ByteBuffer& byte_buffer = hyp_data->Get<ByteBuffer>();

            *out_ptr = byte_buffer.Data();
            *out_size = uint32(byte_buffer.Size());

            return true;
        }

        return false;
    }

    HYP_EXPORT int8 HypData_SetByteBuffer(HypData* hyp_data, const void* ptr, uint32 size)
    {
        if (!hyp_data || !ptr)
        {
            return false;
        }

        *hyp_data = HypData(ByteBuffer(size, ptr));

        return true;
    }

} // extern "C"