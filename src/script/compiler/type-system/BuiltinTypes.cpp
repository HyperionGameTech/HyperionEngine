#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTupleExpression.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/ast/AstBlockExpression.hpp>

namespace hyperion {
namespace compiler {

const SymbolTypePtr_t BuiltinTypes::UNDEFINED = SymbolType::Primitive(
    "Undefined",
    sp<AstUndefined>(new AstUndefined(SourceLocation::eof))
);

const SymbolTypePtr_t BuiltinTypes::ANY = SymbolType::Primitive(
    "Any",
    sp<AstNil>(new AstNil(SourceLocation::eof))
);

const SymbolTypePtr_t BuiltinTypes::OBJECT = SymbolType::Primitive(
    "Object",
    nullptr,
    nullptr
);

const SymbolTypePtr_t BuiltinTypes::INT = SymbolType::Primitive(
    "Int",
    sp<AstInteger>(new AstInteger(0, SourceLocation::eof))
);

const SymbolTypePtr_t BuiltinTypes::FLOAT = SymbolType::Primitive(
    "Float",
    sp<AstFloat>(new AstFloat(0.0, SourceLocation::eof))
);

const SymbolTypePtr_t BuiltinTypes::NUMBER = SymbolType::Primitive(
    "Number",
    sp<AstInteger>(new AstInteger(0, SourceLocation::eof))
);

const SymbolTypePtr_t BuiltinTypes::BOOLEAN = SymbolType::Primitive(
    "Boolean",
    sp<AstFalse>(new AstFalse(SourceLocation::eof))
);

const SymbolTypePtr_t BuiltinTypes::STRING = SymbolType::Primitive(
    "String",
    sp<AstString>(new AstString("", SourceLocation::eof))
);

const SymbolTypePtr_t BuiltinTypes::FUNCTION = SymbolType::Generic(
    "Function",
    sp<AstFunctionExpression>(new AstFunctionExpression(
        {},
        nullptr, 
        sp<AstBlock>(new AstBlock(SourceLocation::eof)),
        false,
        false,
        false,
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo{ -1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::ARRAY = SymbolType::Generic(
    "Array",
    sp<AstArrayExpression>(new AstArrayExpression(
        {},
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::TUPLE = SymbolType::Generic(
    "Tuple",
    sp<AstTupleExpression>(new AstTupleExpression(
        {},
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo { -1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::VAR_ARGS = SymbolType::Generic(
    "Args",
    sp<AstArrayExpression>(new AstArrayExpression(
        {},
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::NULL_TYPE = SymbolType::Primitive(
    "Null",
    sp<AstNil>(new AstNil(SourceLocation::eof))
);

const SymbolTypePtr_t BuiltinTypes::EVENT_IMPL = SymbolType::Object(
    "Event_impl",
    {
        SymbolMember_t {
            "key",
            BuiltinTypes::STRING,
            BuiltinTypes::STRING->GetDefaultValue()
        },
        SymbolMember_t {
            "trigger",
            BuiltinTypes::FUNCTION,
            BuiltinTypes::FUNCTION->GetDefaultValue()
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::EVENT = SymbolType::Generic(
    "$Event",
    BuiltinTypes::UNDEFINED->GetDefaultValue(),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::EVENT_ARRAY = SymbolType::Generic(
    "$EventArray",
    sp<AstArrayExpression>(new AstArrayExpression(
        {},
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::MODULE_INFO = SymbolType::Object(
    "ModuleInfo",
    {
        SymbolMember_t {
            "id",
            BuiltinTypes::INT,
            BuiltinTypes::INT->GetDefaultValue()
        },
        SymbolMember_t {
            "name",
            BuiltinTypes::STRING,
            BuiltinTypes::STRING->GetDefaultValue()
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::GENERATOR = SymbolType::Generic(
    "Generator",
    sp<AstFunctionExpression>(new AstFunctionExpression(
        {},
        nullptr, 
        sp<AstBlock>(new AstBlock(SourceLocation::eof)),
        false,
        false,
        false,
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo{ 1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::BOXED_TYPE = SymbolType::Generic(
    "Boxed",
    nullptr,
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::MAYBE = SymbolType::Generic(
    "Maybe",
    sp<AstNil>(new AstNil(SourceLocation::eof)),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::BOXED_TYPE
);

const SymbolTypePtr_t BuiltinTypes::CONST_TYPE = SymbolType::Generic(
    "Const",
    nullptr,
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::BOXED_TYPE
);

const SymbolTypePtr_t BuiltinTypes::BLOCK_TYPE = SymbolType::Generic(
    "Block",
    nullptr,
    {},
    GenericTypeInfo { -1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::CLOSURE_TYPE = SymbolType::Generic(
    "Closure",
    nullptr,
    {},
    GenericTypeInfo { -1 },
    BuiltinTypes::FUNCTION
);

} // namespace compiler
} // namespace hyperion
