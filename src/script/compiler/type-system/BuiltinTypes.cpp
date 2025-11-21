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

#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/IdentifierTable.hpp>

namespace hyperion {

const SymbolType* BuiltinTypes::s_primitiveType = new SymbolType(
    "<primitive>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {});

const SymbolType* BuiltinTypes::s_errorType = new SymbolType(
    "<error>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {});

const SymbolType* BuiltinTypes::s_anyType = new SymbolType(
    "any",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {});

const SymbolType* BuiltinTypes::s_classType = new SymbolType(
    "<class>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {});

const SymbolType* BuiltinTypes::s_placeholderType = new SymbolType(
    "<placeholder>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {});

const SymbolType* BuiltinTypes::s_voidType = SymbolType::Primitive(
    "void",
    RC<AstUndefined>(new AstUndefined(SourceLocation::Eof())));

const SymbolType* BuiltinTypes::s_objectType = new SymbolType(
    "object",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {});

const SymbolType* BuiltinTypes::s_functionBaseType = SymbolType::Primitive(
    "FunctionBase",
    nullptr);

const SymbolType* BuiltinTypes::s_arrayBaseType = SymbolType::Primitive(
    "ArrayBase",
    nullptr);

const SymbolType* BuiltinTypes::s_varArgsBaseType = SymbolType::Extend(
    "VarArgsBase",
    BuiltinTypes::s_arrayBaseType,
    {},
    {});

const SymbolType* BuiltinTypes::s_mapBaseType = SymbolType::Primitive(
    "MapBase",
    nullptr);

const SymbolType* BuiltinTypes::s_int8Type = SymbolType::Primitive(
    "int8",
    RC<AstInteger>(new AstInteger(0, CBS_8, SourceLocation::Eof())),
    CBS_8);

const SymbolType* BuiltinTypes::s_int16Type = SymbolType::Primitive(
    "int16",
    RC<AstInteger>(new AstInteger(0, CBS_16, SourceLocation::Eof())),
    CBS_16);

const SymbolType* BuiltinTypes::s_int32Type = SymbolType::Primitive(
    "int32",
    RC<AstInteger>(new AstInteger(0, CBS_32, SourceLocation::Eof())),
    CBS_32);

const SymbolType* BuiltinTypes::s_int64Type = SymbolType::Primitive(
    "int64",
    RC<AstInteger>(new AstInteger(0, CBS_64, SourceLocation::Eof())),
    CBS_64);

const SymbolType* BuiltinTypes::s_uint8Type = SymbolType::Primitive(
    "uint8",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_8, SourceLocation::Eof())),
    CBS_8);

const SymbolType* BuiltinTypes::s_uint16Type = SymbolType::Primitive(
    "uint16",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_16, SourceLocation::Eof())),
    CBS_16);

const SymbolType* BuiltinTypes::s_uint32Type = SymbolType::Primitive(
    "uint32",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_32, SourceLocation::Eof())),
    CBS_32);

const SymbolType* BuiltinTypes::s_uint64Type = SymbolType::Primitive(
    "uint64",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_64, SourceLocation::Eof())),
    CBS_64);

const SymbolType* BuiltinTypes::s_floatType = SymbolType::Primitive(
    "float",
    RC<AstFloat>(new AstFloat(0.0, CBS_32, SourceLocation::Eof())),
    CBS_32);

const SymbolType* BuiltinTypes::s_doubleType = SymbolType::Primitive(
    "double",
    RC<AstFloat>(new AstFloat(0.0, CBS_64, SourceLocation::Eof())),
    CBS_64);

const SymbolType* BuiltinTypes::s_boolType = SymbolType::Primitive(
    "bool",
    RC<AstFalse>(new AstFalse(SourceLocation::Eof())));

const SymbolType* BuiltinTypes::s_stringType = SymbolType::Primitive(
    "string",
    RC<AstString>(new AstString("", SourceLocation::Eof())));

const SymbolType* BuiltinTypes::s_nullType = SymbolType::Primitive(
    "<null>",
    RC<AstNil>(new AstNil(SourceLocation::Eof())));

const SymbolType* BuiltinTypes::s_varArgsType = SymbolType::Generic(
    "VarArgs",
    s_varArgsBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "type", SymbolType::GenericParameter("T") } } });

const SymbolType* BuiltinTypes::s_functionType = SymbolType::Generic(
    "Function",
    BuiltinTypes::s_functionBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "@return", SymbolType::GenericParameter("ReturnType") },
            { "@args", SymbolType::GenericInstance(BuiltinTypes::s_varArgsType, {}, {}, GenericInstanceTypeInfo {}) } } });

