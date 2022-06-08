#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

namespace hyperion {
namespace compiler {

AstArgument::AstArgument(
    const std::shared_ptr<AstExpression> &expr,
    bool is_named,
    const std::string &name,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_is_named(is_named),
      m_name(name)
{
}

void AstArgument::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstArgument::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    return m_expr->Build(visitor, mod);
}

void AstArgument::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

Pointer<AstStatement> AstArgument::Clone() const
{
    return CloneImpl();
}

Tribool AstArgument::IsTrue() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->IsTrue();
}

bool AstArgument::MayHaveSideEffects() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypePtr_t AstArgument::GetSymbolType() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetSymbolType();
}

} // namespace compiler
} // namespace hyperion
