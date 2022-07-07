#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstModuleAccess::AstModuleAccess(const std::string &target,
    const std::shared_ptr<AstExpression> &expr,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_target(target),
      m_expr(expr),
      m_mod_access(nullptr),
      m_is_chained(false),
      m_looked_up(false)
{
}

void AstModuleAccess::PerformLookup(AstVisitor *visitor, Module *mod)
{
    if (m_is_chained) {
        AssertThrow(mod != nullptr);
        m_mod_access = mod->LookupNestedModule(m_target);
    } else {
        m_mod_access = visitor->GetCompilationUnit()->LookupModule(m_target);
    }

    m_looked_up = true;
}

void AstModuleAccess::Visit(AstVisitor *visitor, Module *mod)
{
    if (!m_looked_up) {
        PerformLookup(visitor, mod);
    }

    if (AstModuleAccess *expr_mod_access = dynamic_cast<AstModuleAccess*>(m_expr.get())) {
        // set expr to be chained
        expr_mod_access->m_is_chained = true;
    }

    // check modules for one with the same name
    if (m_mod_access != nullptr) {
        m_expr->Visit(visitor, m_mod_access);
    } else {
        CompilerError err(LEVEL_ERROR, Msg_unknown_module, m_location, m_target);
        visitor->GetCompilationUnit()->GetErrorList().AddError(err);
    }
}

std::unique_ptr<Buildable> AstModuleAccess::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    AssertThrow(m_mod_access != nullptr);

    m_expr->SetAccessMode(m_access_mode);
    return m_expr->Build(visitor, m_mod_access);
}

void AstModuleAccess::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    AssertThrow(m_mod_access != nullptr);
    m_expr->Optimize(visitor, m_mod_access);
}

Pointer<AstStatement> AstModuleAccess::Clone() const
{
    return CloneImpl();
}

Tribool AstModuleAccess::IsTrue() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->IsTrue();
}

bool AstModuleAccess::MayHaveSideEffects() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypePtr_t AstModuleAccess::GetExprType() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetExprType();
}

const AstExpression *AstModuleAccess::GetValueOf() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetValueOf();
}

const AstExpression *AstModuleAccess::GetDeepValueOf() const
{
    AssertThrow(m_expr != nullptr && m_expr.get() != this);
    return m_expr->GetDeepValueOf();
}

} // namespace hyperion::compiler
