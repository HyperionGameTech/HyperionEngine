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
    m_vars["Class"].reset(new AstTypeObject(
        BuiltinTypes::CLASS_TYPE, nullptr, SourceLocation::eof
    ));

    m_vars["Null"].reset(new AstTypeObject(
        BuiltinTypes::NULL_TYPE, nullptr, SourceLocation::eof
    ));

    m_vars["int"].reset(new AstTypeObject(
        BuiltinTypes::INT, nullptr, SourceLocation::eof
    ));

    m_vars["uint"].reset(new AstTypeObject(
        BuiltinTypes::UNSIGNED_INT, nullptr, SourceLocation::eof
    ));

    m_vars["float"].reset(new AstTypeObject(
        BuiltinTypes::FLOAT, nullptr, SourceLocation::eof
    ));

    m_vars["number"].reset(new AstTypeObject(
        BuiltinTypes::NUMBER, nullptr, SourceLocation::eof
    ));

    m_vars["bool"].reset(new AstTypeObject(
        BuiltinTypes::BOOLEAN, nullptr, SourceLocation::eof
    ));

    m_vars["string"].reset(new AstTypeObject(
        BuiltinTypes::STRING, nullptr, SourceLocation::eof
    ));

    m_vars["Function"].reset(new AstTypeObject(
        BuiltinTypes::FUNCTION, nullptr, SourceLocation::eof
    ));

    m_vars["array"].reset(new AstTemplateExpression(
        sp<AstTypeObject>(new AstTypeObject(
            BuiltinTypes::ARRAY, nullptr, SourceLocation::eof
        )),
        {
            sp<AstParameter>(new AstParameter(
                "of", nullptr, nullptr, false, false, SourceLocation::eof
            ))
        },
        nullptr,
        SourceLocation::eof
    ));

    for (const auto &it : m_vars) {
        m_ast.Push(std::shared_ptr<AstVariableDeclaration>(new AstVariableDeclaration(
            it.first,
            nullptr,
            it.second,
            {},
            true, // const
            false, // not generic 
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

    return std::move(chunk);
}

} // namespace hyperion::compiler
