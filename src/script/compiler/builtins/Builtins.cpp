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

#include <core/containers/Array.hpp>

namespace hyperion::compiler {

const SourceLocation Builtins::BUILTIN_SOURCE_LOCATION(-1, -1, "<builtin>");

Builtins::Builtins(CompilationUnit* unit)
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
                {},
                { // variadic trait
                    RC<AstVariableDeclaration>(new AstVariableDeclaration(
                        BuiltinTypeTraits::variadic.name,
                        nullptr,
                        RC<AstTrue>(new AstTrue(BUILTIN_SOURCE_LOCATION)),
                        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_TRAIT,
                        BUILTIN_SOURCE_LOCATION)) },
                true, // proxy class, so we can use Map<K, V>.length() like {}.length()
                BUILTIN_SOURCE_LOCATION)),
            { RC<AstParameter>(new AstParameter(
                "T",
                RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                    RC<AstTypeRef>(new AstTypeRef(
                        BuiltinTypes::CLASS_TYPE,
                        BUILTIN_SOURCE_LOCATION)),
                    BUILTIN_SOURCE_LOCATION)),
                nullptr,
                false,
                false,
                false,
                BUILTIN_SOURCE_LOCATION)) },
            nullptr,
            BUILTIN_SOURCE_LOCATION)),
        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC,
        BUILTIN_SOURCE_LOCATION)));

    m_vars.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
        "function",
        nullptr,
        RC<AstTemplateExpression>(new AstTemplateExpression(
            RC<AstTypeExpression>(new AstTypeExpression(
                "function",
                nullptr,
                {},
                {},
                {},
                true, // proxy class, so we can use Map<K, V>.length() like {}.length()
                BUILTIN_SOURCE_LOCATION)),
            { RC<AstParameter>(new AstParameter(
                  "ReturnType",
                  RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                      RC<AstTypeRef>(new AstTypeRef(
                          BuiltinTypes::CLASS_TYPE,
                          BUILTIN_SOURCE_LOCATION)),
                      BUILTIN_SOURCE_LOCATION)),
                  nullptr,
                  false,
                  false,
                  false,
                  BUILTIN_SOURCE_LOCATION)),

                RC<AstParameter>(new AstParameter(
                    "Args",
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            BuiltinTypes::CLASS_TYPE,
                            BUILTIN_SOURCE_LOCATION)),
                        BUILTIN_SOURCE_LOCATION)),
                    nullptr,
                    true, // variadic
                    false,
                    false,
                    BUILTIN_SOURCE_LOCATION)) },
            nullptr,
            BUILTIN_SOURCE_LOCATION)),
        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC,
        BUILTIN_SOURCE_LOCATION)));
}

void Builtins::Visit(AstVisitor* visitor)
{
    Array<SymbolTypeRef> builtinTypes {
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

    for (const SymbolTypeRef& typePtr : builtinTypes)
    {
        Assert(typePtr != nullptr);

        typePtr->SetId(-1); // TEMP; refactor this
        // typePtr->SetTypeObject({});

        Assert(typePtr->GetId() == -1);
        // Assert(typePtr->GetTypeObject() == nullptr);

        // add 'name' member here
        typePtr->AddMember({ "name",
            BuiltinTypes::STRING,
            RC<AstString>(new AstString(typePtr->GetName(), BUILTIN_SOURCE_LOCATION)) });

        // Add "$proto" so we can use isInstance to check if a value is an instance of a type
        if (typePtr->IsPrimitive() && typePtr->GetDefaultValue() != nullptr)
        {
            typePtr->AddMember({ "$proto",
                typePtr,
                typePtr->GetDefaultValue() });
        }

        // RC<AstTypeObject> typeObject(new AstTypeObject(
        //     typePtr,
        //     typePtr->GetBaseType(),
        //     BUILTIN_SOURCE_LOCATION));

        // // push it so that it can be visited, and registered
        // visitor->GetAstIterator()->Push(typeObject);

        // typePtr->SetTypeObject(typeObject);

        // add it to the global scope
        Scope& scope = m_unit->GetGlobalModule()->m_scopes.Top();
        scope.GetIdentifierTable().AddSymbolType(typePtr);
    }

    for (auto& it : m_vars)
    {
        visitor->GetAstIterator()->Push(std::move(it));
    }
}

} // namespace hyperion::compiler
