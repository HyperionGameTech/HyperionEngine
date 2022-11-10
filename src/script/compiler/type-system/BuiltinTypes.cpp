#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTupleExpression.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/ast/AstBlockExpression.hpp>

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
    sp<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::ANY_TYPE = SymbolType::Primitive(
    "__any",
    sp<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::ANY = SymbolType::Primitive(
    "any",
    sp<AstNil>(new AstNil(SourceLocation::eof)),
    BuiltinTypes::ANY_TYPE
);

const SymbolTypePtr_t BuiltinTypes::VOID_TYPE = SymbolType::Primitive(
    "void",
    sp<AstUndefined>(new AstUndefined(SourceLocation::eof)),
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
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            BuiltinTypes::ANY,
            nullptr
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::OBJECT,
            sp<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::OBJECT,
                nullptr,
                SourceLocation::eof
            )),
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::ENUM_TYPE = SymbolType::Primitive(
    "Enum",
    sp<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::INT = SymbolType::Extend(
    "Int",
    BuiltinTypes::CLASS_TYPE,
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__Int", nullptr
            ),
            sp<AstInteger>(new AstInteger(0, SourceLocation::eof))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            sp<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::UNSIGNED_INT = SymbolType::Extend(
    "UInt",
    BuiltinTypes::CLASS_TYPE,
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__UInt", nullptr
            ),
            sp<AstUnsignedInteger>(new AstUnsignedInteger(0, SourceLocation::eof))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            sp<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::FLOAT = SymbolType::Extend(
    "Float",
    BuiltinTypes::CLASS_TYPE,
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__Float", nullptr
            ),
            sp<AstFloat>(new AstFloat(0.0, SourceLocation::eof))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            sp<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::NUMBER = SymbolType::Extend(
    "Number",
    BuiltinTypes::CLASS_TYPE,
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__Number", nullptr
            ),
            sp<AstFloat>(new AstFloat(0.0, SourceLocation::eof))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            sp<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    }
);


// const SymbolTypePtr_t BuiltinTypes::BOOLEAN = SymbolType::Primitive(
//     "Boolean",
//     sp<AstFalse>(new AstFalse(SourceLocation::eof))
// );

const SymbolTypePtr_t BuiltinTypes::BOOLEAN = SymbolType::Extend(
    "Bool",
    BuiltinTypes::CLASS_TYPE,
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__Bool", nullptr
            ),
            sp<AstFalse>(new AstFalse(SourceLocation::eof))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            sp<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    }
);

// const SymbolTypePtr_t BuiltinTypes::STRING = SymbolType::Primitive(
//     "String",
//     sp<AstString>(new AstString("", SourceLocation::eof))
// );


const SymbolTypePtr_t BuiltinTypes::STRING = SymbolType::Extend(
    "String",
    BuiltinTypes::CLASS_TYPE,
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__String", nullptr
            ),
            sp<AstString>(new AstString("", SourceLocation::eof))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            sp<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::FUNCTION = SymbolType::Generic(
    "Function",
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "FunctionInstance", nullptr
            ),
            sp<AstFunctionExpression>(new AstFunctionExpression(
                {},
                nullptr,
                sp<AstBlock>(new AstBlock(SourceLocation::eof)),
                false,
                false,
                false,
                SourceLocation::eof
            ))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            sp<AstTypeObject>(new AstTypeObject(
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
    std::vector<SymbolMember_t> {
        SymbolMember_t {
            "$proto",
            SymbolType::Primitive(
                "__Array", nullptr
            ),
            sp<AstArrayExpression>(new AstArrayExpression(
                {}, SourceLocation::eof
            ))
        },
        SymbolMember_t {
            "base",
            BuiltinTypes::CLASS_TYPE,
            sp<AstTypeObject>(new AstTypeObject(
                BuiltinTypes::CLASS_TYPE,
                nullptr,
                SourceLocation::eof
            )),
        }
    },
    GenericTypeInfo { 1 },
    BuiltinTypes::CLASS_TYPE
);

const SymbolTypePtr_t BuiltinTypes::TUPLE = SymbolType::Generic(
    "tuple",
    sp<AstTupleExpression>(new AstTupleExpression(
        {},
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo { -1 },
    BuiltinTypes::CLASS_TYPE
);

const SymbolTypePtr_t BuiltinTypes::VAR_ARGS = SymbolType::Generic(
    "Args",
    sp<AstArrayExpression>(new AstArrayExpression(
        {},
        SourceLocation::eof
    )),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::CLASS_TYPE
);

const SymbolTypePtr_t BuiltinTypes::NULL_TYPE = SymbolType::Primitive(
    "Null",
    sp<AstNil>(new AstNil(SourceLocation::eof)),
    BuiltinTypes::CLASS_TYPE
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
    BuiltinTypes::CLASS_TYPE
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
    sp<AstNil>(new AstNil(SourceLocation::eof)),
    {},
    GenericTypeInfo { 1 },
    BuiltinTypes::BOXED_TYPE
);

const SymbolTypePtr_t BuiltinTypes::CONST_TYPE_TYPE = SymbolType::Primitive(
    "ConstType",
    sp<AstUndefined>(new AstUndefined(SourceLocation::eof)),
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
    std::vector<SymbolMember_t> {
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
            sp<AstTypeObject>(new AstTypeObject(
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
