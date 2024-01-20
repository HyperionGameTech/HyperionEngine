#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstTemplateInstantiation::AstTemplateInstantiation(
    const RC<AstExpression> &expr,
    const Array<RC<AstArgument>> &generic_args,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr),
    m_generic_args(generic_args)
{
}

void AstTemplateInstantiation::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(!m_is_visited);
    m_is_visited = true;

    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    m_block.Reset(new AstBlock({}, m_location));

    ScopeGuard scope(mod, SCOPE_TYPE_GENERIC_INSTANTIATION, 0);

    for (auto &arg : m_generic_args) {
        AssertThrow(arg != nullptr);
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }

    // visit the expression
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    // We clone the target expr from the initial expression so that we can use it in the case of
    // generic instantiation of a member function.
    // e.g: `foo.bar<int>()` where `bar` is a generic function. `foo` is the target expression, being passed as the `self` argument
    m_target_expr = CloneAstNode(m_expr->GetTarget());

    if (m_target_expr != nullptr) {
        // Visit it so that it can be analyzed
        m_target_expr->Visit(visitor, mod);
    }

    const SymbolTypePtr_t expr_type = m_expr->GetExprType();
    AssertThrow(expr_type != nullptr);

    const AstExpression *value_of = m_expr->GetDeepValueOf();
    AssertThrow(value_of != nullptr);

    // Check if the expression is a generic expression.
    const AstExpression *generic_expr = value_of->GetHeldGenericExpr();

    if (generic_expr == nullptr) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expression_not_generic,
            m_expr->GetLocation(),
            expr_type->ToString()
        ));

        return;
    }

    FunctionTypeSignature_t substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
        visitor, mod,
        expr_type,
        m_generic_args,
        m_location
    );

    SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
        visitor, mod,
        expr_type,
        m_generic_args,
        m_location
    );

    m_expr_type = substituted.first;
    const auto args_substituted = substituted.second;

    if (m_expr_type == nullptr) {
        // error should occur
        return;
    }
    
    m_inner_expr = CloneAstNode(generic_expr);

    // temporarily define all generic parameters so there are no undefined reference errors.
    for (SizeType index = 0; index < expr_type->GetGenericInstanceInfo().m_generic_args.Size() - 1; index++) {
        AssertThrow(args_substituted[index]->GetExpr() != nullptr);

        if (!args_substituted[index]->GetExpr()->GetExprType()->IsOrHasBase(*BuiltinTypes::UNDEFINED)) {
            RC<AstVariableDeclaration> param_override(new AstVariableDeclaration(
                expr_type->GetGenericInstanceInfo().m_generic_args[index + 1].m_name,
                nullptr,
                CloneAstNode(args_substituted[index]->GetExpr()),
                {},
                IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC_SUBSTITUTION,
                args_substituted[index]->GetLocation()
            ));

            m_block->AddChild(param_override);
        }
    }

    // TODO: Cache instantiations so we don't create a new one for every set of arguments

    AssertThrow(m_inner_expr != nullptr);

    m_block->AddChild(m_inner_expr);
    m_block->Visit(visitor, mod);
    
    AssertThrow(m_inner_expr->GetExprType() != nullptr);

    // If the current return type is a placeholder, we need to set it to the inner expression's implicit return type
    if (m_expr_type->IsPlaceholderType()) {
        m_expr_type = m_inner_expr->GetExprType();
    } else {
        SemanticAnalyzer::Helpers::EnsureLooseTypeAssignmentCompatibility(
            visitor,
            mod,
            m_inner_expr->GetExprType(),
            m_expr_type,
            m_location
        );
    }
}

std::unique_ptr<Buildable> AstTemplateInstantiation::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_block != nullptr);
    return m_block->Build(visitor, mod);
}

void AstTemplateInstantiation::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_block != nullptr);
    m_block->Optimize(visitor, mod);
}

RC<AstStatement> AstTemplateInstantiation::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateInstantiation::IsTrue() const
{
    if (m_inner_expr != nullptr) {
        return m_inner_expr->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstTemplateInstantiation::MayHaveSideEffects() const
{
    for (auto &arg : m_generic_args) {
        AssertThrow(arg != nullptr);

        if (arg->MayHaveSideEffects()) {
            return true;
        }
    }
    
    if (m_inner_expr != nullptr) {
        return m_inner_expr->MayHaveSideEffects();
    }

    return true;
}

SymbolTypePtr_t AstTemplateInstantiation::GetExprType() const
{
    if (m_expr_type != nullptr) {
        return m_expr_type;
    }

    return BuiltinTypes::UNDEFINED;
}

SymbolTypePtr_t AstTemplateInstantiation::GetHeldType() const
{
    if (m_inner_expr != nullptr) {
        return m_inner_expr->GetHeldType();
    }

    return AstExpression::GetHeldType();
}

const AstExpression *AstTemplateInstantiation::GetValueOf() const
{
    if (m_inner_expr != nullptr) {
        return m_inner_expr.Get();
        // AssertThrow(m_inner_expr.Get() != this);

        // return m_inner_expr->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression *AstTemplateInstantiation::GetDeepValueOf() const
{
    if (m_inner_expr != nullptr) {
        AssertThrow(m_inner_expr.Get() != this);

        return m_inner_expr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

AstExpression *AstTemplateInstantiation::GetTarget() const
{
    if (m_target_expr != nullptr) {
        AssertThrow(m_target_expr.Get() != this);

        return m_target_expr.Get();
    }

    return AstExpression::GetTarget();
}

} // namespace hyperion::compiler
