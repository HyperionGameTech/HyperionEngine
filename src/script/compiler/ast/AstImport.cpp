#include <script/compiler/ast/AstImport.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <util/StringUtil.hpp>

#include <Types.hpp>

#include <fstream>
#include <iostream>
#include <functional>

namespace hyperion::compiler {

AstImport::AstImport(const SourceLocation &location)
    : AstStatement(location)
{
}

void AstImport::CopyModules(
    AstVisitor *visitor,
    Module *mod_to_copy,
    bool update_tree_link
)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod_to_copy != nullptr);
    AssertThrow(visitor->GetCompilationUnit()->GetCurrentModule() != nullptr);

    if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(mod_to_copy->GetName()) != nullptr) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_module_already_defined,
            mod_to_copy->GetLocation(),
            mod_to_copy->GetName()
        ));

        return;
    }

    // add this module to the compilation unit
    visitor->GetCompilationUnit()->m_module_tree.Open(mod_to_copy);
    // open scope for module
    //mod->m_scopes.Open(Scope());

    if (update_tree_link) {
        mod_to_copy->SetImportTreeLink(visitor->GetCompilationUnit()->m_module_tree.TopNode());
    }

    // function to copy nested modules 
    std::function<void(TreeNode<Module *> *)> copy_nodes = [visitor, &copy_nodes, &update_tree_link](TreeNode<Module*> *link) {
        AssertThrow(link != nullptr);
        AssertThrow(link->Get() != nullptr);

        for (auto *sibling : link->m_siblings) {
            AssertThrow(sibling != nullptr);

            if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(sibling->Get()->GetName()) != nullptr) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_module_already_defined,
                    sibling->Get()->GetLocation(),
                    sibling->Get()->GetName()
                ));
            } else {
                visitor->GetCompilationUnit()->m_module_tree.Open(sibling->Get());

                if (update_tree_link) {
                    sibling->Get()->SetImportTreeLink(sibling);
                }

                copy_nodes(sibling);

                visitor->GetCompilationUnit()->m_module_tree.Close();
            }
        }
    };

    // copy all nested modules
    copy_nodes(mod_to_copy->GetImportTreeLink());

    // close scope for module
    //mod->m_scopes.Close();

    // close module
    visitor->GetCompilationUnit()->m_module_tree.Close();
}

bool AstImport::TryOpenFile(const String &path, std::ifstream &is)
{
    is.open(path.Data(), std::ios::in | std::ios::ate);
    return is.is_open();
}

void AstImport::PerformImport(
    AstVisitor *visitor,
    Module *mod,
    const String &filepath
)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    if (!mod->IsInGlobalScope()) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_import_outside_global,
            m_location
        ));

        return;
    }

    // parse path into vector
    Array<String> path_parts = filepath.Split('\\', '/');
    // canonicalize the vector
    path_parts = StringUtil::CanonicalizePath(path_parts);
    // put it back into a string
    const String canon_path = String::Join(path_parts, '/');

    // first, check if the file has already been imported somewhere in this compilation unit
    const auto it = visitor->GetCompilationUnit()->m_imported_modules.Find(canon_path);
    if (it != visitor->GetCompilationUnit()->m_imported_modules.End()) {
        // imported file found, so just re-open all
        // modules that belong to the file into this scope

        // TODO: Fix issues with duplicated symbols...
        for (RC<Module> &mod : it->second) {
            AstImport::CopyModules(
                visitor,
                mod.Get(),
                false
            );
        }
    } else {
        // file hasn't been imported, so open it
        std::ifstream file;

        if (!TryOpenFile(filepath, file)) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_could_not_open_file,
                m_location,
                filepath
            ));
        } else {
            // get number of bytes
            SizeType max = file.tellg();
            // seek to beginning
            file.seekg(0, std::ios::beg);
            // load stream into file buffer
            SourceFile source_file(filepath, max);

            ByteBuffer temp;
            temp.SetSize(max);

            file.read(reinterpret_cast<char *>(temp.Data()), max);

            source_file.ReadIntoBuffer(temp);

            // use the lexer and parser on this file buffer
            TokenStream token_stream(TokenStreamInfo {
                filepath
            });

            Lexer lexer(SourceStream(&source_file), &token_stream, visitor->GetCompilationUnit());
            lexer.Analyze();

            Parser parser(&m_ast_iterator, &token_stream, visitor->GetCompilationUnit());
            parser.Parse();

            SemanticAnalyzer semantic_analyzer(&m_ast_iterator, visitor->GetCompilationUnit());
            semantic_analyzer.Analyze();
        }

        /*if (make_parent_module) {
            visitor->GetCompilationUnit()->m_module_tree.Close();
        }*/
    }
}

std::unique_ptr<Buildable> AstImport::Build(AstVisitor *visitor, Module *mod)
{
    m_ast_iterator.ResetPosition();

    // compile the imported module
    Compiler compiler(&m_ast_iterator, visitor->GetCompilationUnit());
    return compiler.Compile();
}

void AstImport::Optimize(AstVisitor *visitor, Module *mod)
{
    m_ast_iterator.ResetPosition();

    // optimize the imported module
    Optimizer optimizer(&m_ast_iterator, visitor->GetCompilationUnit());
    optimizer.Optimize();
}

} // namespace hyperion::compiler
