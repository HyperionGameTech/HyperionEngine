#include <script/compiler/ast/AstTypeAlias.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

AstTypeAlias::AstTypeAlias(
    const String &name,
    const RC<AstPrototypeSpecification> &aliasee,
    const SourceLocation &location
) : AstStatement(location),
    m_name(name),
    m_aliasee(aliasee)
{
}

void AstTypeAlias::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr && mod != nullptr);
    AssertThrow(m_aliasee != nullptr);

    m_aliasee->Visit(visitor, mod);

    SymbolTypePtr_t aliasee_type = m_aliasee->GetHeldType();
    AssertThrow(aliasee_type != nullptr);
    aliasee_type = aliasee_type->GetUnaliased();

    // make sure name isn't already defined
    if (mod->LookupSymbolType(m_name)) {
        // error; redeclaration of type in module
        visitor->GetCompilationUnit()->GetErrorList().AddError(
            CompilerError(
                LEVEL_ERROR,
                Msg_redefined_type,
                m_location,
                m_name
            )
        );
    } else {
        SymbolTypePtr_t alias_type = SymbolType::Alias(
            m_name, { aliasee_type }
        );

        // add it
        mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(alias_type);
    }
}

std::unique_ptr<Buildable> AstTypeAlias::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstTypeAlias::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstTypeAlias::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
