/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/Serialization.hpp>

#include <core/serialization/fbom/FBOMBaseTypes.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

using namespace hyperion;
using namespace serialization;

extern "C"
{

#pragma region FBOMType

#define FBOM_TYPE_CREATE_FUNCTION(fbomTypeName, functionSuffix) \
    HYP_EXPORT FBOMType* FBOMType_##functionSuffix()            \
    {                                                           \
        return new FBOM##fbomTypeName();                        \
    }

    FBOM_TYPE_CREATE_FUNCTION(Unset, Unset)
    FBOM_TYPE_CREATE_FUNCTION(UInt8, UInt8)
    FBOM_TYPE_CREATE_FUNCTION(UInt16, UInt16)
    FBOM_TYPE_CREATE_FUNCTION(UInt32, UInt32)
    FBOM_TYPE_CREATE_FUNCTION(UInt64, UInt64)
    FBOM_TYPE_CREATE_FUNCTION(Int8, Int8)
    FBOM_TYPE_CREATE_FUNCTION(Int16, Int16)
    FBOM_TYPE_CREATE_FUNCTION(Int32, Int32)
    FBOM_TYPE_CREATE_FUNCTION(Int64, Int64)
    FBOM_TYPE_CREATE_FUNCTION(Char, Char)
    FBOM_TYPE_CREATE_FUNCTION(Float, Float)
    FBOM_TYPE_CREATE_FUNCTION(Double, Double)
    FBOM_TYPE_CREATE_FUNCTION(Bool, Bool)
    FBOM_TYPE_CREATE_FUNCTION(Mat3f, Matrix3)
    FBOM_TYPE_CREATE_FUNCTION(Mat4f, Matrix4)
    FBOM_TYPE_CREATE_FUNCTION(Vec2f, Vec2f)
    FBOM_TYPE_CREATE_FUNCTION(Vec3f, Vec3f)
    FBOM_TYPE_CREATE_FUNCTION(Vec4f, Vec4f)
    FBOM_TYPE_CREATE_FUNCTION(Vec2i, Vec2i)
    FBOM_TYPE_CREATE_FUNCTION(Vec3i, Vec3i)
    FBOM_TYPE_CREATE_FUNCTION(Vec4i, Vec4i)
    FBOM_TYPE_CREATE_FUNCTION(Vec2u, Vec2u)
    FBOM_TYPE_CREATE_FUNCTION(Vec3u, Vec3u)
    FBOM_TYPE_CREATE_FUNCTION(Vec4u, Vec4u)
    FBOM_TYPE_CREATE_FUNCTION(Quat4f, Quaternion)

#undef FBOM_TYPE_CREATE_FUNCTION

    HYP_EXPORT void FBOMType_Destroy(FBOMType* ptr)
    {
        if (!ptr)
        {
            return;
        }

        delete ptr;
    }

    HYP_EXPORT bool FBOMType_Equals(FBOMType* lhs, FBOMType* rhs)
    {
        if (!lhs || !rhs)
        {
            return !lhs && !rhs;
        }

        return *lhs == *rhs;
    }

    HYP_EXPORT bool FBOMType_IsOrExtends(FBOMType* lhs, FBOMType* rhs, bool allowUnbounded)
    {
        if (!lhs || !rhs)
        {
            return false;
        }

        return lhs->IsOrExtends(*rhs, allowUnbounded);
    }

    HYP_EXPORT const char* FBOMType_GetName(const FBOMType* ptr)
    {
        if (!ptr)
        {
            return "";
        }

        return ptr->name.Data();
    }

    HYP_EXPORT void FBOMType_GetNativeTypeId(const FBOMType* ptr, TypeId* outTypeId)
    {
        if (!ptr || !outTypeId)
        {
            return;
        }

        *outTypeId = ptr->GetNativeTypeId();
    }

    HYP_EXPORT const HypClass* FBOMType_GetHypClass(const FBOMType* ptr)
    {
        if (!ptr)
        {
            return nullptr;
        }

        return ptr->GetHypClass();
    }

#pragma endregion FBOMType

#pragma region FBOMData

    HYP_EXPORT FBOMData* FBOMData_Create(FBOMType* typePtr)
    {
        return new FBOMData(typePtr != nullptr ? *typePtr : FBOMUnset());
    }

    HYP_EXPORT void FBOMData_Destroy(FBOMData* ptr)
    {
        if (!ptr)
        {
            return;
        }

        delete ptr;
    }

    HYP_EXPORT FBOMType* FBOMData_GetType(FBOMData* data)
    {
        if (!data)
        {
            return nullptr;
        }

        return new FBOMType(data->GetType());
    }

    HYP_EXPORT uint64 FBOMData_TotalSize(FBOMData* data)
    {
        if (!data)
        {
            return 0;
        }

        return data->TotalSize();
    }

#define FBOM_TYPE_GET_SET_FUNCTIONS(fbomTypeName, functionSuffix, cType)          \
    HYP_EXPORT bool FBOMData_Get##functionSuffix(FBOMData* data, cType* outValue) \
    {                                                                             \
        Assert(data != nullptr);                                                  \
                                                                                  \
        if (auto err = data->Read##fbomTypeName(outValue))                        \
        {                                                                         \
            return false;                                                         \
        }                                                                         \
                                                                                  \
        return true;                                                              \
    }                                                                             \
                                                                                  \
    HYP_EXPORT void FBOMData_Set##functionSuffix(FBOMData* data, cType* inValue)  \
    {                                                                             \
        Assert(data != nullptr);                                                  \
                                                                                  \
        *data = FBOMData::From##fbomTypeName(*inValue);                           \
    }

    FBOM_TYPE_GET_SET_FUNCTIONS(UInt8, UInt8, uint8)
    FBOM_TYPE_GET_SET_FUNCTIONS(UInt16, UInt16, uint16)
    FBOM_TYPE_GET_SET_FUNCTIONS(UInt32, UInt32, uint32)
    FBOM_TYPE_GET_SET_FUNCTIONS(UInt64, UInt64, uint64)
    FBOM_TYPE_GET_SET_FUNCTIONS(Int8, Int8, int8)
    FBOM_TYPE_GET_SET_FUNCTIONS(Int16, Int16, int16)
    FBOM_TYPE_GET_SET_FUNCTIONS(Int32, Int32, int32)
    FBOM_TYPE_GET_SET_FUNCTIONS(Int64, Int64, int64)
    FBOM_TYPE_GET_SET_FUNCTIONS(Char, Char, char)
    FBOM_TYPE_GET_SET_FUNCTIONS(Float, Float, float)
    FBOM_TYPE_GET_SET_FUNCTIONS(Double, Double, double)
    FBOM_TYPE_GET_SET_FUNCTIONS(Bool, Bool, bool)
    FBOM_TYPE_GET_SET_FUNCTIONS(Mat3f, Matrix3, Matrix3)
    FBOM_TYPE_GET_SET_FUNCTIONS(Mat4f, Matrix4, Matrix4)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec2f, Vec2f, Vec2f)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec3f, Vec3f, Vec3f)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec4f, Vec4f, Vec4f)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec2i, Vec2i, Vec2i)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec3i, Vec3i, Vec3i)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec4i, Vec4i, Vec4i)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec2u, Vec2u, Vec2u)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec3u, Vec3u, Vec3u)
    FBOM_TYPE_GET_SET_FUNCTIONS(Vec4u, Vec4u, Vec4u)
    FBOM_TYPE_GET_SET_FUNCTIONS(Quat4f, Quaternion, Quaternion)

    HYP_EXPORT bool FBOMData_GetObject(FBOMLoadContext* context, FBOMData* data, FBOMObject* outPtr)
    {
        Assert(context != nullptr);
        Assert(data != nullptr);
        Assert(outPtr != nullptr);

        if (auto err = data->ReadObject(*context, *outPtr))
        {
            return false;
        }

        return true;
    }

    HYP_EXPORT void FBOMData_SetObject(FBOMData* data, FBOMObject* inPtr)
    {
        Assert(data != nullptr);
        Assert(inPtr != nullptr);

        *data = FBOMData::FromObject(*inPtr);
    }

