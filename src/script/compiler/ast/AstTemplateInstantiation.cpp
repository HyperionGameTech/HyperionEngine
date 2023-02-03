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
    const std::shared_ptr<AstIdentifier> &expr,
    const std::vector<std::shared_ptr<AstArgument>> &generic_args,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr),
    m_generic_args(generic_args)
{
}

void AstTemplateInstantiation::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    // TODO: Declare a variable with some mangled name,
    // use it to look up to see if it is already instantiated.
    // it will be declared in the same scope as the original generic.

    m_block.reset(new AstBlock({}, m_location));

    ScopeGuard scope(mod, SCOPE_TYPE_GENERIC_INSTANTIATION, 0);

    for (auto &arg : m_generic_args) {
        AssertThrow(arg != nullptr);
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }

    // visit the expression
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    const auto &expr_type = m_expr->GetExprType();
    AssertThrow(expr_type != nullptr);

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

    // Create mangled name

    // std::string mangled_name = "__$" + m_expr->GetName();

    // for (const auto &arg : args_substituted) {
    //     AssertThrow(arg != nullptr);

    //     const std::string &name = arg->GetName();

    //     if (name.empty()) {
    //         mangled_name += ";invalid";

    //         visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
    //             LEVEL_ERROR,
    //             Msg_generic_argument_must_be_literal,
    //             arg->GetLocation()
    //         ));
    //     } else {
    //         mangled_name += ";" + name;
    //     }
    // }
    
    // Scope *original_scope = m_expr->GetProperties().m_function_scope;
    // AssertThrow(original_scope != nullptr);

    // lookup identifier by mangled name to see if it exists already

    // if (auto identifier = mod->LookUpIdentifier(mangled_name, false, true)) {
    //     // we can use it by adding a variable node with the name of the mangled name

    //     // @TODO: Reopen existing scope to use?

    //     m_inner_expr.reset(new AstVariable(
    //         mangled_name,
    //         m_location
    //     ));

    // } else {


        // first instantiation, have to visit
        m_inner_expr = CloneAstNode(generic_expr);

    // there can be more because of varargs
    // if (args_substituted.size() + 1 >= expr_type->GetGenericInstanceInfo().m_generic_args.size()) {
        for (SizeType i = 0; i < expr_type->GetGenericInstanceInfo().m_generic_args.size() - 1; i++) {
            AssertThrow(args_substituted[i]->GetExpr() != nullptr);

            if (args_substituted[i]->GetExpr()->GetExprType() != BuiltinTypes::UNDEFINED) {
                std::shared_ptr<AstVariableDeclaration> param_override(new AstVariableDeclaration(
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

        // TODO: Cache instantiations so we don't create a new one for every set of arguments
    // }
    // }

    AssertThrow(m_inner_expr != nullptr);

    m_block->AddChild(m_inner_expr);
    m_block->Visit(visitor, mod);
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
