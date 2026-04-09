/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/serialization/fbom/FBOMBaseTypes.hpp>

#include <Core/reflection/TypeId.hpp>
#include <Core/reflection/Class.hpp>

namespace Hyperion::serialization {

FBOMUInt8::FBOMUInt8()
    : FBOMType("u8", 1, TypeId::ForType<uint8>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMUInt16::FBOMUInt16()
    : FBOMType("u16", 2, TypeId::ForType<uint16>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMUInt32::FBOMUInt32()
    : FBOMType("u32", 4, TypeId::ForType<uint32>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMUInt64::FBOMUInt64()
    : FBOMType("u64", 8, TypeId::ForType<uint64>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMInt8::FBOMInt8()
    : FBOMType("i8", 1, TypeId::ForType<int8>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMInt16::FBOMInt16()
    : FBOMType("i16", 2, TypeId::ForType<int16>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMInt32::FBOMInt32()
    : FBOMType("i32", 4, TypeId::ForType<int32>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMInt64::FBOMInt64()
    : FBOMType("i64", 8, TypeId::ForType<int64>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMFloat::FBOMFloat()
    : FBOMType("f32", 4, TypeId::ForType<float>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMDouble::FBOMDouble()
    : FBOMType("f64", 8, TypeId::ForType<double>(), FBOMTypeFlags::NUMERIC)
{
}

FBOMChar::FBOMChar()
    : FBOMType("char", 1, TypeId::ForType<char>())
{
}

FBOMBool::FBOMBool()
    : FBOMType("bool", 1, TypeId::ForType<bool>())
{
}

FBOMStruct::FBOMStruct()
    : FBOMType("struct", -1, /* no valid native TypeId */ TypeId::Void())
{
}

FBOMStruct::FBOMStruct(const ANSIStringView& typeName, size_t sz, const TypeId& typeId)
    : FBOMType(typeName, sz, typeId, FBOMType("struct", sz, typeId))
{
}

FBOMSequence::FBOMSequence()
    : FBOMType("seq", -1, /* no valid TypeId */ TypeId::Void())
{
}

FBOMSequence::FBOMSequence(const FBOMType& heldType)
    : FBOMType("seq", -1, /* no valid TypeId */ TypeId::Void())
{
    HYP_CORE_ASSERT(!heldType.IsUnbounded(), "Cannot create sequence of unbounded type");
}

FBOMSequence::FBOMSequence(const FBOMType& heldType, size_t count)
    : FBOMType("seq", heldType.size * count, /* no valid TypeId */ TypeId::Void())
{
    HYP_CORE_ASSERT(!heldType.IsUnbounded(), "Cannot create sequence of unbounded type");
}

FBOMByteBuffer::FBOMByteBuffer()
    : FBOMType("buf", -1, TypeId::ForType<ByteBuffer>())
{
}

FBOMByteBuffer::FBOMByteBuffer(size_t count)
    : FBOMType("buf", count, TypeId::ForType<ByteBuffer>())
{
}

FBOMVec2f::FBOMVec2f()
    : FBOMType("vec2f", 8, TypeId::ForType<Vec2f>(), FBOMSequence(FBOMFloat(), 2))
{
}

FBOMVec3f::FBOMVec3f()
    : FBOMType("vec3f", 16, TypeId::ForType<Vec3f>(), FBOMSequence(FBOMFloat(), 4 /* 3 + 1 for padding */))
{
}

FBOMVec4f::FBOMVec4f()
    : FBOMType("vec4f", 16, TypeId::ForType<Vec4f>(), FBOMSequence(FBOMFloat(), 4))
{
}

FBOMVec2i::FBOMVec2i()
    : FBOMType("vec2i", 8, TypeId::ForType<Vec2i>(), FBOMSequence(FBOMInt32(), 2))
{
}

FBOMVec3i::FBOMVec3i()
    : FBOMType("vec3i", 16, TypeId::ForType<Vec3i>(), FBOMSequence(FBOMInt32(), 4 /* 3 + 1 for padding */))
{
}

FBOMVec4i::FBOMVec4i()
    : FBOMType("vec4i", 16, TypeId::ForType<Vec4i>(), FBOMSequence(FBOMInt32(), 4))
{
}

FBOMVec2u::FBOMVec2u()
    : FBOMType("vec2u", 8, TypeId::ForType<Vec2u>(), FBOMSequence(FBOMUInt32(), 2))
{
}

FBOMVec3u::FBOMVec3u()
    : FBOMType("vec3u", 16, TypeId::ForType<Vec3u>(), FBOMSequence(FBOMUInt32(), 4 /* 3 + 1 for padding */))
{
}

FBOMVec4u::FBOMVec4u()
    : FBOMType("vec4u", 16, TypeId::ForType<Vec4u>(), FBOMSequence(FBOMUInt32(), 4))
{
}

FBOMMat3f::FBOMMat3f()
    : FBOMType("mat3f", 48, TypeId::ForType<Mat3f>(), FBOMSequence(FBOMFloat(), 12))
{
}

FBOMMat4f::FBOMMat4f()
    : FBOMType("mat4f", 64, TypeId::ForType<Mat4f>(), FBOMSequence(FBOMFloat(), 16))
{
}

FBOMQuat4f::FBOMQuat4f()
    : FBOMType("quat4f", 16, TypeId::ForType<Quaternion>(), FBOMSequence(FBOMFloat(), 4))
{
}

FBOMString::FBOMString()
    : FBOMString(-1)
{
}

FBOMString::FBOMString(size_t length)
    : FBOMType("string", length, TypeId::ForType<String>())
{
}

FBOMName::FBOMName()
    : FBOMName(-1)
{
}

FBOMName::FBOMName(size_t length)
    : FBOMType("name", length, TypeId::ForType<Name>())
{
}

FBOMBaseObjectType::FBOMBaseObjectType()
    : FBOMType("object", 0, /* no valid TypeId */ TypeId::Void(), FBOMTypeFlags::DEFAULT)
{
}

FBOMBaseObjectType::FBOMBaseObjectType(const FBOMType& extends)
    : FBOMType("object", 0, /* no valid TypeId */ TypeId::Void(), FBOMTypeFlags::DEFAULT, extends)
{
}

FBOMObjectType::FBOMObjectType(const Class* cls)
    : FBOMType(
          cls->GetName().LookupString(),
          size_t(-1),
          cls->GetTypeId(),
          FBOMTypeFlags::CONTAINER,
          cls->GetParent() ? FBOMType(FBOMObjectType(cls->GetParent())) : FBOMType(FBOMBaseObjectType()))
{
}

} // namespace Hyperion::serialization
