/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_BASE_TYPES_HPP
#define HYPERION_FBOM_BASE_TYPES_HPP

#include <core/Name.hpp>

#include <core/utilities/TypeID.hpp>

#include <core/containers/String.hpp>

#include <core/Util.hpp>

#include <core/serialization/fbom/FBOMType.hpp>

#include <Types.hpp>

namespace hyperion {
class HypClass;
} // namespace hyperion

namespace hyperion::serialization {

struct FBOMUnset : FBOMType
{
    FBOMUnset()
        : FBOMType()
    {
    }
};

struct FBOMUInt8 : FBOMType
{
    FBOMUInt8()
        : FBOMType("u8", 1, TypeID::ForType<uint8>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMUInt16 : FBOMType
{
    FBOMUInt16()
        : FBOMType("u16", 2, TypeID::ForType<uint16>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMUInt32 : FBOMType
{
    FBOMUInt32()
        : FBOMType("u32", 4, TypeID::ForType<uint32>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMUInt64 : FBOMType
{
    FBOMUInt64()
        : FBOMType("u64", 8, TypeID::ForType<uint64>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMInt8 : FBOMType
{
    FBOMInt8()
        : FBOMType("i8", 1, TypeID::ForType<int8>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMInt16 : FBOMType
{
    FBOMInt16()
        : FBOMType("i16", 2, TypeID::ForType<int16>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMInt32 : FBOMType
{
    FBOMInt32()
        : FBOMType("i32", 4, TypeID::ForType<int32>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMInt64 : FBOMType
{
    FBOMInt64()
        : FBOMType("i64", 8, TypeID::ForType<int64>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMFloat : FBOMType
{
    FBOMFloat()
        : FBOMType("f32", 4, TypeID::ForType<float>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMDouble : FBOMType
{
    FBOMDouble()
        : FBOMType("f64", 8, TypeID::ForType<double>(), FBOMTypeFlags::NUMERIC)
    {
    }
};

struct FBOMChar : FBOMType
{
    FBOMChar()
        : FBOMType("char", 1, TypeID::ForType<char>())
    {
    }
};

struct FBOMBool : FBOMType
{
    FBOMBool()
        : FBOMType("bool", 1, TypeID::ForType<bool>())
    {
    }
};

struct FBOMStruct : FBOMType
{
    template <class T>
    static constexpr bool is_valid_struct_type = !std::is_pointer_v<T>
        && !std::is_reference_v<T>
        && !std::is_const_v<T>
        && !std::is_volatile_v<T>
        && is_pod_type<T>;

    FBOMStruct()
        : FBOMType("struct", -1, /* no valid native TypeID */ TypeID::Void())
    {
    }

    FBOMStruct(const ANSIStringView& type_name, SizeType sz, TypeID type_id)
        : FBOMType(type_name, sz, type_id, FBOMType("struct", sz, type_id))
    {
    }

    template <class T, bool CompileTimeChecked = true>
    FBOMStruct(TypeWrapper<T>, std::bool_constant<CompileTimeChecked> = {})
        : FBOMType(TypeNameWithoutNamespace<T>(), sizeof(T), TypeID::ForType<T>(), FBOMType("struct", sizeof(T), TypeID::ForType<T>()))
    {
        AssertStaticMsgCond(CompileTimeChecked, is_valid_struct_type<T>, "T is not a valid type to use with FBOMStruct");
    }

    template <class T, bool CompileTimeChecked = true>
    static FBOMStruct Create()
    {
        return FBOMStruct(TypeWrapper<T> {}, std::bool_constant<CompileTimeChecked> {});
    }
};

struct FBOMSequence : FBOMType
{
    FBOMSequence()
        : FBOMType("seq", -1, /* no valid TypeID */ TypeID::Void())
    {
    }

    FBOMSequence(const FBOMType& held_type)
        : FBOMType("seq", -1, /* no valid TypeID */ TypeID::Void())
    {
        AssertThrowMsg(!held_type.IsUnbounded(), "Cannot create sequence of unbounded type");
    }

    FBOMSequence(const FBOMType& held_type, SizeType count)
        : FBOMType("seq", held_type.size * count, /* no valid TypeID */ TypeID::Void())
    {
        AssertThrowMsg(!held_type.IsUnbounded(), "Cannot create sequence of unbounded type");
    }
};

struct FBOMByteBuffer : FBOMType
{
    FBOMByteBuffer()
        : FBOMType("buf", -1, TypeID::ForType<ByteBuffer>())
    {
    }

    FBOMByteBuffer(SizeType count)
        : FBOMType("buf", count, TypeID::ForType<ByteBuffer>())
    {
    }
};

struct FBOMVec2f : FBOMType
{
    FBOMVec2f()
        : FBOMType("vec2f", 8, TypeID::ForType<Vec2f>(), FBOMSequence(FBOMFloat(), 2))
    {
    }
};

struct FBOMVec3f : FBOMType
{
    FBOMVec3f()
        : FBOMType("vec3f", 16, TypeID::ForType<Vec3f>(), FBOMSequence(FBOMFloat(), 4 /* 3 + 1 for padding */))
    {
    }
};

struct FBOMVec4f : FBOMType
{
    FBOMVec4f()
        : FBOMType("vec4f", 16, TypeID::ForType<Vec4f>(), FBOMSequence(FBOMFloat(), 4))
    {
    }
};

struct FBOMVec2i : FBOMType
{
    FBOMVec2i()
        : FBOMType("vec2i", 8, TypeID::ForType<Vec2i>(), FBOMSequence(FBOMInt32(), 2))
    {
    }
};

struct FBOMVec3i : FBOMType
{
    FBOMVec3i()
        : FBOMType("vec3i", 16, TypeID::ForType<Vec3i>(), FBOMSequence(FBOMInt32(), 4 /* 3 + 1 for padding */))
    {
    }
};

struct FBOMVec4i : FBOMType
{
    FBOMVec4i()
        : FBOMType("vec4i", 16, TypeID::ForType<Vec4i>(), FBOMSequence(FBOMInt32(), 4))
    {
    }
};

struct FBOMVec2u : FBOMType
{
    FBOMVec2u()
        : FBOMType("vec2u", 8, TypeID::ForType<Vec2u>(), FBOMSequence(FBOMUInt32(), 2))
    {
    }
};

struct FBOMVec3u : FBOMType
{
    FBOMVec3u()
        : FBOMType("vec3u", 16, TypeID::ForType<Vec3u>(), FBOMSequence(FBOMUInt32(), 4 /* 3 + 1 for padding */))
    {
    }
};

struct FBOMVec4u : FBOMType
{
    FBOMVec4u()
        : FBOMType("vec4u", 16, TypeID::ForType<Vec4u>(), FBOMSequence(FBOMUInt32(), 4))
    {
    }
};

struct FBOMMat3f : FBOMType
{
    FBOMMat3f()
        : FBOMType("mat3f", 48, TypeID::ForType<Matrix3>(), FBOMSequence(FBOMFloat(), 12))
    {
    }
};

struct FBOMMat4f : FBOMType
{
    FBOMMat4f()
        : FBOMType("mat4f", 64, TypeID::ForType<Matrix4>(), FBOMSequence(FBOMFloat(), 16))
    {
    }
};

struct FBOMQuat4f : FBOMType
{
    FBOMQuat4f()
        : FBOMType("quat4f", 16, TypeID::ForType<Quaternion>(), FBOMSequence(FBOMFloat(), 4))
    {
    }
};

struct FBOMString : FBOMType
{
    FBOMString()
        : FBOMString(-1)
    {
    }

    FBOMString(SizeType length)
        : FBOMType("string", length, TypeID::ForType<String>())
    {
    }
};

struct FBOMBaseObjectType : FBOMType
{
    FBOMBaseObjectType()
        : FBOMType("object", 0, /* no valid TypeID */ TypeID::Void(), FBOMTypeFlags::DEFAULT)
    {
    }

    FBOMBaseObjectType(const FBOMType& extends)
        : FBOMType("object", 0, /* no valid TypeID */ TypeID::Void(), FBOMTypeFlags::DEFAULT, extends)
    {
    }
};

struct FBOMObjectType : FBOMType
{
    FBOMObjectType(const ANSIStringView& name)
        : FBOMType(name, 0, /* no valid TypeID */ TypeID::Void(), FBOMTypeFlags::CONTAINER, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const ANSIStringView& name, const FBOMType& extends)
        : FBOMType(name, 0, /* no valid TypeID */ TypeID::Void(), FBOMTypeFlags::CONTAINER, extends)
    {
        AssertThrowMsg(extends.IsOrExtends(FBOMBaseObjectType()),
            "Creating FBOMObjectType instance `%s` with parent type `%s`, but parent type does not extend `object`",
            name.Data(), extends.name.Data());
    }

    FBOMObjectType(const ANSIStringView& name, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends)
        : FBOMType(name, 0, /* no valid TypeID */ TypeID::Void(), flags, extends)
    {
        AssertThrowMsg(extends.IsOrExtends(FBOMBaseObjectType()),
            "Creating FBOMObjectType instance `%s` with parent type `%s`, but parent type does not extend `object`",
            name.Data(), extends.name.Data());
    }

    template <class T>
    explicit FBOMObjectType(TypeWrapper<T>)
        : FBOMType(TypeNameWithoutNamespace<T>(), 0, TypeID::ForType<T>(), FBOMTypeFlags::CONTAINER, FBOMBaseObjectType())
    {
    }

    template <class T>
    FBOMObjectType(TypeWrapper<T>, const FBOMType& extends)
        : FBOMType(TypeNameWithoutNamespace<T>(), 0, TypeID::ForType<T>(), FBOMTypeFlags::CONTAINER, extends)
    {
        AssertThrowMsg(extends.IsOrExtends(FBOMBaseObjectType()),
            "Creating FBOMObjectType instance `%s` with parent type `%s`, but parent type does not extend `object`",
            TypeNameWithoutNamespace<T>().Data(), extends.name.Data());
    }

    template <class T>
    FBOMObjectType(TypeWrapper<T>, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends)
        : FBOMType(TypeNameWithoutNamespace<T>(), 0, TypeID::ForType<T>(), flags, extends)
    {
        AssertThrowMsg(extends.IsOrExtends(FBOMBaseObjectType()),
            "Creating FBOMObjectType instance `%s` with parent type `%s`, but parent type does not extend `object`",
            TypeNameWithoutNamespace<T>().Data(), extends.name.Data());
    }

    FBOMObjectType(const ANSIStringView& name, const TypeID& type_id)
        : FBOMType(name, 0, type_id, FBOMTypeFlags::CONTAINER, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const ANSIStringView& name, const TypeID& type_id, const FBOMType& extends)
        : FBOMType(name, 0, type_id, FBOMTypeFlags::CONTAINER, extends)
    {
    }

    FBOMObjectType(const ANSIStringView& name, const TypeID& type_id, EnumFlags<FBOMTypeFlags> flags)
        : FBOMType(name, 0, type_id, flags, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const ANSIStringView& name, const TypeID& type_id, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends)
        : FBOMType(name, 0, type_id, flags, extends)
    {
    }

    explicit FBOMObjectType(const HypClass* hyp_class);
};

struct FBOMPlaceholderType : FBOMType
{
    FBOMPlaceholderType()
        : FBOMType("<placeholder>", 0, /* no valid TypeID */ TypeID::Void(), FBOMTypeFlags::PLACEHOLDER, FBOMBaseObjectType())
    {
    }
};

struct FBOMArrayType : FBOMType
{
    FBOMArrayType()
        : FBOMType("array", 0, /* no valid TypeID */ TypeID::Void(), FBOMTypeFlags::DEFAULT)
    {
    }

    FBOMArrayType(const FBOMType& extends)
        : FBOMType("array", 0, /* no valid TypeID */ TypeID::Void(), FBOMTypeFlags::DEFAULT, extends)
    {
    }
};

} // namespace hyperion::serialization

#endif
