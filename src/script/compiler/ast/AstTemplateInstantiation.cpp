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
    const std::shared_ptr<AstExpression> &expr,
    const std::vector<std::shared_ptr<AstArgument>> &generic_args,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_generic_args(generic_args)
{
}

void AstTemplateInstantiation::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    m_block.reset(new AstBlock({}, m_location));

    ScopeGuard scope(mod, SCOPE_TYPE_GENERIC_INSTANTIATION, 0);

    for (auto &arg : m_generic_args) {
        AssertThrow(arg != nullptr);
        arg->Visit(visitor, mod);
    }

    // visit the expression
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    const auto &expr_type = m_expr->GetExprType();
    AssertThrow(expr_type != nullptr);

    if (expr_type == BuiltinTypes::ANY) { // 'Any' is not usable for generic inst
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expression_not_generic,
            m_expr->GetLocation(),
            expr_type->GetName()
        ));

        return;
    }

    // temporarily define all generic parameters so there are no undefined reference errors.
    const AstExpression *value_of = m_expr->GetValueOf();
    const AstExpression *generic_expr = value_of->GetHeldGenericExpr();

    if (generic_expr == nullptr) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expression_not_generic,
            m_expr->GetLocation(),
            expr_type->GetName()
        ));

        return;
    }

    FunctionTypeSignature_t substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
        visitor, mod,
        expr_type,
        m_generic_args,
        m_location
    );

    m_expr_type = substituted.first;
    const auto args_substituted = substituted.second;

    // there can be more because of varargs
    if (args_substituted.size() + 1 >= expr_type->GetGenericInstanceInfo().m_generic_args.size()) {
        for (SizeType i = 0; i < expr_type->GetGenericInstanceInfo().m_generic_args.size() - 1; i++) {
            AssertThrow(args_substituted[i]->GetExpr() != nullptr);

            if (args_substituted[i]->GetExpr()->GetExprType() != BuiltinTypes::UNDEFINED) {
                std::shared_ptr<AstVariableDeclaration> param_override(new AstVariableDeclaration(
                    // template_expr->GetGenericParameters()[i]->GetName(),
                    expr_type->GetGenericInstanceInfo().m_generic_args[i + 1].m_name,
                    nullptr,
                    CloneAstNode(args_substituted[i]->GetExpr()),
                    {},
                    IdentifierFlags::FLAG_CONST,
                    args_substituted[i]->GetLocation()
                ));

                m_block->AddChild(param_override);
            }
        }

        m_inner_expr = CloneAstNode(generic_expr);

        m_block->AddChild(m_inner_expr);
        m_block->Visit(visitor, mod);

        // TODO: Cache instantiations so we don't create a new one for every set of arguments
    }
}

std::unique_ptr<Buildable> AstTemplateInstantiation::Build(AstVisitor *visitor, Module *mod)
{
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

Pointer<AstStatement> AstTemplateInstantiation::Clone() const
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

const AstExpression *AstTemplateInstantiation::GetValueOf() const
{
    if (m_inner_expr != nullptr) {
        AssertThrow(m_inner_expr.get() != this);

        return m_inner_expr->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression *AstTemplateInstantiation::GetDeepValueOf() const
{
    if (m_inner_expr != nullptr) {
        AssertThrow(m_inner_expr.get() != this);

        return m_inner_expr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