// See ScriptArrayWrapper.cpp in Hyperion Engine for implementation of array methods
const SymbolType* BuiltinTypes::s_arrayType = SymbolType::Generic(
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

const SymbolType* BuiltinTypes::s_mapType = SymbolType::Generic(
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

void BuiltinTypes::Initialize(CompilationUnit* globalCompilationUnit)
{
    Assert(globalCompilationUnit != nullptr);
#pragma region String
    // HAX - we need to cast away const-ness here because we want to add members to the string type
    SymbolType* stringTypeNonConst = const_cast<SymbolType*>(BuiltinTypes::s_stringType);

    stringTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "operator+",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                { { "@return", BuiltinTypes::s_stringType },
                    { "self", BuiltinTypes::s_stringType },
                    { "other", BuiltinTypes::s_stringType } } }) });

    stringTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "Length",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                { { "@return", BuiltinTypes::s_uint64Type },
                    { "self", BuiltinTypes::s_stringType } } }) });

    stringTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Join",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                { { "@return", BuiltinTypes::s_stringType },
                    { "elems", SymbolType::GenericInstance(BuiltinTypes::s_arrayType, {}, {}, GenericInstanceTypeInfo { { { "type", BuiltinTypes::s_anyType } } }) },
                    { "sep", BuiltinTypes::s_stringType } } }) });

    stringTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "$invoke",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                { { "@return", BuiltinTypes::s_stringType },
                    { "val", BuiltinTypes::s_anyType } } }) });
#pragma endregion String

#define REGISTER_GLOBAL_TYPE(type)                                      \
    do                                                                  \
    {                                                                   \
        const_cast<SymbolType*>(type)->Register(globalCompilationUnit); \
    }                                                                   \
    while (0)

    // Register static types in global compilation unit
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_primitiveType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_errorType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_anyType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_classType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_placeholderType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_voidType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_objectType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_functionBaseType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_arrayBaseType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_varArgsBaseType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_mapBaseType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_int8Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_int16Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_int32Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_int64Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_uint8Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_uint16Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_uint32Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_uint64Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_floatType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_doubleType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_boolType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_stringType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_nullType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_varArgsType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_functionType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_arrayType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_mapType);

#undef REGISTER_GLOBAL_TYPE
}

void BuiltinTypes::RegisterTypes(CompilationUnit* compilationUnit)
{
    Assert(compilationUnit != nullptr);

    IdentifierTable& table = compilationUnit->GetGlobalModule()->scopeTree.Top().identifierTable;

    // Create new types per-compilation unit:

    SymbolType* intType = SymbolType::Alias("int", { BuiltinTypes::s_int32Type });
    intType->Register(compilationUnit);

    SymbolType* uintType = SymbolType::Alias("uint", { BuiltinTypes::s_uint32Type });
    uintType->Register(compilationUnit);

    SymbolType* uintptrType = SymbolType::Alias("UIntPtr", { sizeof(void*) == 4 ? BuiltinTypes::s_uint32Type : BuiltinTypes::s_uint64Type });
    uintptrType->Register(compilationUnit);

    SymbolType* intptrType = SymbolType::Alias("IntPtr", { sizeof(void*) == 4 ? BuiltinTypes::s_int32Type : BuiltinTypes::s_int64Type });
    intptrType->Register(compilationUnit);

    SymbolType* vec2iType = SymbolType::Object(
        "Vec2i", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) } },
        {});

    vec2iType->Register(compilationUnit);

    SymbolType* vec2uType = SymbolType::Object(
        "Vec2u", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) } },
        {});

    vec2uType->Register(compilationUnit);

    SymbolType* vec2fType = SymbolType::Object(
        "Vec2f", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_floatType) } },
        {});

    vec2fType->Register(compilationUnit);

    SymbolType* vec3iType = SymbolType::Object(
        "Vec3i", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) } },
        {});

    vec3iType->Register(compilationUnit);

    SymbolType* vec3uType = SymbolType::Object(
        "Vec3u", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) } },
        {});

    vec3uType->Register(compilationUnit);

    SymbolType* vec3fType = SymbolType::Object(
        "Vec3f", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_floatType) } },
        {});

    vec3fType->Register(compilationUnit);

    SymbolType* vec4iType = SymbolType::Object(
        "Vec4i", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "w", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) } },
        {});

    vec4iType->Register(compilationUnit);

    SymbolType* vec4uType = SymbolType::Object(
        "Vec4u", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "w", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) } },
        {});

    vec4uType->Register(compilationUnit);

    SymbolType* vec4fType = SymbolType::Object(
        "Vec4f", nullptr,
        { SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "w", const_cast<SymbolType*>(BuiltinTypes::s_floatType) } },
        {});

    vec4fType->Register(compilationUnit);

    SymbolType* nameType = SymbolType::Primitive("Name", nullptr);
    nameType->Register(compilationUnit);

    SymbolType* byteBufferType = SymbolType::Object("ByteBuffer", nullptr, {}, {});
    byteBufferType->Register(compilationUnit);

    const SymbolType* const s_globalVisibleTypes[] {
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
        intType,
        uintType,
        uintptrType,
        intptrType,
        vec2iType,
        vec2uType,
        vec2fType,
        vec3iType,
        vec3uType,
        vec3fType,
        vec4iType,
        vec4uType,
        vec4fType,
        nameType,
        byteBufferType
    };

    for (const SymbolType* type : s_globalVisibleTypes)
    {
        table.AddSymbolType(const_cast<SymbolType*>(type));
    }
}

} // namespace hyperion
