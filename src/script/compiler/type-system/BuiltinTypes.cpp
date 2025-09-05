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

const SymbolTypeRef BuiltinTypes::PRIMITIVE_TYPE = SymbolTypeRef(new SymbolType(
    "<primitive>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::UNDEFINED = SymbolTypeRef(new SymbolType(
    "<error>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::ANY = SymbolTypeRef(new SymbolType(
    "any",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::PLACEHOLDER = SymbolTypeRef(new SymbolType(
    "<placeholder>",
    TYPE_BUILTIN,
    nullptr,
    nullptr,
    {}, {}));

const SymbolTypeRef BuiltinTypes::VOID_TYPE = SymbolType::Primitive(
    "void",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::OBJECT = SymbolType::Primitive(
    "object",
    nullptr);

const SymbolTypeRef BuiltinTypes::CLASS_TYPE = SymbolType::Extend(
    "Class",
    BuiltinTypes::OBJECT,
    {},
    {});

// Enum type is a generic class type similar to Array<T>.
// e.g. Enum<uint>
const SymbolTypeRef BuiltinTypes::ENUM_TYPE = SymbolType::Generic(
    "Enum",
    {},
    {},
    GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::INT = SymbolType::Primitive(
    "int",
    RC<AstInteger>(new AstInteger(0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::UNSIGNED_INT = SymbolType::Primitive(
    "uint",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::FLOAT = SymbolType::Primitive(
    "float",
    RC<AstFloat>(new AstFloat(0.0, SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::BOOLEAN = SymbolType::Primitive(
    "bool",
    RC<AstFalse>(new AstFalse(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::STRING = SymbolType::Primitive(
    "string",
    RC<AstString>(new AstString("", SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::NULL_TYPE = SymbolType::Primitive(
    "<null>",
    RC<AstNil>(new AstNil(SourceLocation::eof)));

const SymbolTypeRef BuiltinTypes::MODULE_INFO = SymbolType::Object(
    "ModuleInfo",
    BuiltinTypes::OBJECT,
    {
        SymbolTypeMember { "id", BuiltinTypes::INT, BuiltinTypes::INT->GetDefaultValue() },
        SymbolTypeMember { "name", BuiltinTypes::STRING, BuiltinTypes::STRING->GetDefaultValue() },
    },
    {});

const SymbolTypeRef BuiltinTypes::FUNCTION_BASE = SymbolType::Primitive(
    "FunctionBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::FUNCTION = SymbolType::Generic(
    "Function",
    BuiltinTypes::FUNCTION_BASE,
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        {
            { "@return", SymbolType::GenericParameter("ReturnType") },
            { "@args", SymbolType::Generic("ArgTypes", { SymbolTypeMember { "@variadic", BuiltinTypes::ANY } }, {}, GenericInstanceTypeInfo {}) },
        } });

const SymbolTypeRef BuiltinTypes::ARRAY_BASE = SymbolType::Primitive(
    "ArrayBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::ARRAY = SymbolType::Generic(
    "Array",
    BuiltinTypes::ARRAY_BASE,
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "operator[]",
            SymbolType::GenericInstance(
                BuiltinTypes::FUNCTION,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::ANY },
                        { "index", BuiltinTypes::INT } } }) },
        SymbolTypeMember {
            "operator[]=",
            SymbolType::GenericInstance(
                BuiltinTypes::FUNCTION,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("T") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::ANY },
                        { "index", BuiltinTypes::INT },
                        { "value", SymbolType::GenericParameter("T") } } }) } },
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

const SymbolTypeRef BuiltinTypes::MAP_BASE = SymbolType::Primitive(
    "MapBase",
    nullptr);

const SymbolTypeRef BuiltinTypes::MAP = SymbolType::Generic(
    "Map",
    BuiltinTypes::MAP_BASE,
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "operator[]",
            SymbolType::GenericInstance(
                BuiltinTypes::FUNCTION,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("V") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::ANY },
                        { "key", SymbolType::GenericParameter("K") } } }) },
        SymbolTypeMember {
            "operator[]=",
            SymbolType::GenericInstance(
                BuiltinTypes::FUNCTION,
                {}, {},
                GenericInstanceTypeInfo {
                    { { "@return", SymbolType::GenericParameter("V") },
                        { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::ANY },
                        { "key", SymbolType::GenericParameter("K") },
                        { "value", SymbolType::GenericParameter("V") } } }) } },
    Array<SymbolTypeMember> {},
    GenericInstanceTypeInfo {
        { { "key", SymbolType::GenericParameter("K") },
            { "value", SymbolType::GenericParameter("V") } } });

void BuiltinTypes::AddToSymbolTable(IdentifierTable& table)
{
    static const Array<SymbolType*> g_globalVisibleTypes {
        BuiltinTypes::ANY,
        BuiltinTypes::OBJECT,
        BuiltinTypes::CLASS_TYPE,
        BuiltinTypes::ENUM_TYPE,
        BuiltinTypes::VOID_TYPE,
        BuiltinTypes::INT,
        BuiltinTypes::UNSIGNED_INT,
        BuiltinTypes::FLOAT,
        BuiltinTypes::BOOLEAN,
        BuiltinTypes::STRING,
        BuiltinTypes::FUNCTION,
        BuiltinTypes::ARRAY,
        BuiltinTypes::MAP
    };

    for (SymbolType* type : g_globalVisibleTypes)
    {
        table.AddSymbolType(type->RefCountedPtrFromThis());
    }
}

} // namespace hyperion
