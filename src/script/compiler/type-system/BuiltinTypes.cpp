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

const SymbolTypeRef BuiltinTypes::s_varArgsBaseType = SymbolType::Primitive(
    "VarArgsBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::s_varArgsType = SymbolType::Generic(
    "VarArgs",
    s_varArgsBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "type", SymbolType::GenericParameter("T") } } });

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

// Enum type is a generic class type similar to Array<T>.
// e.g. Enum<uint>
const SymbolTypeRef BuiltinTypes::s_enumType = SymbolType::Generic(
    "Enum",
    {},
    {},
    GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::s_intType = SymbolType::Primitive(
    "int",
    RC<AstInteger>(new AstInteger(0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_unsignedIntType = SymbolType::Primitive(
    "uint",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_floatType = SymbolType::Primitive(
    "float",
    RC<AstFloat>(new AstFloat(0.0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_boolType = SymbolType::Primitive(
    "bool",
    RC<AstFalse>(new AstFalse(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_stringType = SymbolType::Primitive(
    "string",
    RC<AstString>(new AstString("", SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_nullType = SymbolType::Primitive(
    "<null>",
    RC<AstNil>(new AstNil(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::s_functionBaseType = SymbolType::Primitive(
    "FunctionBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::s_functionType = SymbolType::Generic(
    "Function",
    BuiltinTypes::s_functionBaseType,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "@return", SymbolType::GenericParameter("ReturnType") },
            { "@args", SymbolType::GenericInstance(BuiltinTypes::s_varArgsType, {}, {}, GenericInstanceTypeInfo {}) } } });

const SymbolTypeRef BuiltinTypes::s_arrayBaseType = SymbolType::Primitive(
    "ArrayBase",
    nullptr);

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
                        { "index", BuiltinTypes::s_intType } } }) },
        SymbolTypeMember {
            "operator[]=",
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", SymbolType::Placeholder("SelfType") },
                        { "index", BuiltinTypes::s_intType },
                        { "value", SymbolType::GenericParameter("T") } } }) } },
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::s_mapBaseType = SymbolType::Primitive(
    "MapBase",
    nullptr);

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

void BuiltinTypes::AddToSymbolTable(IdentifierTable& table)
{
    static SymbolType* const s_globalVisibleTypes[] {
        BuiltinTypes::s_anyType,
        BuiltinTypes::s_objectType,
        BuiltinTypes::s_enumType,
        BuiltinTypes::s_voidType,
        BuiltinTypes::s_intType,
        BuiltinTypes::s_unsignedIntType,
        BuiltinTypes::s_floatType,
        BuiltinTypes::s_boolType,
        BuiltinTypes::s_stringType,
        BuiltinTypes::s_functionType,
        BuiltinTypes::s_arrayType,
        BuiltinTypes::s_mapType
    };

    for (SymbolType* type : s_globalVisibleTypes)
    {
        table.AddSymbolType(type->RefCountedPtrFromThis());
    }
}

} // namespace hyperion
