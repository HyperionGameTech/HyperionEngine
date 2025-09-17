#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstHashMap.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstUndefined.hpp>

namespace hyperion {

SymbolType* BuiltinTypes::s_primitiveType = SymbolTypeRef(new SymbolType(
    "<primitive>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

SymbolType* BuiltinTypes::s_errorType = SymbolTypeRef(new SymbolType(
    "<error>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

SymbolType* BuiltinTypes::s_anyType = SymbolTypeRef(new SymbolType(
    "any",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

SymbolType* BuiltinTypes::s_classType = SymbolTypeRef(new SymbolType(
    "<class>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

SymbolType* BuiltinTypes::s_placeholderType = SymbolTypeRef(new SymbolType(
    "<placeholder>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

SymbolType* BuiltinTypes::s_voidType = SymbolType::Primitive(
    "void",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)));

SymbolType* BuiltinTypes::s_objectType = SymbolTypeRef(new SymbolType(
    "object",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

SymbolType* BuiltinTypes::s_functionBaseType = SymbolType::Primitive(
    "FunctionBase",
    nullptr);

SymbolType* BuiltinTypes::s_arrayBaseType = SymbolType::Primitive(
    "ArrayBase",
    nullptr);

SymbolType* BuiltinTypes::s_varArgsBaseType = SymbolType::Extend(
    "VarArgsBase",
    BuiltinTypes::s_arrayBaseType,
    {},
    {});

SymbolType* BuiltinTypes::s_mapBaseType = SymbolType::Primitive(
    "MapBase",
    nullptr);

SymbolType* BuiltinTypes::s_int8Type = SymbolType::Primitive(
    "int8",
    RC<AstInteger>(new AstInteger(0, CBS_8, SourceLocation::eof)),
    CBS_8);

SymbolType* BuiltinTypes::s_int16Type = SymbolType::Primitive(
    "int16",
    RC<AstInteger>(new AstInteger(0, CBS_16, SourceLocation::eof)),
    CBS_16);

SymbolType* BuiltinTypes::s_int32Type = SymbolType::Primitive(
    "int32",
    RC<AstInteger>(new AstInteger(0, CBS_32, SourceLocation::eof)),
    CBS_32);

SymbolType* BuiltinTypes::s_int64Type = SymbolType::Primitive(
    "int64",
    RC<AstInteger>(new AstInteger(0, CBS_64, SourceLocation::eof)),
    CBS_64);

SymbolType* BuiltinTypes::s_uint8Type = SymbolType::Primitive(
    "uint8",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_8, SourceLocation::eof)),
    CBS_8);

SymbolType* BuiltinTypes::s_uint16Type = SymbolType::Primitive(
    "uint16",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_16, SourceLocation::eof)),
    CBS_16);

SymbolType* BuiltinTypes::s_uint32Type = SymbolType::Primitive(
    "uint32",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_32, SourceLocation::eof)),
    CBS_32);

SymbolType* BuiltinTypes::s_uint64Type = SymbolType::Primitive(
    "uint64",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_64, SourceLocation::eof)),
    CBS_64);

SymbolType* BuiltinTypes::s_floatType = SymbolType::Primitive(
    "float",
    RC<AstFloat>(new AstFloat(0.0, CBS_32, SourceLocation::eof)),
    CBS_32);

SymbolType* BuiltinTypes::s_doubleType = SymbolType::Primitive(
    "double",
    RC<AstFloat>(new AstFloat(0.0, CBS_64, SourceLocation::eof)),
    CBS_64);

SymbolType* BuiltinTypes::s_boolType = SymbolType::Primitive(
    "bool",
    RC<AstFalse>(new AstFalse(SourceLocation::eof)));

SymbolType* BuiltinTypes::s_stringType = SymbolType::Primitive(
    "string",
    RC<AstString>(new AstString("", SourceLocation::eof)));

SymbolType* BuiltinTypes::s_nullType = SymbolType::Primitive(
    "<null>",
    RC<AstNil>(new AstNil(SourceLocation::eof)));

SymbolType* BuiltinTypes::s_varArgsType = SymbolType::Generic(
    "VarArgs",
    s_varArgsBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "type", SymbolType::GenericParameter("T") } } });

