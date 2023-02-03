#include <script/compiler/ast/AstModuleImport.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>

#include <util/fs/FsUtil.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <functional>

namespace hyperion::compiler {

AstModuleImportPart::AstModuleImportPart(
    const std::string &left,
    const std::vector<RC<AstModuleImportPart>> &right_parts,
    const SourceLocation &location
) : AstStatement(location),
    m_left(left),
    m_right_parts(right_parts),
    m_pull_in_modules(true)
{
}

void AstModuleImportPart::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    RC<Identifier> left_identifier;

    if (Module *this_module = mod->LookupNestedModule(m_left)) {
        if (m_pull_in_modules && m_right_parts.empty()) {
            // pull this module into scope
            AstImport::CopyModules(
                visitor,
                this_module,
                false
            );
        } else {
            // get nested items
            for (const RC<AstModuleImportPart> &part : m_right_parts) {
                AssertThrow(part != nullptr);
                part->Visit(visitor, this_module);

                if (!part->GetIdentifiers().empty()) {
                    for (const auto &identifier : part->GetIdentifiers()) {
                        m_identifiers.push_back(identifier);
                    }
                }
            }
        }
    } else if (m_right_parts.empty() && (left_identifier = mod->LookUpIdentifier(m_left, false))) {
        // pull the identifier into local scope
        m_identifiers.push_back(std::move(left_identifier));
    } else {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_could_not_find_nested_module,
            m_location,
            m_left,
            mod->GetName()
        ));
    }
}

std::unique_ptr<Buildable> AstModuleImportPart::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstModuleImportPart::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstModuleImportPart::Clone() const
{
    return CloneImpl();
}


AstModuleImport::AstModuleImport(
    const std::vector<RC<AstModuleImportPart>> &parts,
    const SourceLocation &location
) : AstImport(location),
    m_parts(parts)
{
}

void AstModuleImport::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(!m_parts.empty());

    const RC<AstModuleImportPart> &first = m_parts[0];
    AssertThrow(first != nullptr);

    bool opened = false;

    // already imported into this module, set opened to true
    if (mod->LookupNestedModule(first->GetLeft()) != nullptr) {
        opened = true;
    }

    // do not pull module into scope for single imports
    // i.e `import range` will just import the file
    if (first->GetParts().empty()) {
        first->SetPullInModules(false);
    }

    std::vector<std::string> tried_paths;

    // if this is not a direct import (i.e `import range`),
    // we will allow duplicates in imports like `import range::{_Detail_}`
    // and we won't import the 'range' module again
    if (first->GetParts().empty() || !opened) {
        // find the folder which the current file is in
        const SizeType index = m_location.GetFileName().find_last_of("/\\");

        std::string current_dir;
        if (index != std::string::npos) {
            current_dir = m_location.GetFileName().substr(0, index);
        }

        std::ifstream file;

        std::unordered_set<std::string> scan_paths;
        std::string found_path;

        // add current directory as first.
        scan_paths.insert(current_dir);

        // add this module's scan paths.
        scan_paths.insert(
            mod->GetScanPaths().begin(),
            mod->GetScanPaths().end()
        );

        // add global module's scan paths
        const std::unordered_set<std::string> &global_scan_paths =
            visitor->GetCompilationUnit()->GetGlobalModule()->GetScanPaths();
        
        scan_paths.insert(
            global_scan_paths.begin(),
            global_scan_paths.end()
        );

        // iterate through library paths to try and find a file
        for (const std::string &scan_path : scan_paths) {
            const std::string &filename = first->GetLeft();
            const std::string ext = ".hypscript";

            found_path = FileSystem::Join(scan_path, filename + ext);
            tried_paths.push_back(found_path);

            if (AstImport::TryOpenFile(found_path, file)) {
                opened = true;
                break;
            }

            // try it without extension
            found_path = FileSystem::Join(scan_path, filename);
            tried_paths.push_back(found_path);

            if (AstImport::TryOpenFile(found_path, file)) {
                opened = true;
                break;
            }
        }

        if (opened) {
            AstImport::PerformImport(
                visitor,
                mod,
                found_path
            );
        }
    }

    if (opened) {
        std::vector<RC<Identifier>> pulled_in_identifiers;

        for (const RC<AstModuleImportPart> &part : m_parts) {
            AssertThrow(part != nullptr);
            part->Visit(visitor, mod);

            for (const RC<Identifier> &identifier : part->GetIdentifiers()) {
                pulled_in_identifiers.push_back(identifier);
            }
        }

        for (auto &identifier : pulled_in_identifiers) {
            if (!mod->m_scopes.Top().GetIdentifierTable().AddIdentifier(identifier)) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_redeclared_identifier,
                    m_location,
                    identifier->GetName()
                ));
            }
        }
    } else {
        std::stringstream tried_paths_string;
        tried_paths_string << "[";

        for (size_t i = 0; i < tried_paths.size(); i++) {
            tried_paths_string << '"' << tried_paths[i] << '"';

            if (i != tried_paths.size() - 1) {
                tried_paths_string << ", ";
            }
        }

        tried_paths_string << "]";

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_could_not_find_module,
            m_location,
            first->GetLeft(),
            tried_paths_string.str()
        ));
    }
}

RC<AstStatement> AstModuleImport::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
