#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstUndefined.hpp>

namespace hyperion::compiler {

const SymbolTypePtr_t BuiltinTypes::PRIMITIVE_TYPE = SymbolType::Primitive(
    "Primitive",
    nullptr,
    nullptr
);

const SymbolTypePtr_t BuiltinTypes::TRAIT_TYPE = SymbolType::Primitive(
    "Trait",
    nullptr,
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::UNDEFINED = SymbolType::Primitive(
    "<undef>",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::ANY_TYPE = SymbolType::Primitive(
    "__any",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::ANY = SymbolType::Primitive(
    "Any",
    RC<AstNil>(new AstNil(SourceLocation::eof)),
    BuiltinTypes::ANY_TYPE
);

const SymbolTypePtr_t BuiltinTypes::VOID_TYPE = SymbolType::Primitive(
    "void",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::OBJECT = SymbolType::Primitive(
    "Object",
    nullptr,
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::CLASS_TYPE = SymbolType::Extend(
    "Class",
    BuiltinTypes::OBJECT,
    Array<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            BuiltinTypes::ANY,
            nullptr
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::OBJECT,
            RC<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::OBJECT,
                nullptr,
                SourceLocation::eof
            )),
        }
        // SymbolMember_t {
        //     "name",
        //     BuiltinTypes::STRING,
        //     RC<AstString>(new AstString("Class", SourceLocation::eof))
        // },
    }
);

const SymbolTypePtr_t BuiltinTypes::ENUM_TYPE = SymbolType::Primitive(
    "Enum",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

/*
const SymbolTypePtr_t BuiltinTypes::ENUM_TYPE = SymbolType::Generic(
    "Enum",
    Array<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__Enum", nullptr
            ),
            RC<AstUndefined>(new AstUndefined(SourceLocation::eof))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            RC<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    },
    GenericTypeInfo { 1 },
    BuiltinTypes::CLASS_TYPE
);*/

const SymbolTypePtr_t BuiltinTypes::INT = SymbolType::Primitive(
    "Int",
    RC<AstInteger>(new AstInteger(0, SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::UNSIGNED_INT = SymbolType::Primitive(
    "UInt",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::FLOAT = SymbolType::Primitive(
    "Float",
    RC<AstFloat>(new AstFloat(0.0, SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::NUMBER = SymbolType::Primitive(
    "Number",
    RC<AstFloat>(new AstFloat(0.0, SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::BOOLEAN = SymbolType::Primitive(
    "Bool",
    RC<AstFalse>(new AstFalse(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::STRING = SymbolType::Extend(
    "String",
    BuiltinTypes::CLASS_TYPE,
    Array<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__String", nullptr
            ),
            RC<AstString>(new AstString("", SourceLocation::eof))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            RC<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::FUNCTION = SymbolType::Generic(
    "Function",
    Array<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "FunctionInstance", nullptr
            ),
            RC<AstFunctionExpression>(new AstFunctionExpression(
                {},
                nullptr,
                RC<AstBlock>(new AstBlock(SourceLocation::eof)),
                SourceLocation::eof
            ))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            RC<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    },
    GenericTypeInfo{ -1 },
    BuiltinTypes::CLASS_TYPE
);

const SymbolTypePtr_t BuiltinTypes::ARRAY = SymbolType::Generic(
    "Array",
    Array<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__Array", nullptr
            ),
            RC<AstArrayExpression>(new AstArrayExpression(
                {}, SourceLocation::eof
            ))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            RC<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    },
    GenericTypeInfo { 1 },
    BuiltinTypes::CLASS_TYPE
);

const SymbolTypePtr_t BuiltinTypes::VAR_ARGS = SymbolType::Generic(
    "Args",
    RC<AstArrayExpression>(new AstArrayExpression(
        {},
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::CLASS_TYPE
);

const SymbolTypePtr_t BuiltinTypes::NULL_TYPE = SymbolType::Primitive(
    "NullType",
    RC<AstNil>(new AstNil(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
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
    RC<AstFunctionExpression>(new AstFunctionExpression(
        {},
        nullptr, 
        RC<AstBlock>(new AstBlock(SourceLocation::eof)),
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo{ 1 },
    BuiltinTypes::CLASS_TYPE
);

const SymbolTypePtr_t BuiltinTypes::BOXED_TYPE = SymbolType::Generic(
    "Boxed",
    nullptr,
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::TRAIT_TYPE
);

const SymbolTypePtr_t BuiltinTypes::MAYBE = SymbolType::Generic(
    "Maybe",
    RC<AstNil>(new AstNil(SourceLocation::eof)),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::BOXED_TYPE
);

const SymbolTypePtr_t BuiltinTypes::CONST_TYPE_TYPE = SymbolType::Primitive(
    "ConstType",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::CONST_TYPE = SymbolType::Generic(
    "Const",
    nullptr,
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::CONST_TYPE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::BLOCK_TYPE = SymbolType::Generic(
    "Block",
    nullptr,
    {},
    GenericTypeInfo { -1 },
    BuiltinTypes::CLASS_TYPE
);

const SymbolTypePtr_t BuiltinTypes::CLOSURE_TYPE = SymbolType::Generic(
    "Closure",
    {},
    GenericTypeInfo { -1 },
    BuiltinTypes::FUNCTION
);

const SymbolTypePtr_t BuiltinTypes::GENERIC_VARIABLE_TYPE = SymbolType::Generic(
    "Generic",
    Array<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "GenericInstance", nullptr
            ),
            nullptr
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            RC<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    },
    GenericTypeInfo { -1 },
    BuiltinTypes::CLASS_TYPE
);

} // namespace hyperion::compiler
