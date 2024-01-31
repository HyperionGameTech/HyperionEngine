#include <script/compiler/builtins/Builtins.hpp>
#include <script/SourceFile.hpp>
#include <script/SourceStream.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstString.hpp>
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

        // add 'name' member here
        type_ptr->AddMember({
            "name",
            BuiltinTypes::STRING,
            RC<AstString>(new AstString(type_ptr->GetName(), BUILTIN_SOURCE_LOCATION))
        });

        // Add "$proto" so we can use is_instance to check if a value is an instance of a type
        if (type_ptr->IsPrimitive() && type_ptr->GetDefaultValue() != nullptr) {
            type_ptr->AddMember({
                "$proto",
                type_ptr,
                type_ptr->GetDefaultValue()
            });
        }

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
