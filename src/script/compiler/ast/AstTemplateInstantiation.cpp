#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
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

// AstTemplateInstantiationWrapper
AstTemplateInstantiationWrapper::AstTemplateInstantiationWrapper(
    const RC<AstExpression> &expr,
    const Array<GenericInstanceTypeInfo::Arg> &generic_args,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr),
    m_generic_args(generic_args)
{
}

void AstTemplateInstantiationWrapper::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);

    m_expr->Visit(visitor, mod);

    auto *value_of = m_expr->GetDeepValueOf();
    AssertThrow(value_of != nullptr);

    m_expr_type = value_of->GetExprType();
    AssertThrow(m_expr_type != nullptr);
    m_expr_type = m_expr_type->GetUnaliased();

    m_held_type = value_of->GetHeldType();

    if (m_held_type != nullptr) {
        m_held_type = m_held_type->GetUnaliased();

        MakeSymbolTypeGenericInstance(m_held_type);
    }
}

void AstTemplateInstantiationWrapper::MakeSymbolTypeGenericInstance(SymbolTypePtr_t &symbol_type)
{
    // Set it to a GenericInstance version,
    // So we can use the params that were provided, later
    if (symbol_type != BuiltinTypes::UNDEFINED) {
        const int current_symbol_type_id = symbol_type->GetId();
        AssertThrow(current_symbol_type_id != -1);

        const RC<AstTypeObject> current_type_object = symbol_type->GetTypeObject().Lock();
        AssertThrow(current_type_object != nullptr);

        const SymbolTypeFlags current_flags = symbol_type->GetFlags();

        symbol_type = SymbolType::GenericInstance(
            symbol_type,
            GenericInstanceTypeInfo {
                m_generic_args
            }
        );

        // Reuse the same ID
        symbol_type->SetId(current_symbol_type_id);
        symbol_type->SetTypeObject(current_type_object);
        symbol_type->SetFlags(current_flags);
    }
}

std::unique_ptr<Buildable> AstTemplateInstantiationWrapper::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);

    return m_expr->Build(visitor, mod);
}

