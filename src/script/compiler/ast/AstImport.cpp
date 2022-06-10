#include <script/compiler/ast/AstImport.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <util/string_util.h>

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
    bool update_tree_link)
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
    }

    // add this module to the compilation unit
    visitor->GetCompilationUnit()->m_module_tree.Open(mod_to_copy);
    // open scope for module
    //mod->m_scopes.Open(Scope());

    if (update_tree_link) {
        mod_to_copy->SetImportTreeLink(visitor->GetCompilationUnit()->m_module_tree.TopNode());
    }

    // function to copy nested modules 
    std::function<void(TreeNode<Module*>*)> copy_nodes = [visitor, &copy_nodes, &update_tree_link](TreeNode<Module*> *link) {
        AssertThrow(link != nullptr);
        AssertThrow(link->m_value != nullptr);

        for (auto *sibling : link->m_siblings) {
            AssertThrow(sibling != nullptr);

            if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(sibling->m_value->GetName()) != nullptr) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_module_already_defined,
                    sibling->m_value->GetLocation(),
                    sibling->m_value->GetName()
                ));
            } else {
                visitor->GetCompilationUnit()->m_module_tree.Open(sibling->m_value);

                if (update_tree_link) {
                    sibling->m_value->SetImportTreeLink(sibling);
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

bool AstImport::TryOpenFile(const std::string &path, std::ifstream &is)
{
    is.open(path, std::ios::in | std::ios::ate);
    return is.is_open();
}

void AstImport::PerformImport(
    AstVisitor *visitor,
    Module *mod,
    const std::string &filepath
    
    /*bool make_parent_module,
    const std::string &parent_module_name*/)
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
    std::vector<std::string> path_vec = StringUtil::SplitPath(filepath);
    // canonicalize the vector
    path_vec = StringUtil::CanonicalizePath(path_vec);
    // put it back into a string
    const std::string canon_path = StringUtil::PathToString(path_vec);

    // first, check if the file has already been imported somewhere in this compilation unit
    const auto it = visitor->GetCompilationUnit()->m_imported_modules.find(canon_path);
    if (it != visitor->GetCompilationUnit()->m_imported_modules.end()) {
        // imported file found, so just re-open all
        // modules that belong to the file into this scope
        for (std::shared_ptr<Module> &mod : it->second) {
            AstImport::CopyModules(
                visitor,
                mod.get(),
                false
            );
        }
    } else {
        /*if (make_parent_module) {
            // Create a new module object called whatever the FILENAME is,
            // and copy all the modules from that file into it.
            // e.g a file called car.ace with module "car_example" in it:
            // import car
            //    to use something, write car::car_example::car1 or whatever
            // or you can write:
            // import car::car_example to get around having to write car

            AssertThrow(!parent_module_name.empty());

            SourceLocation mod_location = m_location;
            mod_location.SetFileName(filepath);

            std::shared_ptr<Module> new_module(new Module(
                parent_module_name,
                mod_location
            ));

            // map filepath to module
            visitor->GetCompilationUnit()->m_imported_modules[canon_path] = { new_module };

            visitor->GetCompilationUnit()->m_module_tree.Open(new_module.get());
            new_module->SetImportTreeLink(visitor->GetCompilationUnit()->m_module_tree.TopNode());
        }*/

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
            size_t max = file.tellg();
            // seek to beginning
            file.seekg(0, std::ios::beg);
            // load stream into file buffer
            SourceFile source_file(filepath, max);
            file.read(source_file.GetBuffer(), max);

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
