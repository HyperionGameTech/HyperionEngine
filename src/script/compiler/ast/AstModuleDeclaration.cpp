#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/debug.h>
#include <util/string_util.h>

namespace hyperion {
namespace compiler {

AstModuleDeclaration::AstModuleDeclaration(
    const std::string &name,
    const std::vector<std::shared_ptr<AstStatement>> &children,
    const SourceLocation &location)
    : AstDeclaration(name, location),
      m_children(children)
{
}

AstModuleDeclaration::AstModuleDeclaration(const std::string &name, const SourceLocation &location)
    : AstDeclaration(name, location)
{
}

void AstModuleDeclaration::PerformLookup(AstVisitor *visitor)
{
    // make sure this module was not already declared/imported
    if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(m_name)) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_module_already_defined,
            m_location,
            m_name
        ));
    } else {
        m_module.reset(new Module(m_name, m_location));
    }
}

void AstModuleDeclaration::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);

    if (m_module == nullptr) {
        PerformLookup(visitor);
    }

    if (m_module != nullptr) {
        // add this module to the compilation unit
        visitor->GetCompilationUnit()->m_module_tree.Open(m_module.get());
        // set the link to the module in the tree
        m_module->SetImportTreeLink(visitor->GetCompilationUnit()->m_module_tree.TopNode());

        // add this module to list of imported modules,
        // but only if mod == nullptr, that way we don't add nested modules
        if (mod == nullptr) {
            // parse filename
            std::vector<std::string> path = StringUtil::SplitPath(m_location.GetFileName());
            path = StringUtil::CanonicalizePath(path);
            // change it back to string
            std::string canon_path = StringUtil::PathToString(path);

            // map filepath to module
            auto it = visitor->GetCompilationUnit()->m_imported_modules.find(canon_path);
            if (it != visitor->GetCompilationUnit()->m_imported_modules.end()) {
                it->second.push_back(m_module);
            } else {
                visitor->GetCompilationUnit()->m_imported_modules[canon_path] = { m_module };
            }
        }

        // update current module
        mod = m_module.get();
        AssertThrow(mod == visitor->GetCompilationUnit()->GetCurrentModule());

        // visit all children
        for (auto &child : m_children) {
            if (child != nullptr) {
                child->Visit(visitor, mod);
            }
        }

        // close this module
        visitor->GetCompilationUnit()->m_module_tree.Close();
    }
}

std::unique_ptr<Buildable> AstModuleDeclaration::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_module != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // build all children
    for (auto &child : m_children) {
        if (child != nullptr) {
            chunk->Append(child->Build(visitor, m_module.get()));
        }
    }

    return std::move(chunk);
}

void AstModuleDeclaration::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_module != nullptr);

    // optimize all children
    for (auto &child : m_children) {
        if (child) {
            child->Optimize(visitor, m_module.get());
        }
    }
}

Pointer<AstStatement> AstModuleDeclaration::Clone() const
{
    return CloneImpl();
}

} // namespace compiler
} // namespace hyperion
