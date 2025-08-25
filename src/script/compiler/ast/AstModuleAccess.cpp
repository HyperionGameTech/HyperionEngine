#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstModuleAccess::AstModuleAccess(
    const String& target,
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_target(target),
      m_expr(expr),
      m_modAccess(nullptr),
      m_isChained(false),
      m_lookedUp(false)
{
}

void AstModuleAccess::PerformLookup(AstVisitor* visitor, Module* mod)
{
    if (m_isChained)
    {
        Assert(mod != nullptr);
        m_modAccess = mod->LookupNestedModule(m_target);
    }
    else
    {
        m_modAccess = visitor->GetCompilationUnit()->LookupModule(m_target);
    }

    m_lookedUp = true;
}

void AstModuleAccess::Visit(AstVisitor* visitor, Module* mod)
{
    if (!m_lookedUp)
    {
        PerformLookup(visitor, mod);
    }

    if (AstModuleAccess* exprModAccess = dynamic_cast<AstModuleAccess*>(m_expr.Get()))
    {
        // set expr to be chained
        exprModAccess->m_isChained = true;
    }

    // check modules for one with the same name
    if (m_modAccess != nullptr)
    {
        m_expr->Visit(visitor, m_modAccess);
    }
    else
    {
        CompilerError err(LEVEL_ERROR, Msg_unknown_module, m_location, m_target);
        visitor->GetCompilationUnit()->GetErrorList().AddError(err);
    }
}

UniquePtr<Buildable> AstModuleAccess::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    Assert(m_modAccess != nullptr);

    m_expr->SetAccessMode(m_accessMode);
    return m_expr->Build(visitor, m_modAccess);
}

void AstModuleAccess::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    Assert(m_modAccess != nullptr);
    m_expr->Optimize(visitor, m_modAccess);
}

RC<AstStatement> AstModuleAccess::Clone() const
{
    return CloneImpl();
}

Tribool AstModuleAccess::IsTrue() const
{
    Assert(m_expr != nullptr);
    return m_expr->IsTrue();
}

bool AstModuleAccess::MayHaveSideEffects() const
{
    Assert(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypeRef AstModuleAccess::GetExprType() const
{
    Assert(m_expr != nullptr);
    return m_expr->GetExprType();
}

const AstExpression* AstModuleAccess::GetValueOf() const
{
    Assert(m_expr != nullptr);
    return m_expr->GetValueOf();
}

const AstExpression* AstModuleAccess::GetDeepValueOf() const
{
    Assert(m_expr != nullptr && m_expr.Get() != this);
    return m_expr->GetDeepValueOf();
}

AstExpression* AstModuleAccess::GetTarget() const
{
    return AstExpression::GetTarget();
}

bool AstModuleAccess::IsMutable() const
{
    Assert(m_expr != nullptr && m_expr.Get() != this);
    return m_expr->IsMutable();
}

bool AstModuleAccess::IsLiteral() const
{
    Assert(m_expr != nullptr && m_expr.Get() != this);
    return m_expr->IsLiteral();
}

} // namespace hyperion::compiler
