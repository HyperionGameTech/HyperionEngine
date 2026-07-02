#include <Lang/Compiler/Ast/AstTypeAlias.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>

#include <Core/Debug/Debug.hpp>
#include <Core/Unicode.hpp>

#include <AstTypeAlias.generated.inl>

namespace Hyperion {

AstTypeAlias::AstTypeAlias(
    const String& name,
    const SharedPtr<AstTypeSpecifier>& aliasee,
    const SourceLocation& location)
    : AstStatement(location),
      m_name(name),
      m_aliasee(aliasee)
{
}

void AstTypeAlias::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr && mod != nullptr);
    Assert(m_aliasee != nullptr);

    m_aliasee->Visit(visitor, mod);

    const SymbolType* aliaseeType = m_aliasee->GetHeldType();
    Assert(aliaseeType != nullptr);
    aliaseeType = aliaseeType->GetUnaliased();

    // make sure name isn't already defined
    if (mod->LookupSymbolType(m_name))
    {
        // error; redeclaration of type in module
        visitor->GetCompilationUnit()->GetErrorList().AddError(
            CompilerError(
                LEVEL_ERROR,
                Msg_redefined_type,
                m_location,
                m_name));
    }
    else
    {
        SymbolType* aliasType = SymbolType::Alias(m_name, { aliaseeType });
        aliasType->Register(visitor->GetCompilationUnit());
        mod->scopeTree.Top().identifierTable.AddSymbolType(aliasType);
    }
}

UniquePtr<Buildable> AstTypeAlias::Build(AstVisitor* visitor, Module* mod)
{
    return nullptr;
}

void AstTypeAlias::Optimize(AstVisitor* visitor, Module* mod)
{
}

SharedPtr<AstStatement> AstTypeAlias::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