#undef FBOM_TYPE_GET_SET_FUNCTIONS

#pragma endregion FBOMData

#pragma region FBOMObject

    HYP_EXPORT FBOMObject* FBOMObject_Create()
    {
        return new FBOMObject();
    }

    HYP_EXPORT void FBOMObject_Destroy(FBOMObject* ptr)
    {
        if (!ptr)
        {
            return;
        }

        delete ptr;
    }

    HYP_EXPORT bool FBOMObject_GetProperty(FBOMObject* ptr, const char* key, FBOMData* outDataPtr)
    {
        Assert(ptr != nullptr);
        Assert(key != nullptr);
        Assert(outDataPtr != nullptr);

        if (!ptr->HasProperty(key))
        {
            return false;
        }

        *outDataPtr = ptr->GetProperty(key);

        return true;
    }

    HYP_EXPORT bool FBOMObject_SetProperty(FBOMObject* ptr, const char* name, FBOMData* dataPtr)
    {
        Assert(ptr != nullptr);
        Assert(name != nullptr);
        Assert(dataPtr != nullptr);

        ptr->SetProperty(name, *dataPtr);

        return true;
    }

    // HYP_EXPORT void *FBOMObject_GetDeserializedValue(FBOMObject *ptr)
    // {
    //     if (!ptr || !ptr->m_deserializedObject) {
    //         return nullptr;
    //     }

    //     return ptr->m_deserializedObject->anyValue.GetPointer();
    // }

#pragma endregion FBOMObject

} // extern "C"
