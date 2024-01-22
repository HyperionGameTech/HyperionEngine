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
    m_vars["any"].Reset(new AstTypeRef(
        BuiltinTypes::ANY, SourceLocation::eof
    ));

    m_vars["Class"].Reset(new AstTypeRef(
        BuiltinTypes::CLASS_TYPE, SourceLocation::eof
    ));

    m_vars["Object"].Reset(new AstTypeRef(
        BuiltinTypes::OBJECT, SourceLocation::eof
    ));

    m_vars["void"].Reset(new AstTypeRef(
        BuiltinTypes::VOID_TYPE, SourceLocation::eof
    ));

    m_vars["int"].Reset(new AstTypeRef(
        BuiltinTypes::INT, SourceLocation::eof
    ));

    m_vars["uint"].Reset(new AstTypeRef(
        BuiltinTypes::UNSIGNED_INT, SourceLocation::eof
    ));

    m_vars["float"].Reset(new AstTypeRef(
        BuiltinTypes::FLOAT, SourceLocation::eof
    ));

    m_vars["bool"].Reset(new AstTypeRef(
        BuiltinTypes::BOOLEAN, SourceLocation::eof
    ));

    m_vars["string"].Reset(new AstTypeRef(
        BuiltinTypes::STRING, SourceLocation::eof
    ));

    m_vars["Function"].Reset(new AstTypeRef(
        BuiltinTypes::FUNCTION, SourceLocation::eof
    ));

    m_vars["Array"].Reset(new AstTemplateExpression(
        RC<AstTypeRef>(new AstTypeRef(
            BuiltinTypes::ARRAY, SourceLocation::eof
        )),
        {
            RC<AstParameter>(new AstParameter(
                "of", nullptr, nullptr, false, false, false, SourceLocation::eof
            ))
        },
        nullptr,
        SourceLocation::eof
    ));

    // for (const auto &it : m_vars) {
    //     m_ast.Push(RC<AstVariableDeclaration>(new AstVariableDeclaration(
    //         it.first,
    //         nullptr,
    //         it.second,
    //         IdentifierFlags::FLAG_CONST,
    //         BUILTIN_SOURCE_LOCATION
    //     )));
    // }
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

std::unique_ptr<BytecodeChunk> Builtins::Build(CompilationUnit *unit)
{
    // Compiler compiler(&m_ast, unit);
    // std::unique_ptr<BytecodeChunk> chunk = compiler.Compile();

    // m_ast.ResetPosition();

    // return chunk;

    return nullptr;
}

} // namespace hyperion::compiler
