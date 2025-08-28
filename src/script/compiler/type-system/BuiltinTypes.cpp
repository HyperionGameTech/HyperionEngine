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

namespace hyperion::compiler {

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
    "Any",
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
    { SymbolTypeMember {
        "base",
        BuiltinTypes::OBJECT,
        RC<AstTypeRef>(new AstTypeRef(
            BuiltinTypes::OBJECT,
            SourceLocation::eof)) } });

// Enum type is a generic class type similar to Array<T>.
// e.g. Enum<uint>
const SymbolTypeRef BuiltinTypes::ENUM_TYPE = SymbolType::Generic(
    "Enum",
    {},
    { SymbolTypeMember {
        "base",
        BuiltinTypes::OBJECT,
        RC<AstTypeRef>(new AstTypeRef(
            BuiltinTypes::OBJECT,
            SourceLocation::eof)) } },
    GenericTypeInfo { 1 },
    BuiltinTypes::OBJECT);

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

const SymbolTypeRef BuiltinTypes::STRING = SymbolType::Extend(
    "string",
    BuiltinTypes::CLASS_TYPE,
    {},
    { SymbolTypeMember {
        "base",
        BuiltinTypes::CLASS_TYPE,
        RC<AstTypeRef>(new AstTypeRef(
            BuiltinTypes::CLASS_TYPE,
            SourceLocation::eof)),
    } });

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

const SymbolTypeRef BuiltinTypes::GENERIC_VARIABLE_TYPE = SymbolType::Generic(
    "<generic>",
    {},
    { SymbolTypeMember {
        "base",
        BuiltinTypes::CLASS_TYPE,
        RC<AstTypeRef>(new AstTypeRef(
            BuiltinTypes::CLASS_TYPE,
            SourceLocation::eof)) } },
    GenericTypeInfo { -1 },
    BuiltinTypes::CLASS_TYPE);

const SymbolTypeRef BuiltinTypes::FUNCTION = SymbolType::Generic(
    "Function",
    Array<SymbolTypeMember> {},
    Array<SymbolTypeMember> {},
    GenericTypeInfo {
        2, /* ReturnType, ParamTypes (variadic) */
        {
            SymbolType::GenericParameter("ReturnType", BuiltinTypes::CLASS_TYPE),
            SymbolType::Generic("varargs", { SymbolTypeMember { "@variadic", BuiltinTypes::PLACEHOLDER } }, {}, GenericTypeInfo { -1 }, BuiltinTypes::PRIMITIVE_TYPE),
        } },
    BuiltinTypes::CLASS_TYPE);

} // namespace hyperion::compiler
