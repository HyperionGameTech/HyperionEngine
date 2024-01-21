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
#include <script/compiler/emit/StorageOperation.hpp>

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

    bool any_args_placeholder = false;

    for (auto &arg : m_generic_args) {
        AssertThrow(arg != nullptr);
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());

        auto arg_type = arg->GetExprType();
        AssertThrow(arg_type != nullptr);

        if (arg_type->IsPlaceholderType()) {
            any_args_placeholder = true;
        }

        if (arg->MayHaveSideEffects()) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_generic_arg_may_not_have_side_effects,
                arg->GetLocation()
            ));
        }
    }

    // if (any_args_placeholder) {
    //     m_expr_type = BuiltinTypes::PLACEHOLDER;

    //     return;
    // }

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

    SymbolTypePtr_t expr_type = m_expr->GetExprType();
    AssertThrow(expr_type != nullptr);
    expr_type = expr_type->GetUnaliased();

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
    m_substituted_args = substituted.second;

    if (m_expr_type == nullptr) {
        // error should occur
        return;
    }

    GenericInstanceCache::Key generic_instance_cache_key;
    // { // look in generic instance cache
    //     generic_instance_cache_key.generic_expr = CloneAstNode(generic_expr);
    //     generic_instance_cache_key.arg_hash_codes.Resize(m_substituted_args.Size());

    //     for (SizeType index = 0; index < m_substituted_args.Size(); index++) {
    //         const auto &arg = m_substituted_args[index];
    //         AssertThrow(arg != nullptr);

    //         generic_instance_cache_key.arg_hash_codes[index] = arg->GetHashCode();
    //     }

    //     Optional<GenericInstanceCache::CachedObject> cached_object = mod->LookupGenericInstance(generic_instance_cache_key);

    //     if (cached_object.HasValue()) {
    //         m_inner_expr = cached_object.Get().instantiated_expr;
    //         AssertThrow(m_inner_expr != nullptr);

    //         return;
    //     }
    // }
    
    m_inner_expr = CloneAstNode(generic_expr);

    const auto &params = expr_type->GetGenericInstanceInfo().m_generic_args;
    AssertThrow(params.Size() >= 1);

    AssertThrow(m_substituted_args.Size() >= params.Size() - 1);

    // temporarily define all generic parameters as constants
    for (SizeType index = 0; index < params.Size() - 1; index++) {
        auto &arg = m_substituted_args[index];
        const auto &param = params[index + 1];

        AssertThrow(arg != nullptr);
        
        auto *value_of = arg->GetDeepValueOf();
        AssertThrow(value_of != nullptr);

        // For each generic parameter, we add a new symbol type to the current scope
        // as an alias to the held type of the argument.

        SymbolTypePtr_t held_type = value_of->GetHeldType();
        AssertThrow(held_type != nullptr);

        scope->GetIdentifierTable().AddSymbolType(SymbolType::Alias(
            param.m_name,
            { held_type }
        ));

        RC<AstVariableDeclaration> param_override(new AstVariableDeclaration(
            param.m_name,
            nullptr,
            CloneAstNode(arg->GetExpr()),
            IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC_SUBSTITUTION,
            arg->GetLocation()
        ));

        m_block->AddChild(param_override);
    }

    // TODO: Cache instantiations so we don't create a new one for every set of arguments

    AssertThrow(m_inner_expr != nullptr);

    m_block->AddChild(m_inner_expr);
    m_block->Visit(visitor, mod);
    
    AssertThrow(m_inner_expr->GetExprType() != nullptr);

    // If the current return type is a placeholder, we need to set it to the inner expression's implicit return type
    if (m_expr_type->IsPlaceholderType()) {
        m_expr_type = m_inner_expr->GetExprType();
        m_expr_type = m_expr_type->GetUnaliased();
    } else {
        SemanticAnalyzer::Helpers::EnsureLooseTypeAssignmentCompatibility(
            visitor,
            mod,
            m_inner_expr->GetExprType(),
            m_expr_type,
            m_location
        );
    }

    // // @TODO Will this work with other modules?

    // const UInt expr_scope_depth = generic_expr->GetScopeDepth();

    // // add it to the generic instance cache, for the scope of the original generic expression
    // Scope *expr_scope = visitor->GetCompilationUnit()->GetCurrentModule()->m_scopes.FindClosestMatch(
    //     [expr_scope_depth](const TreeNode<Scope> *node, const Scope &scope) 
    //     {
    //         return node->m_depth == expr_scope_depth;
    //     }
    // );

    // AssertThrow(expr_scope != nullptr);

    // expr_scope->GetGenericInstanceCache().Add(generic_instance_cache_key, m_inner_expr);

    // m_is_new_instantiation = true;
}

std::unique_ptr<Buildable> AstTemplateInstantiation::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_block != nullptr);
    return m_block->Build(visitor, mod);


    // std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // // Build the arguments
    // chunk->Append(Compiler::BuildArgumentsStart(
    //     visitor,
    //     mod,
    //     m_substituted_args
    // ));

    // const Int stack_size_before = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    
    // // Build the original expression.
    // // Usage of arguments in the expression will be replaced with the substituted arguments.
    // chunk->Append(m_expr->Build(visitor, mod));

    // const Int stack_size_now = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // AssertThrowMsg(
    //     stack_size_now == stack_size_before,
    //     "Stack size mismatch detected! Internal record of stack does not match. (%d != %d)",
    //     stack_size_now,
    //     stack_size_before
    // );

    // chunk->Append(Compiler::BuildArgumentsEnd(
    //     visitor,
    //     mod,
    //     m_substituted_args.Size()
    // ));

    // return chunk;
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
