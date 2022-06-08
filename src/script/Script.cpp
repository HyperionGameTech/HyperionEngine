#include "Script.hpp"

#include <script/compiler/Module.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/dis/DecompilationUnit.hpp>
#include <script/compiler/emit/codegen/Codegen.hpp>

#include <script/vm/VM.hpp>

namespace hyperion {
namespace compiler {

Script::Script(const SourceFile &source_file)
    : m_source_file(source_file)
{

}

bool Script::Compile()
{
    if (!m_source_file.IsValid()) {
        return false;
    }

    CompilationUnit compilation_unit;

    SourceStream source_stream(&m_source_file);

    TokenStream token_stream(TokenStreamInfo {
        m_source_file.GetFilePath()
    });

    Lexer lex(source_stream, &token_stream, &compilation_unit);
    lex.Analyze();

    AstIterator ast_iterator;
    Parser parser(&ast_iterator, &token_stream, &compilation_unit);
    parser.Parse();

    SemanticAnalyzer semantic_analyzer(&ast_iterator, &compilation_unit);
    semantic_analyzer.Analyze();

    compilation_unit.GetErrorList().SortErrors();
    m_errors = compilation_unit.GetErrorList();

    if (!compilation_unit.GetErrorList().HasFatalErrors()) {
        // only optimize if there were no errors
        // before this point
        ast_iterator.ResetPosition();
        Optimizer optimizer(&ast_iterator, &compilation_unit);
        optimizer.Optimize();

        // compile into bytecode instructions
        ast_iterator.ResetPosition();

        Compiler compiler(&ast_iterator, &compilation_unit);

        if ((m_bytecode_chunk = compiler.Compile())) {
            return true;
        }
    }

    return false;
}

void Script::Bake()
{
    AssertThrow(IsCompiled());

    BuildParams build_params{};

    Bake(build_params);
}

void Script::Bake(BuildParams &build_params)
{
    AssertThrow(IsCompiled());

    CodeGenerator code_generator(build_params);
    code_generator.Visit(m_bytecode_chunk.get());

    m_baked_bytes = code_generator.GetInternalByteStream().Bake();
}

void Script::Run()
{
    AssertThrow(IsCompiled() && IsBaked());

    using namespace vm;

    BytecodeStream bytecode_stream(reinterpret_cast<const char *>(m_baked_bytes.data()), m_baked_bytes.size());

    VM().Execute(&bytecode_stream);
}

} // namespace compiler
} // namespace hyperion
