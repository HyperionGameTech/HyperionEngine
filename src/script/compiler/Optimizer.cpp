#include <script/compiler/Optimizer.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstConstant.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

RC<AstConstant> Optimizer::ConstantFold(
    RC<AstExpression>& left,
    RC<AstExpression>& right,
    Operators opType,
    AstVisitor* visitor)
{
    Assert(left != nullptr);
    Assert(right != nullptr);

    const AstConstant* leftAsConstant = dynamic_cast<const AstConstant*>(left->GetValueOf());
    const AstConstant* rightAsConstant = dynamic_cast<const AstConstant*>(right->GetValueOf());

    RC<AstConstant> result;

    if (leftAsConstant != nullptr && rightAsConstant != nullptr)
    {
        result = leftAsConstant->HandleOperator(opType, rightAsConstant);
        // don't have to worry about assignment operations,
        // because at this point both sides are const and literal.
    }

    // one or both of the sides are not a constant
    return result;
}

RC<AstExpression> Optimizer::OptimizeExpr(
    const RC<AstExpression>& expr,
    AstVisitor* visitor,
    Module* mod)
{
    Assert(expr != nullptr);
    expr->Optimize(visitor, mod);

    if (const AstIdentifier* exprAsIdentifier = dynamic_cast<AstIdentifier*>(expr.Get()))
    {
        // the side is a variable, so we can further optimize by inlining,
        // only if it is literal.
        if (exprAsIdentifier->IsLiteral())
        {
            if (const RC<Identifier>& ident = exprAsIdentifier->GetProperties().GetIdentifier())
            {
                if (const RC<AstExpression>& currentValue = ident->GetCurrentValue())
                {
                    // decrement use count because it would have been incremented by Visit()
                    ident->DecUseCount();
                    return Optimizer::OptimizeExpr(currentValue, visitor, mod);
                }
            }
        }
    }
    else if (const AstBinaryExpression* exprAsBinop = dynamic_cast<AstBinaryExpression*>(expr.Get()))
    {
        if (!exprAsBinop->GetRight())
        {
            // right side has been optimized away
            return Optimizer::OptimizeExpr(exprAsBinop->GetLeft(), visitor, mod);
        }
    }

    return expr;
}

Optimizer::Optimizer(AstIterator* astIterator, CompilationUnit* compilationUnit)
    : AstVisitor(astIterator, compilationUnit)
{
}

Optimizer::Optimizer(const Optimizer& other)
    : AstVisitor(other.m_astIterator, other.m_compilationUnit)
{
}

void Optimizer::Optimize(bool expectModuleDecl)
{
    /*if (expectModuleDecl) {
        if (m_astIterator->HasNext()) {
            RC<AstStatement> firstStmt = m_astIterator->Next();

            if (AstModuleDeclaration *modDecl = dynamic_cast<AstModuleDeclaration*>(firstStmt.Get())) {
                // all files must begin with a module declaration
                modDecl->Optimize(this, nullptr);
                OptimizeInner();
            }
        }
    } else {
        OptimizeInner();
    }*/

    OptimizeInner();
}

void Optimizer::OptimizeInner()
{
    Module* mod = m_compilationUnit->GetCurrentModule();
    Assert(mod != nullptr);

    while (m_astIterator->HasNext())
    {
        m_astIterator->Next()->Optimize(this, mod);
    }
}

} // namespace hyperion
