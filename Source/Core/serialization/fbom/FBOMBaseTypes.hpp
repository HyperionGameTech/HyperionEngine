/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Name.hpp>

#include <Core/reflection/TypeId.hpp>

#include <Core/containers/String.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/Util.hpp>

#include <Core/serialization/fbom/FBOMType.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
class Class;
} // namespace Hyperion

namespace Hyperion::serialization {

struct FBOMUnset : FBOMType
{
    FBOMUnset()
        : FBOMType()
    {
    }
};

struct HYP_API FBOMUInt8 : FBOMType
{
    FBOMUInt8();
};

struct HYP_API FBOMUInt16 : FBOMType
{
    FBOMUInt16();
};

struct HYP_API FBOMUInt32 : FBOMType
{
    FBOMUInt32();
};

struct HYP_API FBOMUInt64 : FBOMType
{
    FBOMUInt64();
};

struct HYP_API FBOMInt8 : FBOMType
{
    FBOMInt8();
};

struct HYP_API FBOMInt16 : FBOMType
{
    FBOMInt16();
};

struct HYP_API FBOMInt32 : FBOMType
{
    FBOMInt32();
};

struct HYP_API FBOMInt64 : FBOMType
{
    FBOMInt64();
};

struct HYP_API FBOMFloat : FBOMType
{
    FBOMFloat();
};

struct HYP_API FBOMDouble : FBOMType
{
    FBOMDouble();
};

struct HYP_API FBOMChar : FBOMType
{
    FBOMChar();
};

struct HYP_API FBOMBool : FBOMType
{
    FBOMBool();
};

struct HYP_API FBOMStruct : FBOMType
{
    template <class T>
    static constexpr bool isValidStructType = !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> && IsPodTypeV<T>;

    FBOMStruct();
    FBOMStruct(const ANSIStringView& typeName, SizeType sz, const TypeId& typeId);

    template <class T, bool CompileTimeChecked = true>
    FBOMStruct(TypeWrapper<T>, std::bool_constant<CompileTimeChecked> = {})
        : FBOMType(TypeNameWithoutNamespace<T>(), sizeof(T), TypeId::ForType<T>(), FBOMType("struct", sizeof(T), TypeId::ForType<T>()))
    {
        AssertStaticMsgCond(CompileTimeChecked, isValidStructType<T>, "T is not a valid type to use with FBOMStruct");
    }

    template <class T, bool CompileTimeChecked = true>
    static FBOMStruct Create()
    {
        return FBOMStruct(TypeWrapper<T> {}, std::bool_constant<CompileTimeChecked> {});
    }
};

struct HYP_API FBOMSequence : FBOMType
{
    FBOMSequence();
    FBOMSequence(const FBOMType& heldType);
    FBOMSequence(const FBOMType& heldType, SizeType count);
};

struct HYP_API FBOMByteBuffer : FBOMType
{
    FBOMByteBuffer();
    explicit FBOMByteBuffer(SizeType count);
};

struct HYP_API FBOMVec2f : FBOMType
{
    FBOMVec2f();
};

struct HYP_API FBOMVec3f : FBOMType
{
    FBOMVec3f();
};

struct HYP_API FBOMVec4f : FBOMType
{
    FBOMVec4f();
};

struct HYP_API FBOMVec2i : FBOMType
{
    FBOMVec2i();
};

struct HYP_API FBOMVec3i : FBOMType
{
    FBOMVec3i();
};

struct HYP_API FBOMVec4i : FBOMType
{
    FBOMVec4i();
};

struct HYP_API FBOMVec2u : FBOMType
{
    FBOMVec2u();
};

struct HYP_API FBOMVec3u : FBOMType
{
    FBOMVec3u();
};

struct HYP_API FBOMVec4u : FBOMType
{
    FBOMVec4u();
};

struct HYP_API FBOMMat3f : FBOMType
{
    FBOMMat3f();
};

struct HYP_API FBOMMat4f : FBOMType
{
    FBOMMat4f();
};

struct HYP_API FBOMQuat4f : FBOMType
{
    FBOMQuat4f();
};

struct HYP_API FBOMString : FBOMType
{
    FBOMString();
    explicit FBOMString(SizeType length);
};

struct HYP_API FBOMName : FBOMType
{
    FBOMName();
    explicit FBOMName(SizeType length);
};

struct HYP_API FBOMBaseObjectType : FBOMType
{
    FBOMBaseObjectType();
    explicit FBOMBaseObjectType(const FBOMType& extends);
};

struct FBOMObjectType : FBOMType
{
    explicit FBOMObjectType(const ANSIStringView& name)
        : FBOMType(name, 0, /* no valid TypeId */ TypeId::Void(), FBOMTypeFlags::CONTAINER, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const ANSIStringView& name, const FBOMType& extends)
        : FBOMType(name, 0, /* no valid TypeId */ TypeId::Void(), FBOMTypeFlags::CONTAINER, extends)
    {
        HYP_CORE_ASSERT(extends.IsOrExtends(FBOMBaseObjectType()),
            "Creating FBOMObjectType instance `%s` with parent type `%s`, but parent type does not extend `object`",
            name.Data(), extends.name.Data());
    }

    FBOMObjectType(const ANSIStringView& name, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends)
        : FBOMType(name, 0, /* no valid TypeId */ TypeId::Void(), flags, extends)
    {
        HYP_CORE_ASSERT(extends.IsOrExtends(FBOMBaseObjectType()),
            "Creating FBOMObjectType instance `%s` with parent type `%s`, but parent type does not extend `object`",
            name.Data(), extends.name.Data());
    }

    template <class T>
    explicit FBOMObjectType(TypeWrapper<T>)
        : FBOMType(TypeNameWithoutNamespace<T>(), 0, TypeId::ForType<T>(), FBOMTypeFlags::CONTAINER, FBOMBaseObjectType())
    {
    }

    template <class T>
    FBOMObjectType(TypeWrapper<T>, const FBOMType& extends)
        : FBOMType(TypeNameWithoutNamespace<T>(), 0, TypeId::ForType<T>(), FBOMTypeFlags::CONTAINER, extends)
    {
        HYP_CORE_ASSERT(extends.IsOrExtends(FBOMBaseObjectType()),
            "Creating FBOMObjectType instance `%s` with parent type `%s`, but parent type does not extend `object`",
            TypeNameWithoutNamespace<T>().Data(), extends.name.Data());
    }

    template <class T>
    FBOMObjectType(TypeWrapper<T>, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends)
        : FBOMType(TypeNameWithoutNamespace<T>(), 0, TypeId::ForType<T>(), flags, extends)
    {
        HYP_CORE_ASSERT(extends.IsOrExtends(FBOMBaseObjectType()),
            "Creating FBOMObjectType instance `%s` with parent type `%s`, but parent type does not extend `object`",
            TypeNameWithoutNamespace<T>().Data(), extends.name.Data());
    }

    FBOMObjectType(const ANSIStringView& name, const TypeId& typeId)
        : FBOMType(name, 0, typeId, FBOMTypeFlags::CONTAINER, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const ANSIStringView& name, const TypeId& typeId, const FBOMType& extends)
        : FBOMType(name, 0, typeId, FBOMTypeFlags::CONTAINER, extends)
    {
    }

    FBOMObjectType(const ANSIStringView& name, const TypeId& typeId, EnumFlags<FBOMTypeFlags> flags)
        : FBOMType(name, 0, typeId, flags, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const ANSIStringView& name, const TypeId& typeId, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends)
        : FBOMType(name, 0, typeId, flags, extends)
    {
    }

    explicit FBOMObjectType(const Class* cls);
};

struct FBOMPlaceholderType : FBOMType
{
    FBOMPlaceholderType()
        : FBOMType("<placeholder>", 0, /* no valid TypeId */ TypeId::Void(), FBOMTypeFlags::PLACEHOLDER, FBOMBaseObjectType())
    {
    }
};

struct FBOMArrayType : FBOMType
{
    FBOMArrayType()
        : FBOMType("array", 0, /* no valid TypeId */ TypeId::Void(), FBOMTypeFlags::DEFAULT)
    {
    }

    explicit FBOMArrayType(const FBOMType& extends)
        : FBOMType("array", 0, /* no valid TypeId */ TypeId::Void(), FBOMTypeFlags::DEFAULT, extends)
    {
    }
};

} // namespace Hyperion::serialization
