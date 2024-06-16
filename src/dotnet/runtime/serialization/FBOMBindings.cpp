/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/Serialization.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>

using namespace hyperion;
using namespace fbom;

extern "C" {

#pragma region FBOMType

#define FBOM_TYPE_CREATE_FUNCTION(fbom_type_name, function_suffix) \
    HYP_EXPORT FBOMType *FBOMType_##function_suffix() \
    { \
        return new FBOM##fbom_type_name(); \
    }

FBOM_TYPE_CREATE_FUNCTION(Unset, Unset)
FBOM_TYPE_CREATE_FUNCTION(UnsignedInt, UInt32)
FBOM_TYPE_CREATE_FUNCTION(UnsignedLong, UInt64)
FBOM_TYPE_CREATE_FUNCTION(Int, Int32)
FBOM_TYPE_CREATE_FUNCTION(Long, Int64)
FBOM_TYPE_CREATE_FUNCTION(Float, Float)
FBOM_TYPE_CREATE_FUNCTION(Bool, Bool)
FBOM_TYPE_CREATE_FUNCTION(Byte, Byte)
FBOM_TYPE_CREATE_FUNCTION(Name, Name)
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

HYP_EXPORT void FBOMType_Destroy(FBOMType *ptr)
{
    if (!ptr) {
        return;
    }

    delete ptr;
}

HYP_EXPORT bool FBOMType_Equals(FBOMType *lhs, FBOMType *rhs)
{
    if (!lhs || !rhs) {
        return !lhs && !rhs;
    }

    return *lhs == *rhs;
}

HYP_EXPORT bool FBOMType_IsOrExtends(FBOMType *lhs, FBOMType *rhs, bool allow_unbounded)
{
    if (!lhs || !rhs) {
        return false;
    }

    return lhs->IsOrExtends(*rhs, allow_unbounded);
}

#pragma endregion FBOMType

#pragma region FBOMData

HYP_EXPORT FBOMData *FBOMData_Create(FBOMType *type_ptr)
{
    return new FBOMData(type_ptr != nullptr ? *type_ptr : FBOMUnset());
}

HYP_EXPORT void FBOMData_Destroy(FBOMData *ptr)
{
    if (!ptr) {
        return;
    }

    delete ptr;
}

HYP_EXPORT FBOMType *FBOMData_GetType(FBOMData *data)
{
    if (!data) {
        return nullptr;
    }

    return new FBOMType(data->GetType());
}

HYP_EXPORT uint64 FBOMData_TotalSize(FBOMData *data)
{
    if (!data) {
        return 0;
    }

    return data->TotalSize();
}

#define FBOM_TYPE_GET_SET_FUNCTIONS(fbom_type_name, function_suffix, c_type) \
    HYP_EXPORT bool FBOMData_Get##function_suffix(FBOMData *data, c_type *out_value) \
    { \
        AssertThrow(data != nullptr); \
        \
        if (auto err = data->Read##fbom_type_name(out_value)) { \
            return false; \
        } \
        \
        return true; \
    } \
    \
    HYP_EXPORT void FBOMData_Set##function_suffix(FBOMData *data, c_type *in_value) \
    { \
        AssertThrow(data != nullptr); \
        \
        *data = FBOMData::From##fbom_type_name(*in_value); \
    }

FBOM_TYPE_GET_SET_FUNCTIONS(UnsignedInt, UInt32, uint32)
FBOM_TYPE_GET_SET_FUNCTIONS(UnsignedLong, UInt64, uint64)
FBOM_TYPE_GET_SET_FUNCTIONS(Int, Int32, int32)
FBOM_TYPE_GET_SET_FUNCTIONS(Long, Int64, int64)
FBOM_TYPE_GET_SET_FUNCTIONS(Float, Float, float)
FBOM_TYPE_GET_SET_FUNCTIONS(Bool, Bool, bool)
FBOM_TYPE_GET_SET_FUNCTIONS(Byte, Byte, ubyte)
FBOM_TYPE_GET_SET_FUNCTIONS(Name, Name, Name)
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

HYP_EXPORT bool FBOMData_GetObject(FBOMData *data, FBOMObject *out_ptr)
{
    AssertThrow(data != nullptr);
    AssertThrow(out_ptr != nullptr);

    if (auto err = data->ReadObject(*out_ptr)) {
        return false;
    }

    return true;
}

HYP_EXPORT void FBOMData_SetObject(FBOMData *data, FBOMObject *in_ptr)
{
    AssertThrow(data != nullptr);
    AssertThrow(in_ptr != nullptr);

    *data = FBOMData::FromObject(*in_ptr);
}

#undef FBOM_TYPE_GET_SET_FUNCTIONS

#pragma endregion FBOMData

#pragma region FBOMObject

HYP_EXPORT FBOMObject *FBOMObject_Create()
{
    return new FBOMObject();
}

HYP_EXPORT void FBOMObject_Destroy(FBOMObject *ptr)
{
    if (!ptr) {
        return;
    }

    delete ptr;
}

HYP_EXPORT bool FBOMObject_GetProperty(FBOMObject *ptr, const char *key, FBOMData *out_data_ptr)
{
    AssertThrow(ptr != nullptr);
    AssertThrow(out_data_ptr != nullptr);

    if (!ptr->HasProperty(key)) {
        return false;
    }

    *out_data_ptr = ptr->GetProperty(key);

    return true;
}

HYP_EXPORT bool FBOMObject_SetProperty(FBOMObject *ptr, Name *name_ptr, FBOMData *data_ptr)
{
    AssertThrow(ptr != nullptr);
    AssertThrow(name_ptr != nullptr);
    AssertThrow(data_ptr != nullptr);

    ptr->SetProperty(*name_ptr, *data_ptr);

    return true;
}

#pragma endregion FBOMObject

} // extern "C"
