#include <script/compiler/builtins/Builtins.hpp>
#include <script/SourceFile.hpp>
#include <script/SourceStream.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/SourceLocation.hpp>

#include <core/lib/DynArray.hpp>

namespace hyperion::compiler {

const SourceLocation Builtins::BUILTIN_SOURCE_LOCATION(-1, -1, "<builtin>");

Builtins::Builtins()
{
}

void Builtins::Visit(AstVisitor *visitor, CompilationUnit *unit)
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
        BuiltinTypes::STRING,
        BuiltinTypes::FUNCTION,
        BuiltinTypes::ARRAY
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
        Scope &scope = unit->GetGlobalModule()->m_scopes.Top();
        scope.GetIdentifierTable().AddSymbolType(type_ptr);
    }

    // visitor->GetAstIterator()->Prepend(std::move(ast));
}

} // namespace hyperion::compiler
