#include <script/compiler/ast/AstTypeOfExpression.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/debug.h>

namespace hyperion::compiler {

AstTypeOfExpression::AstTypeOfExpression(
    const std::shared_ptr<AstExpression> &expr,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_static_id(0)
{
}

void AstTypeOfExpression::Visit(AstVisitor *visitor, Module *mod)
{
     AssertThrow(m_expr != nullptr);
     m_expr->Visit(visitor, mod);
    
     SymbolTypePtr_t expr_type = m_expr->GetExprType();
     AssertThrow(expr_type != nullptr);

     m_string_expr.reset(new AstString(
         expr_type->GetName(),
         m_location
     ));
}

std::unique_ptr<Buildable> AstTypeOfExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_string_expr != nullptr);
    return m_string_expr->Build(visitor, mod);
}

void AstTypeOfExpression::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstTypeOfExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstTypeOfExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeOfExpression::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstTypeOfExpression::GetExprType() const
{
    AssertThrow(m_expr != nullptr);
    return BuiltinTypes::STRING;
}

} // namespace hyperion::compiler
