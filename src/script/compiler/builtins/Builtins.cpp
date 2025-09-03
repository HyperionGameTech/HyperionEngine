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
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstClass.hpp>
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
}

void Builtins::Visit(AstVisitor* visitor)
{
    Array<SymbolTypeRef> builtinTypes {
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

    AstIterator ast;

    for (const SymbolTypeRef& typePtr : builtinTypes)
    {
        Assert(typePtr != nullptr);

        // add it to the global scope
        Scope& scope = m_unit->GetGlobalModule()->m_scopes.Top();
        scope.identifierTable.AddSymbolType(typePtr);
    }

    for (auto& it : m_vars)
    {
        visitor->GetAstIterator()->Push(std::move(it));
    }
}

} // namespace hyperion::compiler
