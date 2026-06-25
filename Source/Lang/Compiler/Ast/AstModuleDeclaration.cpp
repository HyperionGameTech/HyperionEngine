#include <Lang/Compiler/Ast/AstModuleDeclaration.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstClass.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Core/Debug/Debug.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <AstModuleDeclaration.generated.inl>

namespace Hyperion {

AstModuleDeclaration::AstModuleDeclaration(
    const String& name,
    const Array<RC<AstStatement>>& children,
    const SourceLocation& location)
    : AstDeclaration(name, IdentifierFlags::MODULE, location),
      m_children(children),
      m_module(nullptr)
{
}

AstModuleDeclaration::AstModuleDeclaration(const String& name, const SourceLocation& location)
    : AstDeclaration(name, IdentifierFlags::MODULE, location),
      m_module(nullptr)
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
        Assert(m_module == nullptr);
        m_module = new Module(m_name, m_location);

        visitor->GetCompilationUnit()->ownedModules.PushBack(m_module);
    }
}

void AstModuleDeclaration::PreRegisterClassTypes(AstVisitor* visitor, Module* mod)
{
    for (const RC<AstStatement>& child : m_children)
    {
        Assert(child != nullptr);

        if (AstClass* classNode = DynamicCast<AstClass>(child.Get()))
        {
            classNode->SetPreRegister(true);

            classNode->Visit(visitor, mod);

            classNode->SetPreRegister(false);
        }
    }
}

void AstModuleDeclaration::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);

    if (!m_module)
    {
        PerformLookup(visitor);
    }

    if (m_module != nullptr)
    {
        // add this module to the compilation unit (will take ownership over deleting it)
        visitor->GetCompilationUnit()->moduleTree.Open(m_module);
        // set the link to the module in the tree
        m_module->SetImportTreeLink(visitor->GetCompilationUnit()->moduleTree.TopNode());

        // add this module to list of imported modules,
        // but only if mod == nullptr, that way we don't add nested modules
        if (!mod)
        {
            // parse filename
            Array<String> path = m_location.GetFileName().Split('\\', '/');
            path = StringUtil::CanonicalizePath(path);
            // change it back to string
            String canonPath = String::Join(path, "/");

            // map filepath to module
            auto it = visitor->GetCompilationUnit()->importedModules.Find(canonPath);
            if (it != visitor->GetCompilationUnit()->importedModules.End())
            {
                it->second.PushBack(m_module);
            }
            else
            {
                visitor->GetCompilationUnit()->importedModules[canonPath.Data()] = { m_module };
            }
        }

        // update current module
        mod = m_module;

        // Pre-register all class types so forward references work
        PreRegisterClassTypes(visitor, mod);

        // visit all children
        for (const RC<AstStatement>& child : m_children)
        {
            Assert(child != nullptr);

            child->Visit(visitor, mod);
        }

        // close this module
        visitor->GetCompilationUnit()->moduleTree.Close();
    }
}

UniquePtr<Buildable> AstModuleDeclaration::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_module != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // build all children
    for (const RC<AstStatement>& child : m_children)
    {
        if (child != nullptr)
        {
            chunk->Append(child->Build(visitor, m_module));

            Compiler::MaybeAutoExport(visitor, child.Get(), chunk);
        }
    }

    return chunk;
}

void AstModuleDeclaration::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_module != nullptr);

    // optimize all children
    for (const RC<AstStatement>& child : m_children)
    {
        if (child)
        {
            child->Optimize(visitor, m_module);
        }
    }
}

RC<AstStatement> AstModuleDeclaration::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
