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
    m_vars["Class"].Reset(new AstTypeObject(
        BuiltinTypes::CLASS_TYPE, nullptr, SourceLocation::eof
    ));

    m_vars["Null"].Reset(new AstTypeObject(
        BuiltinTypes::NULL_TYPE, nullptr, SourceLocation::eof
    ));

    m_vars["void"].Reset(new AstTypeObject(
        BuiltinTypes::VOID_TYPE, nullptr, SourceLocation::eof
    ));

    m_vars["Int"].Reset(new AstTypeObject(
        BuiltinTypes::INT, nullptr, SourceLocation::eof
    ));

    m_vars["UInt"].Reset(new AstTypeObject(
        BuiltinTypes::UNSIGNED_INT, nullptr, SourceLocation::eof
    ));

    m_vars["Float"].Reset(new AstTypeObject(
        BuiltinTypes::FLOAT, nullptr, SourceLocation::eof
    ));

    m_vars["Number"].Reset(new AstTypeObject(
        BuiltinTypes::NUMBER, nullptr, SourceLocation::eof
    ));

    m_vars["Bool"].Reset(new AstTypeObject(
        BuiltinTypes::BOOLEAN, nullptr, SourceLocation::eof
    ));

    m_vars["String"].Reset(new AstTypeObject(
        BuiltinTypes::STRING, nullptr, SourceLocation::eof
    ));

    m_vars["Function"].Reset(new AstTypeObject(
        BuiltinTypes::FUNCTION, nullptr, SourceLocation::eof
    ));

    m_vars["Enum"].Reset(new AstTypeObject(
        BuiltinTypes::ENUM_TYPE, nullptr, SourceLocation::eof
    ));

    m_vars["Array"].Reset(new AstTemplateExpression(
        sp<AstTypeObject>(new AstTypeObject(
            BuiltinTypes::ARRAY, nullptr, SourceLocation::eof
        )),
        {
            sp<AstParameter>(new AstParameter(
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

    return std::move(chunk);
}

} // namespace hyperion::compiler