void AstTemplateInstantiationWrapper::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);

    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstTemplateInstantiationWrapper::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateInstantiationWrapper::IsTrue() const
{
    if (m_expr != nullptr) {
        return m_expr->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstTemplateInstantiationWrapper::MayHaveSideEffects() const
{
    if (m_expr != nullptr) {
        return m_expr->MayHaveSideEffects();
    }

    return true;
}

SymbolTypePtr_t AstTemplateInstantiationWrapper::GetExprType() const
{
    if (m_expr_type != nullptr) {
        return m_expr_type;
    }

    return BuiltinTypes::UNDEFINED;
}

SymbolTypePtr_t AstTemplateInstantiationWrapper::GetHeldType() const
{
    if (m_held_type != nullptr) {
        return m_held_type;
    }

    return AstExpression::GetHeldType();
}

const AstExpression *AstTemplateInstantiationWrapper::GetValueOf() const
{
    // do not return the inner expression - we want to keep the wrapper
    return AstExpression::GetValueOf();
}

const AstExpression *AstTemplateInstantiationWrapper::GetDeepValueOf() const
{
    // do not return the inner expression - we want to keep the wrapper
    return AstExpression::GetDeepValueOf();
}

// AstTemplateInstantiation

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

        // if (arg->MayHaveSideEffects()) {
        //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        //         LEVEL_ERROR,
        //         Msg_generic_arg_may_not_have_side_effects,
        //         arg->GetLocation()
        //     ));
        // }
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

    const AstExpression *value_of = m_expr->GetDeepValueOf();
    AssertThrow(value_of != nullptr);

    SymbolTypePtr_t expr_type = m_expr->GetExprType();
    AssertThrow(expr_type != nullptr);
    expr_type = expr_type->GetUnaliased();

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

    AssertThrow(expr_type != nullptr);

    Optional<SymbolTypeFunctionSignature> substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
        visitor, mod,
        expr_type,
        m_generic_args,
        m_location
    );

    if (!substituted.HasValue()) {
        // not a generic if it doesnt resolve
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_type_not_generic,
            m_location,
            expr_type->ToString()
        ));

        return;
    }

    SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
        visitor, mod,
        expr_type,
        m_generic_args,
        m_location
    );

    AssertThrow(substituted->return_type != nullptr);

    m_expr_type = substituted->return_type;
    m_substituted_args = substituted->params;

    AssertThrow(m_expr_type != nullptr);

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

    const Array<GenericInstanceTypeInfo::Arg> &params = expr_type->GetGenericInstanceInfo().m_generic_args;
    AssertThrow(params.Size() >= 1);

    Array<GenericInstanceTypeInfo::Arg> args;
    args.Resize(m_substituted_args.Size());

    // temporarily define all generic parameters as constants
    for (SizeType index = 0; index < m_substituted_args.Size(); index++) {
        RC<AstArgument> &arg = m_substituted_args[index];
        AssertThrow(arg != nullptr);
        
        const AstExpression *value_of = arg->GetDeepValueOf();
        AssertThrow(value_of != nullptr);

        SymbolTypePtr_t member_expr_type = value_of->GetExprType();
        AssertThrow(member_expr_type != nullptr);
        member_expr_type = member_expr_type->GetUnaliased();

        // For each generic parameter, we add a new symbol type to the current scope
        // as an alias to the held type of the argument.

        SymbolTypePtr_t held_type = value_of->GetHeldType();

        if (held_type == nullptr) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_type,
                arg->GetLocation(),
                member_expr_type->ToString()
            ));

            continue;
        }

        held_type = held_type->GetUnaliased();

        String param_name;

        if (index + 1 < params.Size()) {
            param_name = params[index + 1].m_name;
        } else {
            // if there are more arguments than generic parameters, we just use a generic name
            param_name = params.Back().m_name + String::ToString(index);
        }

        args[index] = GenericInstanceTypeInfo::Arg {
            param_name,
            held_type
        };

        scope->GetIdentifierTable().AddSymbolType(SymbolType::Alias(
            param_name,
            { held_type }
        ));

        RC<AstVariableDeclaration> param_override(new AstVariableDeclaration(
            param_name,
            nullptr,
            RC<AstTypeRef>(new AstTypeRef(
                held_type,
                SourceLocation::eof
            )),
            IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC_SUBSTITUTION,
            arg->GetLocation()
        ));

        m_block->AddChild(param_override);
    }

    // Set up the expression wrapper
    m_inner_expr.Reset(new AstTemplateInstantiationWrapper(
        CloneAstNode(generic_expr),
        args,
        m_location
    ));

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
    
    m_held_type = m_inner_expr->GetHeldType();

    // If expr type is native just load the original type.
    if (expr_type->GetFlags() & SYMBOL_TYPE_FLAGS_NATIVE) {
        m_is_native = true;

        m_block.Reset(new AstBlock({}, m_location));

        if (m_held_type != nullptr) {
            AssertThrowMsg(m_held_type->GetId() != -1,
                "For native generic types, the original generic type must be registered");

            // Add a type ref to the original type
            m_block->AddChild(RC<AstTypeRef>(new AstTypeRef(
                m_held_type,
                m_location
            )));
        } else {
            m_block->AddChild(CloneAstNode(m_expr));
        }

        m_block->Visit(visitor, mod);
    }
}

std::unique_ptr<Buildable> AstTemplateInstantiation::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    auto chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_held_type != nullptr) {
        chunk->Append(BytecodeUtil::Make<Comment>(String("Begin generic instantiation for type `" ) + m_held_type->ToString() + String("`")));
    } else {
        AssertThrow(m_expr_type != nullptr);

        chunk->Append(BytecodeUtil::Make<Comment>(String("Begin generic instantiation for expression of type `" ) + m_expr_type->ToString() + String("`")));
    }

    AssertThrow(m_block != nullptr);
    chunk->Append(m_block->Build(visitor, mod));

    if (m_held_type != nullptr) {
        chunk->Append(BytecodeUtil::Make<Comment>(String("End generic instantiation for type `" ) + m_held_type->ToString() + String("`")));
    } else {
        AssertThrow(m_expr_type != nullptr);

        chunk->Append(BytecodeUtil::Make<Comment>(String("End generic instantiation for expression of type `" ) + m_expr_type->ToString() + String("`")));
    }

    return chunk;


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
    if (m_held_type) {
        return m_held_type;
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
        AssertThrow(m_inner_expr->GetExpr().Get() != this);

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
