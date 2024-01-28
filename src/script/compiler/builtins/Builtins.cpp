#include <script/compiler/builtins/Builtins.hpp>
#include <script/SourceFile.hpp>
#include <script/SourceStream.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/SourceLocation.hpp>

#include <core/lib/DynArray.hpp>

namespace hyperion::compiler {

const SourceLocation Builtins::BUILTIN_SOURCE_LOCATION(-1, -1, "<builtin>");

Builtins::Builtins(CompilationUnit *unit)
    : m_unit(unit)
{
#if 0
    m_vars.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
        "array",
        nullptr,
        RC<AstTemplateExpression>(new AstTemplateExpression(
            RC<AstTypeExpression>(new AstTypeExpression(
                "array",
                nullptr,
                {},
                {
                },
                {
                    // RC<AstVariableDeclaration>(new AstVariableDeclaration(
                    //     "$invoke",
                    //     nullptr,
                    //     RC<AstFunctionExpression>(new AstFunctionExpression(
                    //         {
                    //             RC<AstParameter>(new AstParameter(
                    //                 "self",
                    //                 RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                    //                     RC<AstTypeRef>(new AstTypeRef(
                    //                         BuiltinTypes::CLASS_TYPE,
                    //                         BUILTIN_SOURCE_LOCATION
                    //                     )),
                    //                     BUILTIN_SOURCE_LOCATION
                    //                 )),
                    //                 nullptr,
                    //                 false,
                    //                 false,
                    //                 false,
                    //                 BUILTIN_SOURCE_LOCATION
                    //             )),
                    //         },
                    //         nullptr,
                    //         RC<AstBlock>(new AstBlock(
                    //             {
                    //                 RC<AstReturnStatement>(new AstReturnStatement(
                    //                     RC<AstArrayExpression>(new AstArrayExpression(
                    //                         {},
                    //                         BUILTIN_SOURCE_LOCATION
                    //                     )),
                    //                     BUILTIN_SOURCE_LOCATION
                    //                 ))
                    //             },
                    //             BUILTIN_SOURCE_LOCATION
                    //         )),
                    //         BUILTIN_SOURCE_LOCATION
                    //     )),
                    //     IdentifierFlags::FLAG_CONST,
                    //     BUILTIN_SOURCE_LOCATION
                    // ))

                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        "length",
                        nullptr,
                        RC<AstFunctionExpression>(new AstFunctionExpression(
                            {
                                RC<AstParameter>(new AstParameter(
                                    "self",
                                    nullptr,
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                )),
                            },
                            nullptr,
                            RC<AstBlock>(new AstBlock(
                                {
                                    RC<AstReturnStatement>(new AstReturnStatement(
                                        RC<AstCallExpression>(new AstCallExpression(
                                            RC<AstVariable>(new AstVariable(
                                                "__array_size",
                                                BUILTIN_SOURCE_LOCATION
                                            )),
                                            {
                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "self",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                ))
                                            },
                                            false,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    ))
                                },
                                BUILTIN_SOURCE_LOCATION
                            )),
                            BUILTIN_SOURCE_LOCATION
                        )),
                        IdentifierFlags::FLAG_CONST,
                        BUILTIN_SOURCE_LOCATION
                    )),

                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        "push",
                        nullptr,
                        RC<AstFunctionExpression>(new AstFunctionExpression(
                            {
                                RC<AstParameter>(new AstParameter(
                                    "self",
                                    nullptr,
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                )),

                                RC<AstParameter>(new AstParameter(
                                    "value",
                                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                        RC<AstVariable>(new AstVariable(
                                            "T",
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    )),
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                ))
                            },
                            nullptr,
                            RC<AstBlock>(new AstBlock(
                                {
                                    RC<AstReturnStatement>(new AstReturnStatement(
                                        RC<AstCallExpression>(new AstCallExpression(
                                            RC<AstVariable>(new AstVariable(
                                                "__array_push",
                                                BUILTIN_SOURCE_LOCATION
                                            )),
                                            {
                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "self",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                )),

                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "value",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                ))
                                            },
                                            false,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    ))
                                },
                                BUILTIN_SOURCE_LOCATION
                            )),
                            BUILTIN_SOURCE_LOCATION
                        )),
                        IdentifierFlags::FLAG_CONST,
                        BUILTIN_SOURCE_LOCATION
                    )),

                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        "pop",
                        nullptr,
                        RC<AstFunctionExpression>(new AstFunctionExpression(
                            {
                                RC<AstParameter>(new AstParameter(
                                    "self",
                                    nullptr,
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                ))
                            },
                            nullptr,
                            RC<AstBlock>(new AstBlock(
                                {
                                    RC<AstReturnStatement>(new AstReturnStatement(
                                        RC<AstCallExpression>(new AstCallExpression(
                                            RC<AstVariable>(new AstVariable(
                                                "__array_pop",
                                                BUILTIN_SOURCE_LOCATION
                                            )),
                                            {
                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "self",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                ))
                                            },
                                            false,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    ))
                                },
                                BUILTIN_SOURCE_LOCATION
                            )),
                            BUILTIN_SOURCE_LOCATION
                        )),
                        IdentifierFlags::FLAG_CONST,
                        BUILTIN_SOURCE_LOCATION
                    )),

                    // getter
                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        "operator[]",
                        nullptr,
                        RC<AstFunctionExpression>(new AstFunctionExpression(
                            {
                                RC<AstParameter>(new AstParameter(
                                    "self",
                                    nullptr,
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                )),

                                RC<AstParameter>(new AstParameter(
                                    "index",
                                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                        RC<AstTypeRef>(new AstTypeRef(
                                            BuiltinTypes::INT,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    )),
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                ))
                            },
                            // Return `T`
                            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                RC<AstVariable>(new AstVariable(
                                    "T",
                                    BUILTIN_SOURCE_LOCATION
                                )),
                                BUILTIN_SOURCE_LOCATION
                            )),
                            RC<AstBlock>(new AstBlock(
                                {
                                    RC<AstReturnStatement>(new AstReturnStatement(
                                        RC<AstCallExpression>(new AstCallExpression(
                                            RC<AstVariable>(new AstVariable(
                                                "__array_get",
                                                BUILTIN_SOURCE_LOCATION
                                            )),
                                            {
                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "self",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                )),

                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "index",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                ))
                                            },
                                            false,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    ))
                                },
                                BUILTIN_SOURCE_LOCATION
                            )),
                            BUILTIN_SOURCE_LOCATION
                        )),
                        IdentifierFlags::FLAG_CONST,
                        BUILTIN_SOURCE_LOCATION
                    )),

                    // setter
                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        "operator[]=",
                        nullptr,
                        RC<AstFunctionExpression>(new AstFunctionExpression(
                            {
                                RC<AstParameter>(new AstParameter(
                                    "self",
                                    nullptr,
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                )),

                                RC<AstParameter>(new AstParameter(
                                    "index",
                                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                        RC<AstTypeRef>(new AstTypeRef(
                                            BuiltinTypes::INT,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    )),
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                )),

                                RC<AstParameter>(new AstParameter(
                                    "value",
                                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                        RC<AstVariable>(new AstVariable(
                                            "T",
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    )),
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                ))
                            },
                            // Return `T` (the value that was set)
                            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                RC<AstVariable>(new AstVariable(
                                    "T",
                                    BUILTIN_SOURCE_LOCATION
                                )),
                                BUILTIN_SOURCE_LOCATION
                            )),
                            RC<AstBlock>(new AstBlock(
                                {
                                    RC<AstReturnStatement>(new AstReturnStatement(
                                        RC<AstCallExpression>(new AstCallExpression(
                                            RC<AstVariable>(new AstVariable(
                                                "__array_set",
                                                BUILTIN_SOURCE_LOCATION
                                            )),
                                            {
                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "self",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                )),

                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "index",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                )),

                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "value",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                ))
                                            },
                                            false,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    ))
                                },
                                BUILTIN_SOURCE_LOCATION
                            )),
                            BUILTIN_SOURCE_LOCATION
                        )),
                        IdentifierFlags::FLAG_CONST,
                        BUILTIN_SOURCE_LOCATION
                    ))
                },
                true, // proxy class, so we can use Array<T>.length() like [].length()
                BUILTIN_SOURCE_LOCATION
            )),
            {
                RC<AstParameter>(new AstParameter(
                    "T",
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            BuiltinTypes::CLASS_TYPE,
                            BUILTIN_SOURCE_LOCATION
                        )),
                        BUILTIN_SOURCE_LOCATION
                    )),
                    nullptr,
                    false,
                    false,
                    false,
                    BUILTIN_SOURCE_LOCATION
                ))
            },
            nullptr,
            BUILTIN_SOURCE_LOCATION
        )),
        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC,
        BUILTIN_SOURCE_LOCATION
    )));

    m_vars.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
        "map",
        nullptr,
        RC<AstTemplateExpression>(new AstTemplateExpression(
            RC<AstTypeExpression>(new AstTypeExpression(
                "map",
                nullptr,
                {},
                {
                },
                {
                    // members

                    // getter
                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        "operator[]",
                        nullptr,
                        RC<AstFunctionExpression>(new AstFunctionExpression(
                            {
                                RC<AstParameter>(new AstParameter(
                                    "self",
                                    nullptr,
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                )),

                                RC<AstParameter>(new AstParameter(
                                    "key",
                                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                        RC<AstVariable>(new AstVariable(
                                            "K",
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    )),
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                ))
                            },
                            // Return `V`
                            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                RC<AstVariable>(new AstVariable(
                                    "V",
                                    BUILTIN_SOURCE_LOCATION
                                )),
                                BUILTIN_SOURCE_LOCATION
                            )),
                            RC<AstBlock>(new AstBlock(
                                {
                                    RC<AstReturnStatement>(new AstReturnStatement(
                                        RC<AstCallExpression>(new AstCallExpression(
                                            RC<AstVariable>(new AstVariable(
                                                "__map_get",
                                                BUILTIN_SOURCE_LOCATION
                                            )),
                                            {
                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "self",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                )),

                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "key",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                ))
                                            },
                                            false,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    ))
                                },
                                BUILTIN_SOURCE_LOCATION
                            )),
                            BUILTIN_SOURCE_LOCATION
                        )),
                        IdentifierFlags::FLAG_CONST,
                        BUILTIN_SOURCE_LOCATION
                    )),

                    // setter
                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        "operator[]=",
                        nullptr,
                        RC<AstFunctionExpression>(new AstFunctionExpression(
                            {
                                RC<AstParameter>(new AstParameter(
                                    "self",
                                    nullptr,
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                )),

                                RC<AstParameter>(new AstParameter(
                                    "key",
                                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                        RC<AstVariable>(new AstVariable(
                                            "K",
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    )),
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                )),

                                RC<AstParameter>(new AstParameter(
                                    "value",
                                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                        RC<AstVariable>(new AstVariable(
                                            "V",
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    )),
                                    nullptr,
                                    false,
                                    false,
                                    false,
                                    BUILTIN_SOURCE_LOCATION
                                ))
                            },
                            // Return `V` (the value that was set)
                            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                RC<AstVariable>(new AstVariable(
                                    "V",
                                    BUILTIN_SOURCE_LOCATION
                                )),
                                BUILTIN_SOURCE_LOCATION
                            )),
                            RC<AstBlock>(new AstBlock(
                                {
                                    RC<AstReturnStatement>(new AstReturnStatement(
                                        RC<AstCallExpression>(new AstCallExpression(
                                            RC<AstVariable>(new AstVariable(
                                                "__map_set",
                                                BUILTIN_SOURCE_LOCATION
                                            )),
                                            {
                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "self",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                )),

                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "key",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                )),

                                                RC<AstArgument>(new AstArgument(
                                                    RC<AstVariable>(new AstVariable(
                                                        "value",
                                                        BUILTIN_SOURCE_LOCATION
                                                    )),
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    "",
                                                    BUILTIN_SOURCE_LOCATION
                                                ))
                                            },
                                            false,
                                            BUILTIN_SOURCE_LOCATION
                                        )),
                                        BUILTIN_SOURCE_LOCATION
                                    ))
                                },
                                BUILTIN_SOURCE_LOCATION
                            )),
                            BUILTIN_SOURCE_LOCATION
                        )),
                        IdentifierFlags::FLAG_CONST,
                        BUILTIN_SOURCE_LOCATION
                    ))
                },
                true, // proxy class, so we can use Map<K, V>.length() like {}.length()
                BUILTIN_SOURCE_LOCATION
            )),
            {
                RC<AstParameter>(new AstParameter(
                    "K",
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            BuiltinTypes::CLASS_TYPE,
                            BUILTIN_SOURCE_LOCATION
                        )),
                        BUILTIN_SOURCE_LOCATION
                    )),
                    nullptr,
                    false,
                    false,
                    false,
                    BUILTIN_SOURCE_LOCATION
                )),

                RC<AstParameter>(new AstParameter(
                    "V",
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            BuiltinTypes::CLASS_TYPE,
                            BUILTIN_SOURCE_LOCATION
                        )),
                        BUILTIN_SOURCE_LOCATION
                    )),
                    nullptr,
                    false,
                    false,
                    false,
                    BUILTIN_SOURCE_LOCATION
                ))
            },
            nullptr,
            BUILTIN_SOURCE_LOCATION
        )),
        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC,
        BUILTIN_SOURCE_LOCATION
    )));
