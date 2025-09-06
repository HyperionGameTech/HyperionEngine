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

const SymbolTypeTrait BuiltinTypeTraits::variadic = {
    "@variadic"
};

const SymbolTypeRef BuiltinTypes::g_primitiveType = SymbolTypeRef(new SymbolType(
    "<primitive>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::g_errorType = SymbolTypeRef(new SymbolType(
    "<error>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::g_anyType = SymbolTypeRef(new SymbolType(
    "any",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::g_classType = SymbolTypeRef(new SymbolType(
    "<class>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::g_placeholderType = SymbolTypeRef(new SymbolType(
    "<placeholder>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::g_voidType = SymbolType::Primitive(
    "void",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::g_objectType = SymbolTypeRef(new SymbolType(
    "object",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

// Enum type is a generic class type similar to Array<T>.
// e.g. Enum<uint>
const SymbolTypeRef BuiltinTypes::g_enumType = SymbolType::Generic(
    "Enum",
    {},
    {},
    GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::g_intType = SymbolType::Primitive(
    "int",
    RC<AstInteger>(new AstInteger(0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::g_unsignedIntType = SymbolType::Primitive(
    "uint",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::g_floatType = SymbolType::Primitive(
    "float",
    RC<AstFloat>(new AstFloat(0.0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::g_boolType = SymbolType::Primitive(
    "bool",
    RC<AstFalse>(new AstFalse(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::g_stringType = SymbolType::Primitive(
    "string",
    RC<AstString>(new AstString("", SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::g_nullType = SymbolType::Primitive(
    "<null>",
    RC<AstNil>(new AstNil(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::g_functionBaseType = SymbolType::Primitive(
    "FunctionBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::g_functionType = SymbolType::Generic(
    "Function",
    BuiltinTypes::g_functionBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        {
            { "@return", SymbolType::GenericParameter("ReturnType") },
            { "@args", SymbolType::Generic("ArgTypes", { SymbolTypeMember { "@variadic", BuiltinTypes::g_anyType } }, {}, GenericInstanceTypeInfo {}) },
        } });

const SymbolTypeRef BuiltinTypes::g_arrayBaseType = SymbolType::Primitive(
    "ArrayBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::g_arrayType = SymbolType::Generic(
    "Array",
    BuiltinTypes::g_arrayBaseType,
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "operator[]",
            SymbolType::GenericInstance(
                BuiltinTypes::g_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::g_anyType },
                        { "index", BuiltinTypes::g_intType } } }) },
        SymbolTypeMember {
            "operator[]=",
            SymbolType::GenericInstance(
                BuiltinTypes::g_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::g_anyType },
                        { "index", BuiltinTypes::g_intType },
                        { "value", SymbolType::GenericParameter("T") } } }) } },
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::g_mapBaseType = SymbolType::Primitive(
    "MapBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::g_mapType = SymbolType::Generic(
    "Map",
    BuiltinTypes::g_mapBaseType,
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "operator[]",
            SymbolType::GenericInstance(
                BuiltinTypes::g_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("V") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::g_anyType },
                        { "key", SymbolType::GenericParameter("K") } } }) },
        SymbolTypeMember {
            "operator[]=",
            SymbolType::GenericInstance(
                BuiltinTypes::g_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("V") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::g_anyType },
                        { "key", SymbolType::GenericParameter("K") },
                        { "value", SymbolType::GenericParameter("V") } } }) } },
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "key", SymbolType::GenericParameter("K") },
            { "value", SymbolType::GenericParameter("V") } } });

void BuiltinTypes::AddToSymbolTable(IdentifierTable& table)
{
    static SymbolType* const g_globalVisibleTypes[] {
        BuiltinTypes::g_anyType,
        BuiltinTypes::g_objectType,
        BuiltinTypes::g_enumType,
        BuiltinTypes::g_voidType,
        BuiltinTypes::g_intType,
        BuiltinTypes::g_unsignedIntType,
        BuiltinTypes::g_floatType,
        BuiltinTypes::g_boolType,
        BuiltinTypes::g_stringType,
        BuiltinTypes::g_functionType,
        BuiltinTypes::g_arrayType,
        BuiltinTypes::g_mapType
    };

    for (SymbolType* type : g_globalVisibleTypes)
    {
        table.AddSymbolType(type->RefCountedPtrFromThis());
    }
}

} // namespace hyperion
