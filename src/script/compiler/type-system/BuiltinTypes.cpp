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

const SymbolTypeRef BuiltinTypes::s_primitiveType = SymbolTypeRef(new SymbolType(
    "<primitive>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::s_errorType = SymbolTypeRef(new SymbolType(
    "<error>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::s_anyType = SymbolTypeRef(new SymbolType(
    "any",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::s_classType = SymbolTypeRef(new SymbolType(
    "<class>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::s_placeholderType = SymbolTypeRef(new SymbolType(
    "<placeholder>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::s_voidType = SymbolType::Primitive(
    "void",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_objectType = SymbolTypeRef(new SymbolType(
    "object",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::s_functionBaseType = SymbolType::Primitive(
    "FunctionBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::s_arrayBaseType = SymbolType::Primitive(
    "ArrayBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::s_varArgsBaseType = SymbolType::Extend(
    "VarArgsBase",
    BuiltinTypes::s_arrayBaseType,
    {},
    {});

const SymbolTypeRef BuiltinTypes::s_mapBaseType = SymbolType::Primitive(
    "MapBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::s_enumBaseType = SymbolType::Primitive(
    "EnumBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::s_enumType = SymbolType::GenericInstance(
    "Enum",
    BuiltinTypes::s_enumBaseType,
    {},
    {},
    GenericInstanceTypeInfo { { { "underlyingType", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::s_int8Type = SymbolType::Primitive(
    "int8",
    RC<AstInteger>(new AstInteger(0, CBS_8, SourceLocation::eof)),
    CBS_8);

const SymbolTypeRef BuiltinTypes::s_int16Type = SymbolType::Primitive(
    "int16",
    RC<AstInteger>(new AstInteger(0, CBS_16, SourceLocation::eof)),
    CBS_16);

const SymbolTypeRef BuiltinTypes::s_int32Type = SymbolType::Primitive(
    "int32",
    RC<AstInteger>(new AstInteger(0, CBS_32, SourceLocation::eof)),
    CBS_32);

const SymbolTypeRef BuiltinTypes::s_int64Type = SymbolType::Primitive(
    "int64",
    RC<AstInteger>(new AstInteger(0, CBS_64, SourceLocation::eof)),
    CBS_64);

const SymbolTypeRef BuiltinTypes::s_uint8Type = SymbolType::Primitive(
    "uint8",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_8, SourceLocation::eof)),
    CBS_8);

const SymbolTypeRef BuiltinTypes::s_uint16Type = SymbolType::Primitive(
    "uint16",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_16, SourceLocation::eof)),
    CBS_16);

const SymbolTypeRef BuiltinTypes::s_uint32Type = SymbolType::Primitive(
    "uint32",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_32, SourceLocation::eof)),
    CBS_32);

const SymbolTypeRef BuiltinTypes::s_uint64Type = SymbolType::Primitive(
    "uint64",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, CBS_64, SourceLocation::eof)),
    CBS_64);

const SymbolTypeRef BuiltinTypes::s_floatType = SymbolType::Primitive(
    "float",
    RC<AstFloat>(new AstFloat(0.0, CBS_32, SourceLocation::eof)),
    CBS_32);

const SymbolTypeRef BuiltinTypes::s_boolType = SymbolType::Primitive(
    "bool",
    RC<AstFalse>(new AstFalse(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_stringType = SymbolType::Primitive(
    "string",
    RC<AstString>(new AstString("", SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_nullType = SymbolType::Primitive(
    "<null>",
    RC<AstNil>(new AstNil(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_varArgsType = SymbolType::Generic(
    "VarArgs",
    s_varArgsBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "type", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::s_functionType = SymbolType::Generic(
    "Function",
    BuiltinTypes::s_functionBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "@return", SymbolType::GenericParameter("ReturnType") },
            { "@args", SymbolType::GenericInstance(BuiltinTypes::s_varArgsType, {}, {}, GenericInstanceTypeInfo {}) } } });

const SymbolTypeRef BuiltinTypes::s_arrayType = SymbolType::Generic(
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
                        { "index", BuiltinTypes::s_int32Type } } }) },
        SymbolTypeMember {
            "operator[]=",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", SymbolType::Placeholder("SelfType") },
                        { "index", BuiltinTypes::s_int32Type },
                        { "value", SymbolType::GenericParameter("T") } } }) } },
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::s_mapType = SymbolType::Generic(
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
    // do nothing for now
}

void BuiltinTypes::AddToSymbolTable(IdentifierTable& table)
{
    static const SymbolTypeRef s_intType = SymbolType::Alias("int", { BuiltinTypes::s_int32Type });
    static const SymbolTypeRef s_uintType = SymbolType::Alias("uint", { BuiltinTypes::s_uint32Type });

    static SymbolType* const s_globalVisibleTypes[] {
        BuiltinTypes::s_anyType,
        BuiltinTypes::s_objectType,
        BuiltinTypes::s_enumType,
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
        BuiltinTypes::s_boolType,
        BuiltinTypes::s_stringType,
        BuiltinTypes::s_functionType,
        BuiltinTypes::s_arrayType,
        BuiltinTypes::s_mapType,
        s_intType,
        s_uintType
    };

    for (SymbolType* type : s_globalVisibleTypes)
    {
        table.AddSymbolType(type->RefCountedPtrFromThis());
    }
}

} // namespace hyperion