#endif

    // Variadic args wrapper, to be used in the `function` type
    // like: `function<ReturnType, varargs<T>>`
    m_vars.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
        "varargs",
        nullptr,
        RC<AstTemplateExpression>(new AstTemplateExpression(
            RC<AstTypeExpression>(new AstTypeExpression(
                "varargs",
                nullptr,
                {},
                {
                },
                {
                    // variadic trait
                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        BuiltinTypeTraits::variadic.name,
                        nullptr,
                        RC<AstTrue>(new AstTrue(BUILTIN_SOURCE_LOCATION)),
                        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_TRAIT,
                        BUILTIN_SOURCE_LOCATION
                    ))
                },
                true, // proxy class, so we can use Map<K, V>.length() like {}.length()
                BUILTIN_SOURCE_LOCATION
            )),
            {
                RC<AstParameter>(new AstParameter(
                    "T",
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            BuiltinTypes::CLASS_TYPE,
                            BUILTIN_SOURCE_LOCATION
                        )),
                        BUILTIN_SOURCE_LOCATION
                    )),
                    nullptr,
                    false,
                    false,
                    false,
                    BUILTIN_SOURCE_LOCATION
                ))
            },
            nullptr,
            BUILTIN_SOURCE_LOCATION
        )),
        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC,
        BUILTIN_SOURCE_LOCATION
    )));

    m_vars.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
        "function",
        nullptr,
        RC<AstTemplateExpression>(new AstTemplateExpression(
            RC<AstTypeExpression>(new AstTypeExpression(
                "function",
                nullptr,
                {},
                {
                },
                {
                },
                true, // proxy class, so we can use Map<K, V>.length() like {}.length()
                BUILTIN_SOURCE_LOCATION
            )),
            {
                RC<AstParameter>(new AstParameter(
                    "ReturnType",
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            BuiltinTypes::CLASS_TYPE,
                            BUILTIN_SOURCE_LOCATION
                        )),
                        BUILTIN_SOURCE_LOCATION
                    )),
                    nullptr,
                    false,
                    false,
                    false,
                    BUILTIN_SOURCE_LOCATION
                )),

                RC<AstParameter>(new AstParameter(
                    "Args",
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            BuiltinTypes::CLASS_TYPE,
                            BUILTIN_SOURCE_LOCATION
                        )),
                        BUILTIN_SOURCE_LOCATION
                    )),
                    nullptr,
                    true, // variadic
                    false,
                    false,
                    BUILTIN_SOURCE_LOCATION
                ))
            },
            nullptr,
            BUILTIN_SOURCE_LOCATION
        )),
        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC,
        BUILTIN_SOURCE_LOCATION
    )));
}

