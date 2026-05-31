#include <Lang/Compiler/Ast/AstDeclaration.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>

namespace Hyperion {

AstDeclaration::AstDeclaration(
    const String& name,
    EnumFlags<IdentifierFlags> flags,
    const SourceLocation& location)
    : AstStatement(location),
      m_name(name),
      m_identifier(nullptr),
      m_flags(flags)
{
}

void AstDeclaration::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    CompilationUnit* compilationUnit = visitor->GetCompilationUnit();
    Scope& scope = mod->scopeTree.Top();

    // look up variable to make sure it doesn't already exist
    // only this scope matters, variables with the same name outside
    // of this scope are fine

    if (RC<Identifier> existingLocalIdentifier = mod->LookUpIdentifier(m_name, true))
    {
        // a collision was found, add an error
        compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_redeclared_identifier,
            m_location,
            m_name));

        // redirect identifier to be the existing one, as we don't expect identifier to be nullptr later on
        m_identifier = std::move(existingLocalIdentifier);

        return;
    }

    const bool skipShadowingCheck = m_name.StartsWith("$")
        || (m_flags & IdentifierFlags::LAX)
        || mod->IsInScopeOfType(SCOPE_TYPE_CLASS_DEFINITION, EXTERN_CLASS_FLAG, /* thisScopeOnly */ false);

    if (!skipShadowingCheck)
    {
        if (RC<Identifier> shadowedIdentifier = mod->LookUpIdentifier(m_name, false))
        {
            // allow shadowing only if the found identifier is in global scope
            if (shadowedIdentifier->GetDeclScope() != &mod->scopeTree.Root())
            {
                // a collision was found, add an error, but continue evaluating as if no error.
                compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_shadowing_identifier,
                    m_location,
                    m_name));
            }
        }
    }

    // add identifier
    m_identifier = scope.identifierTable.AddIdentifier(m_name);

    TreeNode<Scope>* top = mod->scopeTree.TopNode();

    while (top != nullptr)
    {
        if (top->Get().scopeType == SCOPE_TYPE_FUNCTION)
        {
            // set declared in function flag
            m_identifier->GetFlags() |= IdentifierFlags::DECLARED_IN_FUNCTION;
            break;
        }

        top = top->m_parent;
    }
}

const String& AstDeclaration::GetName() const
{
    return m_name;
}

} // namespace Hyperion