SymbolType* BuiltinTypes::s_functionType = SymbolType::Generic(
    "Function",
    BuiltinTypes::s_functionBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "@return", SymbolType::GenericParameter("ReturnType") },
            { "@args", SymbolType::GenericInstance(BuiltinTypes::s_varArgsType, {}, {}, GenericInstanceTypeInfo {}) } } });

// See ScriptArrayWrapper.cpp in Hyperion Engine for implementation of array methods
SymbolType* BuiltinTypes::s_arrayType = SymbolType::Generic(
    "Array",
    BuiltinTypes::s_arrayBaseType,
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "operator[]",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", SymbolType::Placeholder("SelfType") },
                        { "index", BuiltinTypes::s_uint64Type } } }) },
        SymbolTypeMember {
            "operator[]=",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", SymbolType::Placeholder("SelfType") },
                        { "index", BuiltinTypes::s_uint64Type },
                        { "value", SymbolType::GenericParameter("T") } } }) },
        SymbolTypeMember {
            "PushBack",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", SymbolType::Placeholder("SelfType") },
                        { "value", SymbolType::GenericParameter("T") } } }) },
        SymbolTypeMember {
            "PopBack",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", SymbolType::Placeholder("SelfType") } } }) },
        SymbolTypeMember {
            "Clear",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", BuiltinTypes::s_voidType },
                        { "self", SymbolType::Placeholder("SelfType") } } }) },
        SymbolTypeMember {
            "Resize",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", BuiltinTypes::s_voidType },
                        { "self", SymbolType::Placeholder("SelfType") },
                        { "newSize", BuiltinTypes::s_uint64Type } } }) },
        SymbolTypeMember {
            "Size",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", BuiltinTypes::s_uint64Type },
                        { "self", SymbolType::Placeholder("SelfType") } } }) } },
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

SymbolType* BuiltinTypes::s_mapType = SymbolType::Generic(
    "Map",
    BuiltinTypes::s_mapBaseType,
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "operator[]",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("V") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::s_anyType },
                        { "key", SymbolType::GenericParameter("K") } } }) },
        SymbolTypeMember {
            "operator[]=",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("V") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::s_anyType },
                        { "key", SymbolType::GenericParameter("K") },
                        { "value", SymbolType::GenericParameter("V") } } }) } },
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "key", SymbolType::GenericParameter("K") },
            { "value", SymbolType::GenericParameter("V") } } });

void BuiltinTypes::Initialize()
{
#pragma region String
    s_stringType->GetMembers().PushBack(SymbolTypeMember {
        "operator+",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                { { "@return", BuiltinTypes::s_stringType },
                    { "self", BuiltinTypes::s_stringType },
                    { "other", BuiltinTypes::s_stringType } } }) });

    s_stringType->GetMembers().PushBack(SymbolTypeMember {
        "Length",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                { { "@return", BuiltinTypes::s_uint64Type },
                    { "self", BuiltinTypes::s_stringType } } }) });
#pragma endregion String
}