void Builtins::Visit(AstVisitor *visitor)
{
    Array<SymbolTypePtr_t> builtin_types {
        BuiltinTypes::PRIMITIVE_TYPE,
        BuiltinTypes::ANY,
        BuiltinTypes::OBJECT,
        BuiltinTypes::CLASS_TYPE,
        BuiltinTypes::ENUM_TYPE,
        BuiltinTypes::VOID_TYPE,
        BuiltinTypes::INT,
        BuiltinTypes::UNSIGNED_INT,
        BuiltinTypes::FLOAT,
        BuiltinTypes::BOOLEAN,
        BuiltinTypes::STRING
    };

    AstIterator ast;

    for (const SymbolTypePtr_t &type_ptr : builtin_types) {
        AssertThrow(type_ptr != nullptr);
        AssertThrow(type_ptr->GetId() == -1);
        AssertThrow(type_ptr->GetTypeObject() == nullptr);

        RC<AstTypeObject> type_object(new AstTypeObject(
            type_ptr,
            type_ptr->GetBaseType(),
            BUILTIN_SOURCE_LOCATION
        ));

        // push it so that it can be visited, and registered
        visitor->GetAstIterator()->Push(type_object);

        type_ptr->SetTypeObject(type_object);

        // add it to the global scope
        Scope &scope = m_unit->GetGlobalModule()->m_scopes.Top();
        scope.GetIdentifierTable().AddSymbolType(type_ptr);
    }

    for (auto &it : m_vars) {
        visitor->GetAstIterator()->Push(std::move(it));
    }
}

} // namespace hyperion::compiler
