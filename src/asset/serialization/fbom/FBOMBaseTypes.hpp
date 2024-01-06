#ifndef HYPERION_V2_FBOM_BASE_TYPES_HPP
#define HYPERION_V2_FBOM_BASE_TYPES_HPP

#include <core/Name.hpp>
#include <core/lib/String.hpp>
#include <core/Util.hpp>
#include <asset/serialization/fbom/FBOMType.hpp>
#include <Types.hpp>

namespace hyperion::v2::fbom {

struct FBOMUnset        : FBOMType { FBOMUnset() : FBOMType() {} };
struct FBOMUnsignedInt  : FBOMType { FBOMUnsignedInt() : FBOMType("u32", 4) { } };
struct FBOMUnsignedLong : FBOMType { FBOMUnsignedLong() : FBOMType("u64", 8) { } };
struct FBOMInt          : FBOMType { FBOMInt() : FBOMType("i32", 4) { } };
struct FBOMLong         : FBOMType { FBOMLong() : FBOMType("i64", 8) { } };
struct FBOMFloat        : FBOMType { FBOMFloat() : FBOMType("f32", 4) {} };
struct FBOMBool         : FBOMType { FBOMBool() : FBOMType("bool", 1) { } };
struct FBOMByte         : FBOMType { FBOMByte() : FBOMType("byte", 1) { } };
struct FBOMStruct       : FBOMType { FBOMStruct(SizeType sz) : FBOMType("struct", sz) { } };

struct FBOMArray : FBOMType
{
    FBOMArray() : FBOMType("array", 0) {}

    FBOMArray(const FBOMType &held_type, SizeType count)
        : FBOMType("array", held_type.size * count)
    {
        AssertThrowMsg(!held_type.IsUnbouned(), "Cannot create array of unbounded type");
    }
};

struct FBOMByteBuffer : FBOMType
{
    FBOMByteBuffer() : FBOMType("bytebuffer", 0) {}
    FBOMByteBuffer(SizeType count) : FBOMType("bytebuffer", count) {}
};

struct FBOMVec2f : FBOMArray { FBOMVec2f() : FBOMArray(FBOMFloat(), 2) {} };
struct FBOMVec3f : FBOMArray { FBOMVec3f() : FBOMArray(FBOMFloat(), 3) {} };
struct FBOMVec4f : FBOMArray { FBOMVec4f() : FBOMArray(FBOMFloat(), 4) {} };
struct FBOMVec2i : FBOMArray { FBOMVec2i() : FBOMArray(FBOMInt(), 2) {} };
struct FBOMVec3i : FBOMArray { FBOMVec3i() : FBOMArray(FBOMInt(), 3) {} };
struct FBOMVec4i : FBOMArray { FBOMVec4i() : FBOMArray(FBOMInt(), 4) {} };
struct FBOMVec2ui : FBOMArray { FBOMVec2ui() : FBOMArray(FBOMUnsignedInt(), 2) {} };
struct FBOMVec3ui : FBOMArray { FBOMVec3ui() : FBOMArray(FBOMUnsignedInt(), 3) {} };
struct FBOMVec4ui : FBOMArray { FBOMVec4ui() : FBOMArray(FBOMUnsignedInt(), 4) {} };

struct FBOMMat3 : FBOMArray { FBOMMat3() : FBOMArray(FBOMFloat(), 9) {} };
struct FBOMMat4 : FBOMArray { FBOMMat4() : FBOMArray(FBOMFloat(), 16) {} };

struct FBOMQuaternion : FBOMVec4f { FBOMQuaternion() : FBOMVec4f() {} };

struct FBOMString : FBOMType
{
    FBOMString() : FBOMString(0) {}
    FBOMString(SizeType length) : FBOMType("string", length) {}
};

struct FBOMName : FBOMUnsignedLong { };

struct FBOMBaseObjectType : FBOMType { FBOMBaseObjectType() : FBOMType("object", 0) {} };

struct FBOMObjectType : FBOMType
{
    explicit FBOMObjectType(void) : FBOMObjectType("UNSET") {}

    FBOMObjectType(const String &name)
        : FBOMObjectType(name, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const String &name, const FBOMType &extends)
        : FBOMType(name, 0, extends)
    {
    }
};

} // namespace hyperion::v2::fbom

#endif
