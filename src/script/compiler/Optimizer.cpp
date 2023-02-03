#include <script/compiler/Optimizer.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstConstant.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

RC<AstConstant> Optimizer::ConstantFold(
    RC<AstExpression> &left,
    RC<AstExpression> &right,
    Operators op_type,
    AstVisitor *visitor
)
{
    AssertThrow(left != nullptr);
    AssertThrow(right != nullptr);

    const AstConstant *left_as_constant  = dynamic_cast<const AstConstant*>(left->GetValueOf());
    const AstConstant *right_as_constant = dynamic_cast<const AstConstant*>(right->GetValueOf());

    RC<AstConstant> result;

    if (left_as_constant != nullptr && right_as_constant != nullptr) {
        result = left_as_constant->HandleOperator(op_type, right_as_constant);
        // don't have to worry about assignment operations,
        // because at this point both sides are const and literal.
    }

    // one or both of the sides are not a constant
    return result;
}

RC<AstExpression> Optimizer::OptimizeExpr(
    const RC<AstExpression> &expr,
    AstVisitor *visitor,
    Module *mod)
{
    AssertThrow(expr != nullptr);
    expr->Optimize(visitor, mod);

    if (const AstIdentifier *expr_as_identifier = dynamic_cast<AstIdentifier *>(expr.Get())) {
        // the side is a variable, so we can further optimize by inlining,
        // only if it is literal.
        if (expr_as_identifier->IsLiteral()) {
            if (const RC<Identifier> &ident = expr_as_identifier->GetProperties().GetIdentifier()) {
                if (const RC<AstExpression> &current_value = ident->GetCurrentValue()) {
                    // decrement use count because it would have been incremented by Visit()
                    ident->DecUseCount();
                    return Optimizer::OptimizeExpr(current_value, visitor, mod);
                }
            }
        }
    } else if (const AstBinaryExpression *expr_as_binop = dynamic_cast<AstBinaryExpression *>(expr.Get())) {
        if (expr_as_binop->GetRight() == nullptr) {
            // right side has been optimized away
            return Optimizer::OptimizeExpr(expr_as_binop->GetLeft(), visitor, mod);
        }
    }

    return expr;
}

Optimizer::Optimizer(AstIterator *ast_iterator, CompilationUnit *compilation_unit)
    : AstVisitor(ast_iterator, compilation_unit)
{
}

Optimizer::Optimizer(const Optimizer &other)
    : AstVisitor(other.m_ast_iterator, other.m_compilation_unit)
{
}

void Optimizer::Optimize(bool expect_module_decl)
{
    /*if (expect_module_decl) {
        if (m_ast_iterator->HasNext()) {
            RC<AstStatement> first_stmt = m_ast_iterator->Next();

            if (AstModuleDeclaration *mod_decl = dynamic_cast<AstModuleDeclaration*>(first_stmt.Get())) {
                // all files must begin with a module declaration
                mod_decl->Optimize(this, nullptr);
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
    Module *mod = m_compilation_unit->GetCurrentModule();
    AssertThrow(mod != nullptr);
    
    while (m_ast_iterator->HasNext()) {
        m_ast_iterator->Next()->Optimize(this, mod);
    }
}

} // namespace hyperion::compiler