void BuiltinTypes::AddToSymbolTable(IdentifierTable& table)
{
    static SymbolType* s_intType = SymbolType::Alias("int", { BuiltinTypes::s_int32Type });
    static SymbolType* s_uintType = SymbolType::Alias("uint", { BuiltinTypes::s_uint32Type });
    static SymbolType* s_uintptrType = SymbolType::Alias("UIntPtr", { sizeof(void*) == 4 ? BuiltinTypes::s_uint32Type : BuiltinTypes::s_uint64Type });
    static SymbolType* s_intptrType = SymbolType::Alias("IntPtr", { sizeof(void*) == 4 ? BuiltinTypes::s_int32Type : BuiltinTypes::s_int64Type });

    static SymbolType* s_vec2iType = SymbolType::Object(
        "Vec2i", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_int32Type },
            SymbolTypeMember { "y", BuiltinTypes::s_int32Type } },
        {});

    static SymbolType* s_vec2uType = SymbolType::Object(
        "Vec2u", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_uint32Type },
            SymbolTypeMember { "y", BuiltinTypes::s_uint32Type } },
        {});

    static SymbolType* s_vec2fType = SymbolType::Object(
        "Vec2f", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_floatType },
            SymbolTypeMember { "y", BuiltinTypes::s_floatType } },
        {});

    static SymbolType* s_vec3iType = SymbolType::Object(
        "Vec3i", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_int32Type },
            SymbolTypeMember { "y", BuiltinTypes::s_int32Type },
            SymbolTypeMember { "z", BuiltinTypes::s_int32Type } },
        {});

    static SymbolType* s_vec3uType = SymbolType::Object(
        "Vec3u", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_uint32Type },
            SymbolTypeMember { "y", BuiltinTypes::s_uint32Type },
            SymbolTypeMember { "z", BuiltinTypes::s_uint32Type } },
        {});

    static SymbolType* s_vec3fType = SymbolType::Object(
        "Vec3f", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_floatType },
            SymbolTypeMember { "y", BuiltinTypes::s_floatType },
            SymbolTypeMember { "z", BuiltinTypes::s_floatType } },
        {});

    static SymbolType* s_vec4iType = SymbolType::Object(
        "Vec4i", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_int32Type },
            SymbolTypeMember { "y", BuiltinTypes::s_int32Type },
            SymbolTypeMember { "z", BuiltinTypes::s_int32Type },
            SymbolTypeMember { "w", BuiltinTypes::s_int32Type } },
        {});

    static SymbolType* s_vec4uType = SymbolType::Object(
        "Vec4u", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_uint32Type },
            SymbolTypeMember { "y", BuiltinTypes::s_uint32Type },
            SymbolTypeMember { "z", BuiltinTypes::s_uint32Type },
            SymbolTypeMember { "w", BuiltinTypes::s_uint32Type } },
        {});

    static SymbolType* s_vec4fType = SymbolType::Object(
        "Vec4f", nullptr,
        { SymbolTypeMember { "x", BuiltinTypes::s_floatType },
            SymbolTypeMember { "y", BuiltinTypes::s_floatType },
            SymbolTypeMember { "z", BuiltinTypes::s_floatType },
            SymbolTypeMember { "w", BuiltinTypes::s_floatType } },
        {});

    static SymbolType* s_nameType = SymbolType::Primitive(
        "Name",
        nullptr);

    static SymbolType* s_byteBufferType = SymbolType::Object(
        "ByteBuffer", nullptr,
        {}, {});

    static SymbolType* const s_globalVisibleTypes[] {
        BuiltinTypes::s_anyType,
        BuiltinTypes::s_objectType,
        BuiltinTypes::s_voidType,
        BuiltinTypes::s_int8Type,
        BuiltinTypes::s_int16Type,
        BuiltinTypes::s_int32Type,
        BuiltinTypes::s_int64Type,
        BuiltinTypes::s_uint8Type,
        BuiltinTypes::s_uint16Type,
        BuiltinTypes::s_uint32Type,
        BuiltinTypes::s_uint64Type,
        BuiltinTypes::s_floatType,
        BuiltinTypes::s_doubleType,
        BuiltinTypes::s_boolType,
        BuiltinTypes::s_stringType,
        BuiltinTypes::s_functionType,
        BuiltinTypes::s_arrayType,
        BuiltinTypes::s_mapType,
        s_intType,
        s_uintType,
        s_uintptrType,
        s_intptrType,
        s_vec2iType,
        s_vec2uType,
        s_vec2fType,
        s_vec3iType,
        s_vec3uType,
        s_vec3fType,
        s_vec4iType,
        s_vec4uType,
        s_vec4fType,
        s_nameType,
        s_byteBufferType
    };

    for (SymbolType* type : s_globalVisibleTypes)
    {
        table.AddSymbolType(type->RefCountedPtrFromThis());
    }
}

} // namespace hyperion
