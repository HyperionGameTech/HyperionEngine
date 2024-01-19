#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>

namespace hyperion::compiler {

AstDeclaration::AstDeclaration(
    const String &name,
    const SourceLocation &location
) : AstStatement(location),
    m_name(name),
    m_identifier(nullptr)
{
}

void AstDeclaration::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(!m_is_visited);
    m_is_visited = true;

    CompilationUnit *compilation_unit = visitor->GetCompilationUnit();
    Scope &scope = mod->m_scopes.Top();

    // look up variable to make sure it doesn't already exist
    // only this scope matters, variables with the same name outside
    // of this scope are fine

    if ((m_identifier = mod->LookUpIdentifier(m_name, true))) {
        // a collision was found, add an error
        compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_redeclared_identifier,
            m_location,
            m_name
        ));
    } else {
        // add identifier
        m_identifier = scope.GetIdentifierTable().AddIdentifier(m_name);

        TreeNode<Scope> *top = mod->m_scopes.TopNode();

        while (top != nullptr) {
            if (top->Get().GetScopeType() == SCOPE_TYPE_FUNCTION) {
                // set declared in function flag
                m_identifier->GetFlags() |= FLAG_DECLARED_IN_FUNCTION;
                break;
            }

            top = top->m_parent;
        }
    }
}

const String &AstDeclaration::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler
