#include <script/compiler/ast/AstAliasDeclaration.hpp>
#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstAliasDeclaration::AstAliasDeclaration(
    const std::string &name,
    const std::shared_ptr<AstExpression> &aliasee,
    const SourceLocation &location)
    : AstDeclaration(name, location),
      m_aliasee(aliasee)
{
}

void AstAliasDeclaration::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_aliasee != nullptr);
    m_aliasee->Visit(visitor, mod);

    AssertThrow(m_aliasee->GetExprType() != nullptr);

    if (mod->LookUpIdentifier(m_name, true) != nullptr) {
        // a collision was found, add an error
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_redeclared_identifier,
            m_location,
            m_name
        ));
    } else {
        Scope &scope = mod->m_scopes.Top();
        /*m_identifier = scope.GetIdentifierTable().AddIdentifier(m_name, FLAG_ALIAS);
        AssertThrow(m_identifier != nullptr);

        m_identifier->SetSymbolType(m_aliasee->GetExprType());
        m_identifier->SetCurrentValue(m_aliasee);*/

        if (AstIdentifier *aliasee_ident = dynamic_cast<AstIdentifier *>(m_aliasee.get())) {
            AssertThrow(aliasee_ident->GetProperties().GetIdentifier() != nullptr);

            m_identifier = scope.GetIdentifierTable().AddAlias(m_name, aliasee_ident->GetProperties().GetIdentifier());
            AssertThrow(m_identifier != nullptr);
        } else {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_WARN,
                Msg_alias_must_be_identifier,
                m_location,
                m_name
            ));

            // work like a mixin
            m_identifier = scope.GetIdentifierTable().AddIdentifier(m_name, FLAG_ALIAS);
            AssertThrow(m_identifier != nullptr);

            m_identifier->SetSymbolType(m_aliasee->GetExprType());
            m_identifier->SetCurrentValue(m_aliasee);
        }
    }
}

std::unique_ptr<Buildable> AstAliasDeclaration::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstAliasDeclaration::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstAliasDeclaration::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
