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
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstUndefined.hpp>

namespace hyperion::compiler {

const SymbolTypeTrait BuiltinTypeTraits::variadic = {
    "@variadic"
};

const SymbolTypePtr_t BuiltinTypes::PRIMITIVE_TYPE = SymbolType::Primitive(
    "<primitive-type>",
    nullptr,
    nullptr
);

const SymbolTypePtr_t BuiltinTypes::UNDEFINED = SymbolType::Primitive(
    "<error-type>",
    RC<AstUndefined>(new AstUndefined(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::ANY = SymbolType::Primitive(
    "any",
    RC<AstNil>(new AstNil(SourceLocation::eof)),
    nullptr
);

const SymbolTypePtr_t BuiltinTypes::PLACEHOLDER = SymbolType::Primitive(
    "<placeholder-type>",
    RC<AstNil>(new AstNil(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
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
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "$proto",
            BuiltinTypes::ANY,
            RC<AstNil>(new AstNil(SourceLocation::eof))
        },
        SymbolTypeMember {
            "base",
            BuiltinTypes::OBJECT,
            RC<AstTypeRef>(new AstTypeRef(
                BuiltinTypes::OBJECT,
                SourceLocation::eof
            ))
        }
        /*SymbolTypeMember {
            "name",
            BuiltinTypes::ANY, // Set to any until this is refactored - string relies on class
            RC<AstString>(new AstString("Class", SourceLocation::eof))
        }*/
    }
);

// Enum type is a generic class type similar to Array<T>.
// e.g. Enum<uint>
const SymbolTypePtr_t BuiltinTypes::ENUM_TYPE = SymbolType::Generic(
    "Enum",
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "base",
            BuiltinTypes::OBJECT,
            RC<AstTypeRef>(new AstTypeRef(
                BuiltinTypes::OBJECT,
                SourceLocation::eof
            ))
        }
    },
    GenericTypeInfo { 1 },
    BuiltinTypes::OBJECT
);

const SymbolTypePtr_t BuiltinTypes::INT = SymbolType::Primitive(
    "int",
    RC<AstInteger>(new AstInteger(0, SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::UNSIGNED_INT = SymbolType::Primitive(
    "uint",
    RC<AstUnsignedInteger>(new AstUnsignedInteger(0, SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::FLOAT = SymbolType::Primitive(
    "float",
    RC<AstFloat>(new AstFloat(0.0, SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::BOOLEAN = SymbolType::Primitive(
    "bool",
    RC<AstFalse>(new AstFalse(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::STRING = SymbolType::Extend(
    "String",
    BuiltinTypes::CLASS_TYPE,
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "$proto",
            SymbolType::Primitive(
                "EmptyStringLiteral", nullptr
            ),
            RC<AstString>(new AstString("", SourceLocation::eof))
        },
        SymbolTypeMember {
            "base",
            BuiltinTypes::CLASS_TYPE,
            RC<AstTypeRef>(new AstTypeRef(
                BuiltinTypes::CLASS_TYPE,
                SourceLocation::eof
            )),
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::NULL_TYPE = SymbolType::Primitive(
    "<null-type>",
    RC<AstNil>(new AstNil(SourceLocation::eof)),
    BuiltinTypes::PRIMITIVE_TYPE
);

const SymbolTypePtr_t BuiltinTypes::MODULE_INFO = SymbolType::Object(
    "ModuleInfo",
    {
        SymbolTypeMember {
            "id",
            BuiltinTypes::INT,
            BuiltinTypes::INT->GetDefaultValue()
        },
        SymbolTypeMember {
            "name",
            BuiltinTypes::STRING,
            BuiltinTypes::STRING->GetDefaultValue()
        }
    }
);

const SymbolTypePtr_t BuiltinTypes::GENERIC_VARIABLE_TYPE = SymbolType::Generic(
    "generic",
    Array<SymbolTypeMember> {
        SymbolTypeMember {
            "$proto",
            SymbolType::Primitive(
                "GenericInstance", nullptr
            ),
            nullptr
        },
        SymbolTypeMember {
            "base",
            BuiltinTypes::CLASS_TYPE,
            RC<AstTypeRef>(new AstTypeRef(
                BuiltinTypes::CLASS_TYPE,
                SourceLocation::eof
            ))
        }
    },
    GenericTypeInfo { -1 },
    BuiltinTypes::CLASS_TYPE
);

} // namespace hyperion::compiler
