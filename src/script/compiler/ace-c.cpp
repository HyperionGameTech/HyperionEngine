#include <script/compiler/ace-c.hpp>

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

#include <util/string_util.h>

#include <fstream>
#include <unordered_set>

namespace hyperion {
namespace compiler {

std::unique_ptr<BytecodeChunk> BuildSourceFile(
    const utf::Utf8String &filename,
    const utf::Utf8String &out_filename)
{
    CompilationUnit compilation_unit;
    return BuildSourceFile(filename, out_filename, compilation_unit);
}

std::unique_ptr<BytecodeChunk> BuildSourceFile(
    const utf::Utf8String &filename,
    const utf::Utf8String &out_filename,
    CompilationUnit &compilation_unit)
{
    std::ifstream in_file(
        filename.GetData(),
        std::ios::in | std::ios::ate | std::ios::binary
    );

    if (!in_file.is_open()) {
        utf::cout << "Could not open file: " << filename << "\n";
    } else {
        // get number of bytes
        size_t max = in_file.tellg();
        // seek to beginning
        in_file.seekg(0, std::ios::beg);
        // load stream into file buffer
        SourceFile source_file(filename.GetData(), max);
        in_file.read(source_file.GetBuffer(), max);
        in_file.close();

        SourceStream source_stream(&source_file);

        TokenStream token_stream(TokenStreamInfo {
            std::string(filename.GetData())
        });

        Lexer lex(source_stream, &token_stream, &compilation_unit);
        lex.Analyze();

        AstIterator ast_iterator;
        Parser parser(&ast_iterator, &token_stream, &compilation_unit);
        parser.Parse();

        SemanticAnalyzer semantic_analyzer(&ast_iterator, &compilation_unit);
        semantic_analyzer.Analyze();

        compilation_unit.GetErrorList().SortErrors();
        compilation_unit.GetErrorList().WriteOutput(std::cout); // TODO make utf8 compatible

        /*for (CompilerError &error : compilation_unit.GetErrorList().m_errors) {
            if (error_filenames.insert(error.GetLocation().GetFileName()).second) {
                auto split = str_util::split_path(error.GetLocation().GetFileName());
                std::string str = !split.empty() ? split.back() : error.GetLocation().GetFileName();
                str = str_util::strip_extension(str);

                utf::cout << "In file \"" << utf::Utf8String(str.c_str()) << "\":\n";
            }

            utf::cout << "  "
                      << "Ln "    << (error.GetLocation().GetLine() + 1)
                      << ", Col " << (error.GetLocation().GetColumn() + 1)
                      << ":  "    << utf::Utf8String(error.GetText().c_str()) << "\n";
        }*/

        if (!compilation_unit.GetErrorList().HasFatalErrors()) {
            // only optimize if there were no errors
            // before this point
            ast_iterator.ResetPosition();
            Optimizer optimizer(&ast_iterator, &compilation_unit);
            optimizer.Optimize();

            // compile into bytecode instructions
            ast_iterator.ResetPosition();
            Compiler compiler(&ast_iterator, &compilation_unit);
            return compiler.Compile();
        }
    }

    return nullptr;
}

void DecompileBytecodeFile(const utf::Utf8String &filename, const utf::Utf8String &out_filename)
{
    std::ifstream in_file(filename.GetData(),
        std::ios::in | std::ios::ate | std::ios::binary);

    if (!in_file.is_open()) {
        utf::cout << "Could not open file: " << filename << "\n";
    } else {
        // get number of bytes
        size_t max = in_file.tellg();
        // seek to beginning
        in_file.seekg(0, std::ios::beg);
        // load stream into file buffer
        SourceFile source_file(filename.GetData(), max);
        in_file.read(source_file.GetBuffer(), max);
        in_file.close();

        vm::BytecodeStream bs = vm::BytecodeStream::FromSourceFile(&source_file);

        DecompilationUnit decompilation_unit;

        bool write_to_file = false;
        utf::utf8_ostream *os = nullptr;
        if (out_filename == "") {
            os = &utf::cout;
        } else {
            write_to_file = true;
            os = new utf::utf8_ofstream(out_filename.GetData(), std::ios::out | std::ios::binary);
        }

        decompilation_unit.Decompile(bs, os);

        if (write_to_file) {
            delete os;
        }
    }
}

} // namespace compiler
} // namespace hyperion
