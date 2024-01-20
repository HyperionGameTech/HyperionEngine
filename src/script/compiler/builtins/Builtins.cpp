#include <script/compiler/builtins/Builtins.hpp>
#include <script/SourceFile.hpp>
#include <script/SourceStream.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/SourceLocation.hpp>

namespace hyperion::compiler {

const SourceLocation Builtins::BUILTIN_SOURCE_LOCATION(-1, -1, "<builtin>");

Builtins::Builtins()
{
    m_vars["any"].Reset(new AstTypeObject(
        BuiltinTypes::ANY, nullptr, SourceLocation::eof
    ));

    m_vars["Class"].Reset(new AstTypeObject(
        BuiltinTypes::CLASS_TYPE, nullptr, SourceLocation::eof
    ));

    m_vars["Object"].Reset(new AstTypeObject(
        BuiltinTypes::OBJECT, nullptr, SourceLocation::eof
    ));

    m_vars["void"].Reset(new AstTypeObject(
        BuiltinTypes::VOID_TYPE, nullptr, SourceLocation::eof
    ));

    m_vars["int"].Reset(new AstTypeObject(
        BuiltinTypes::INT, nullptr, SourceLocation::eof
    ));

    m_vars["uint"].Reset(new AstTypeObject(
        BuiltinTypes::UNSIGNED_INT, nullptr, SourceLocation::eof
    ));

    m_vars["float"].Reset(new AstTypeObject(
        BuiltinTypes::FLOAT, nullptr, SourceLocation::eof
    ));

    m_vars["bool"].Reset(new AstTypeObject(
        BuiltinTypes::BOOLEAN, nullptr, SourceLocation::eof
    ));

    m_vars["string"].Reset(new AstTypeObject(
        BuiltinTypes::STRING, nullptr, SourceLocation::eof
    ));

    m_vars["Function"].Reset(new AstTypeObject(
        BuiltinTypes::FUNCTION, nullptr, SourceLocation::eof
    ));

    m_vars["Array"].Reset(new AstTemplateExpression(
        RC<AstTypeObject>(new AstTypeObject(
            BuiltinTypes::ARRAY, nullptr, SourceLocation::eof
        )),
        {
            RC<AstParameter>(new AstParameter(
                "of", nullptr, nullptr, false, false, false, SourceLocation::eof
            ))
        },
        nullptr,
        SourceLocation::eof
    ));

    for (const auto &it : m_vars) {
        m_ast.Push(RC<AstVariableDeclaration>(new AstVariableDeclaration(
            it.first,
            nullptr,
            it.second,
            {},
            IdentifierFlags::FLAG_CONST,
            BUILTIN_SOURCE_LOCATION
        )));
    }
}

void Builtins::Visit(CompilationUnit *unit)
{
    SemanticAnalyzer analyzer(&m_ast, unit);
    analyzer.Analyze(false);

    m_ast.ResetPosition();
}

std::unique_ptr<BytecodeChunk> Builtins::Build(CompilationUnit *unit)
{
    Compiler compiler(&m_ast, unit);
    std::unique_ptr<BytecodeChunk> chunk = compiler.Compile();

    m_ast.ResetPosition();

    return chunk;
}

} // namespace hyperion::compiler
