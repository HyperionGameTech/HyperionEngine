#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/debug/Debug.hpp>
#include <core/utilities/StringUtil.hpp>

namespace hyperion::compiler {

AstModuleDeclaration::AstModuleDeclaration(
    const String& name,
    const Array<RC<AstStatement>>& children,
    const SourceLocation& location)
    : AstDeclaration(name, location),
      m_children(children)
{
}

AstModuleDeclaration::AstModuleDeclaration(const String& name, const SourceLocation& location)
    : AstDeclaration(name, location)
{
}

void AstModuleDeclaration::PerformLookup(AstVisitor* visitor)
{
    // make sure this module was not already declared/imported
    if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(m_name))
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_module_already_defined,
            m_location,
            m_name));
    }
    else
    {
        m_module.Reset(new Module(m_name, m_location));
    }
}

void AstModuleDeclaration::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);

    if (m_module == nullptr)
    {
        PerformLookup(visitor);
    }

    if (m_module != nullptr)
    {
        // add this module to the compilation unit
        visitor->GetCompilationUnit()->m_moduleTree.Open(m_module.Get());
        // set the link to the module in the tree
        m_module->SetImportTreeLink(visitor->GetCompilationUnit()->m_moduleTree.TopNode());

        // add this module to list of imported modules,
        // but only if mod == nullptr, that way we don't add nested modules
        if (mod == nullptr)
        {
            // parse filename
            Array<String> path = m_location.GetFileName().Split('\\', '/');
            path = StringUtil::CanonicalizePath(path);
            // change it back to string
            String canonPath = String::Join(path, "/");

            // map filepath to module
            auto it = visitor->GetCompilationUnit()->m_importedModules.Find(canonPath);
            if (it != visitor->GetCompilationUnit()->m_importedModules.End())
            {
                it->second.PushBack(m_module);
            }
            else
            {
                visitor->GetCompilationUnit()->m_importedModules[canonPath.Data()] = { m_module };
            }
        }

        // update current module
        mod = m_module.Get();
        Assert(mod == visitor->GetCompilationUnit()->GetCurrentModule());

        // visit all children
        for (auto& child : m_children)
        {
            if (child != nullptr)
            {
                child->Visit(visitor, mod);
            }
        }

        // close this module
        visitor->GetCompilationUnit()->m_moduleTree.Close();
    }
}

UniquePtr<Buildable> AstModuleDeclaration::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_module != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // build all children
    for (auto& child : m_children)
    {
        if (child != nullptr)
        {
            chunk->Append(child->Build(visitor, m_module.Get()));
        }
    }

    return chunk;
}

void AstModuleDeclaration::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_module != nullptr);

    // optimize all children
    for (auto& child : m_children)
    {
        if (child)
        {
            child->Optimize(visitor, m_module.Get());
        }
    }
}

RC<AstStatement> AstModuleDeclaration::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
