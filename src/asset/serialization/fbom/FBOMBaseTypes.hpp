/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_BASE_TYPES_HPP
#define HYPERION_FBOM_BASE_TYPES_HPP

#include <core/Name.hpp>
#include <core/containers/String.hpp>
#include <core/Util.hpp>

#include <asset/serialization/fbom/FBOMType.hpp>

#include <Types.hpp>

namespace hyperion::fbom {

struct FBOMUnset        : FBOMType { FBOMUnset() : FBOMType() {} };
struct FBOMUnsignedInt  : FBOMType { FBOMUnsignedInt() : FBOMType("u32", 4) { } };
struct FBOMUnsignedLong : FBOMType { FBOMUnsignedLong() : FBOMType("u64", 8) { } };
struct FBOMInt          : FBOMType { FBOMInt() : FBOMType("i32", 4) { } };
struct FBOMLong         : FBOMType { FBOMLong() : FBOMType("i64", 8) { } };
struct FBOMFloat        : FBOMType { FBOMFloat() : FBOMType("f32", 4) {} };
struct FBOMBool         : FBOMType { FBOMBool() : FBOMType("bool", 1) { } };
struct FBOMByte         : FBOMType { FBOMByte() : FBOMType("byte", 1) { } };

struct FBOMStruct : FBOMType
{
    FBOMStruct(const char *type_name, SizeType sz)
        : FBOMType(type_name, sz)
    {
    }

    template <class T>
    static FBOMStruct Create()
    {
        static_assert(!std::is_pointer_v<T>, "Cannot create struct of pointer type");
        static_assert(!std::is_reference_v<T>, "Cannot create struct of reference type");
        static_assert(!std::is_const_v<T>, "Cannot create struct of const type");
        static_assert(!std::is_volatile_v<T>, "Cannot create struct of volatile type");

        static_assert(std::is_standard_layout_v<T>, "Cannot create struct of non-standard layout type");
        static_assert(std::is_trivially_copyable_v<T>, "Cannot create struct of non-trivially copyable type");

        return FBOMStruct(TypeNameWithoutNamespace<T>().Data(), sizeof(T));
    }
};

struct FBOMSequence : FBOMType
{
    FBOMSequence() : FBOMType("seq", -1) {}

    FBOMSequence(const FBOMType &held_type, SizeType count)
        : FBOMType("seq", held_type.size * count)
    {
        AssertThrowMsg(!held_type.IsUnbouned(), "Cannot create sequence of unbounded type");
    }
};

struct FBOMByteBuffer : FBOMType
{
    FBOMByteBuffer()
        : FBOMType("buf", 0)
    {
    }

    FBOMByteBuffer(SizeType count)
        : FBOMType("buf", count)
    {
    }
};

struct FBOMVec2f : FBOMSequence { FBOMVec2f() : FBOMSequence(FBOMFloat(), 2) {} };
struct FBOMVec3f : FBOMSequence { FBOMVec3f() : FBOMSequence(FBOMFloat(), 4 /* 3 + 1 for padding */) {} };
struct FBOMVec4f : FBOMSequence { FBOMVec4f() : FBOMSequence(FBOMFloat(), 4) {} };
struct FBOMVec2i : FBOMSequence { FBOMVec2i() : FBOMSequence(FBOMInt(), 2) {} };
struct FBOMVec3i : FBOMSequence { FBOMVec3i() : FBOMSequence(FBOMInt(), 4 /* 3 + 1 for padding */) {} };
struct FBOMVec4i : FBOMSequence { FBOMVec4i() : FBOMSequence(FBOMInt(), 4) {} };
struct FBOMVec2ui : FBOMSequence { FBOMVec2ui() : FBOMSequence(FBOMUnsignedInt(), 2) {} };
struct FBOMVec3ui : FBOMSequence { FBOMVec3ui() : FBOMSequence(FBOMUnsignedInt(), 4 /* 3 + 1 for padding */) {} };
struct FBOMVec4ui : FBOMSequence { FBOMVec4ui() : FBOMSequence(FBOMUnsignedInt(), 4) {} };

struct FBOMMat3 : FBOMSequence { FBOMMat3() : FBOMSequence(FBOMFloat(), 9) {} };
struct FBOMMat4 : FBOMSequence { FBOMMat4() : FBOMSequence(FBOMFloat(), 16) {} };

struct FBOMQuaternion : FBOMVec4f { FBOMQuaternion() : FBOMVec4f() {} };

struct FBOMString : FBOMType
{
    FBOMString()
        : FBOMString(0)
    {
    }

    FBOMString(SizeType length)
        : FBOMType("string", length)
    {
    }
};

struct FBOMName : FBOMUnsignedLong { };

struct FBOMBaseObjectType : FBOMType
{
    FBOMBaseObjectType()
        : FBOMType("object", 0)
    {
    }
};

struct FBOMObjectType : FBOMType
{
    explicit FBOMObjectType(void)
        : FBOMObjectType("UNSET")
    {
    }

    FBOMObjectType(const ANSIStringView &name)
        : FBOMObjectType(name, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const ANSIStringView &name, const FBOMType &extends)
        : FBOMType(name, 0, extends)
    {
    }
};

// /*! \brief An in-memory representation of an object. Cannot be serialized. */
// struct FBOMMemoryObjectType : FBOMType
// {
//     FBOMMemoryObjectType()
//         : FBOMType("memory_object", 0)
//     {
//     }
// };

} // namespace hyperion::fbom

#endif
