#ifndef FBOM_BASE_TYPES_H
#define FBOM_BASE_TYPES_H

#include "type.h"
#include "../../util.h"

#include <string>

namespace hyperion {
namespace fbom {

using ::std::string;

struct FBOMUnset : FBOMType { FBOMUnset() : FBOMType() {} };
struct FBOMUnsignedInt : FBOMType { FBOMUnsignedInt() : FBOMType("u32", 4) {} };
struct FBOMUnsignedLong : FBOMType { FBOMUnsignedLong() : FBOMType("i64", 8) {} };
struct FBOMInt : FBOMType { FBOMInt() : FBOMType("i32", 4) {} };
struct FBOMLong : FBOMType { FBOMLong() : FBOMType("i64", 8) {} };
struct FBOMFloat : FBOMType { FBOMFloat() : FBOMType("f32", 4) {} };
struct FBOMBool : FBOMType { FBOMBool() : FBOMType("bool", 1) {} };
struct FBOMByte : FBOMType { FBOMByte() : FBOMType("byte", 1) {} };
struct FBOMStruct : FBOMType { FBOMStruct(size_t sz = 0) : FBOMType("struct", sz) {} };

struct FBOMArray : FBOMType {
    FBOMArray() : FBOMType("array", 0) {}

    FBOMArray(const FBOMType &held_type, size_t count)
        : FBOMType("array", held_type.size * count)
    {
        AssertThrowMsg(!held_type.IsUnbouned(), "Cannot create array of unbounded type");
    }
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

struct FBOMString : FBOMType {
    FBOMString() : FBOMString(0) {}
    FBOMString(size_t length) : FBOMType("string", length) {}
};

struct FBOMBaseObjectType : FBOMType { FBOMBaseObjectType() : FBOMType("object", 0) {} };

struct FBOMObjectType : FBOMType {
    explicit FBOMObjectType(void) : FBOMObjectType("UNSET") {}

    FBOMObjectType(const string &name)
        : FBOMObjectType(name, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const string &name, FBOMType extends)
        : FBOMType(name, 0, &extends)
    {
    }
};


} // namespace fbom
} // namespace hyperion

#endif
