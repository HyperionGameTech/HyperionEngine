#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
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

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

// AstTemplateInstantiationWrapper
AstTemplateInstantiationWrapper::AstTemplateInstantiationWrapper(
    const RC<AstExpression>& expr,
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_genericArgs(genericArgs)
{
}

void AstTemplateInstantiationWrapper::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);

    m_expr->Visit(visitor, mod);

    auto* valueOf = m_expr->GetDeepValueOf();
    Assert(valueOf != nullptr);

    m_exprType = valueOf->GetExprType();
    Assert(m_exprType != nullptr);
    m_exprType = m_exprType->GetUnaliased();

    m_heldType = valueOf->GetHeldType();

    if (m_heldType != nullptr)
    {
        m_heldType = m_heldType->GetUnaliased();

        MakeSymbolTypeGenericInstance(m_heldType);
    }
}

void AstTemplateInstantiationWrapper::MakeSymbolTypeGenericInstance(SymbolTypeRef& symbolType)
{
    // Set it to a GenericInstance version,
    // So we can use the params that were provided, later
    if (symbolType != BuiltinTypes::UNDEFINED)
    {
        // const int currentSymbolTypeId = symbolType->GetId();
        // Assert(currentSymbolTypeId != -1);

        const SymbolTypeFlags currentFlags = symbolType->GetFlags();

        symbolType = SymbolType::GenericInstance(
            symbolType,
            GenericInstanceTypeInfo {
                m_genericArgs });

        // Reuse the same ID
        // symbolType->SetId(currentSymbolTypeId);
        symbolType->SetFlags(currentFlags);
    }
}

UniquePtr<Buildable> AstTemplateInstantiationWrapper::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);

    return m_expr->Build(visitor, mod);
}

void AstTemplateInstantiationWrapper::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);

    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstTemplateInstantiationWrapper::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateInstantiationWrapper::IsTrue() const
{
    if (m_expr != nullptr)
    {
        return m_expr->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstTemplateInstantiationWrapper::MayHaveSideEffects() const
{
    if (m_expr != nullptr)
    {
        return m_expr->MayHaveSideEffects();
    }

    return true;
}

SymbolTypeRef AstTemplateInstantiationWrapper::GetExprType() const
{
    if (m_exprType != nullptr)
    {
        return m_exprType;
    }

    return BuiltinTypes::UNDEFINED;
}

SymbolTypeRef AstTemplateInstantiationWrapper::GetHeldType() const
{
    if (m_heldType != nullptr)
    {
        return m_heldType;
    }

    return AstExpression::GetHeldType();
}

const AstExpression* AstTemplateInstantiationWrapper::GetValueOf() const
{
    // do not return the inner expression - we want to keep the wrapper
    return AstExpression::GetValueOf();
}

const AstExpression* AstTemplateInstantiationWrapper::GetDeepValueOf() const
{
    // do not return the inner expression - we want to keep the wrapper
    return AstExpression::GetDeepValueOf();
}

// AstTemplateInstantiation

AstTemplateInstantiation::AstTemplateInstantiation(
    const RC<AstExpression>& expr,
    const Array<RC<AstArgument>>& genericArgs,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_genericArgs(genericArgs)
{
}

void AstTemplateInstantiation::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(!m_isVisited);
    m_isVisited = true;

    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    m_block.Reset(new AstBlock({}, m_location));

    ScopeGuard scope(mod, SCOPE_TYPE_GENERIC_INSTANTIATION, 0);

    bool anyArgsPlaceholder = false;

    for (auto& arg : m_genericArgs)
    {
        Assert(arg != nullptr);
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());

        auto argType = arg->GetExprType();
        Assert(argType != nullptr);

        if (argType->IsPlaceholderType())
        {
            anyArgsPlaceholder = true;
        }

        // if (arg->MayHaveSideEffects()) {
        //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        //         LEVEL_ERROR,
        //         Msg_generic_arg_may_not_have_side_effects,
        //         arg->GetLocation()
        //     ));
        // }
    }

    // if (anyArgsPlaceholder) {
    //     m_exprType = BuiltinTypes::PLACEHOLDER;

    //     return;
    // }

    // visit the expression
    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    // We clone the target expr from the initial expression so that we can use it in the case of
    // generic instantiation of a member function.
    // e.g: `foo.bar<int>()` where `bar` is a generic function. `foo` is the target expression, being passed as the `self` argument
    m_targetExpr = CloneAstNode(m_expr->GetTarget());

    if (m_targetExpr != nullptr)
    {
        // Visit it so that it can be analyzed
        m_targetExpr->Visit(visitor, mod);
    }

    const AstExpression* valueOf = m_expr->GetDeepValueOf();
    Assert(valueOf != nullptr);

    SymbolTypeRef exprType = m_expr->GetExprType();
    Assert(exprType != nullptr);
    exprType = exprType->GetUnaliased();

    // Check if the expression is a generic expression.
    const AstExpression* genericExpr = valueOf->GetHeldGenericExpr();

    if (genericExpr == nullptr)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expression_not_generic,
            m_expr->GetLocation(),
            exprType->ToString()));

        return;
    }

    Assert(exprType != nullptr);

    Optional<SymbolTypeFunctionSignature> substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
        visitor, mod,
        exprType,
        m_genericArgs,
        m_location);

    if (!substituted.HasValue())
    {
        // not a generic if it doesnt resolve
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_type_not_generic,
            m_location,
            exprType->ToString()));

        return;
    }

    SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
        visitor, mod,
        exprType,
        m_genericArgs,
        m_location);

    Assert(substituted->returnType != nullptr);

    m_exprType = substituted->returnType;
    m_substitutedArgs = substituted->params;

    Assert(m_exprType != nullptr);

    GenericInstanceCache::Key genericInstanceCacheKey;
    // { // look in generic instance cache
    //     genericInstanceCacheKey.genericExpr = CloneAstNode(genericExpr);
    //     genericInstanceCacheKey.argHashCodes.Resize(m_substitutedArgs.Size());

    //     for (SizeType index = 0; index < m_substitutedArgs.Size(); index++) {
    //         const auto &arg = m_substitutedArgs[index];
    //         Assert(arg != nullptr);

    //         genericInstanceCacheKey.argHashCodes[index] = arg->GetHashCode();
    //     }

    //     Optional<GenericInstanceCache::CachedObject> cachedObject = mod->LookupGenericInstance(genericInstanceCacheKey);

    //     if (cachedObject.HasValue()) {
    //         m_innerExpr = cachedObject.Get().instantiatedExpr;
    //         Assert(m_innerExpr != nullptr);

    //         return;
    //     }
    // }

    const Array<GenericInstanceTypeInfo::Arg>& params = exprType->GetGenericInstanceInfo().m_genericArgs;
    Assert(params.Size() >= 1);

    Array<GenericInstanceTypeInfo::Arg> args;
    args.Resize(m_substitutedArgs.Size());

    // temporarily define all generic parameters as constants
    for (SizeType index = 0; index < m_substitutedArgs.Size(); index++)
    {
        RC<AstArgument>& arg = m_substitutedArgs[index];
        Assert(arg != nullptr);

        const AstExpression* valueOf = arg->GetDeepValueOf();
        Assert(valueOf != nullptr);

        SymbolTypeRef memberExprType = valueOf->GetExprType();
        Assert(memberExprType != nullptr);
        memberExprType = memberExprType->GetUnaliased();

        // For each generic parameter, we add a new symbol type to the current scope
        // as an alias to the held type of the argument.

        SymbolTypeRef heldType = valueOf->GetHeldType();

        if (heldType == nullptr)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_type,
                arg->GetLocation(),
                memberExprType->ToString()));

            continue;
        }

        heldType = heldType->GetUnaliased();

        String paramName;

        if (index + 1 < params.Size())
        {
            paramName = params[index + 1].m_name;
        }
        else
        {
            // if there are more arguments than generic parameters, we just use a generic name
            paramName = params.Back().m_name + String::ToString(index);
        }

        args[index] = GenericInstanceTypeInfo::Arg {
            paramName,
            heldType
        };

        scope->GetIdentifierTable().AddSymbolType(SymbolType::Alias(
            paramName,
            { heldType }));

        RC<AstVariableDeclaration> paramOverride(new AstVariableDeclaration(
            paramName,
            nullptr,
            RC<AstTypeRef>(new AstTypeRef(
                heldType,
                SourceLocation::eof)),
            IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC_SUBSTITUTION,
            arg->GetLocation()));

        m_block->AddChild(paramOverride);
    }

    // Set up the expression wrapper
    m_innerExpr.Reset(new AstTemplateInstantiationWrapper(
        CloneAstNode(genericExpr),
        args,
        m_location));

    m_block->AddChild(m_innerExpr);
    m_block->Visit(visitor, mod);

    Assert(m_innerExpr->GetExprType() != nullptr);

    // If the current return type is a placeholder, we need to set it to the inner expression's implicit return type
    if (m_exprType->IsPlaceholderType())
    {
        m_exprType = m_innerExpr->GetExprType();
        m_exprType = m_exprType->GetUnaliased();
    }
    else
    {
        SemanticAnalyzer::Helpers::EnsureLooseTypeAssignmentCompatibility(
            visitor,
            mod,
            m_innerExpr->GetExprType(),
            m_exprType,
            m_location);
    }

    m_heldType = m_innerExpr->GetHeldType();

    // If expr type is native just load the original type.
    if (exprType->GetFlags() & SYMBOL_TYPE_FLAGS_NATIVE)
    {
        m_isNative = true;

        m_block.Reset(new AstBlock({}, m_location));

        if (m_heldType != nullptr)
        {
            Assert(m_heldType->GetId() != -1,
                "For native generic types, the original generic type must be registered");

            // Add a type ref to the original type
            m_block->AddChild(RC<AstTypeRef>(new AstTypeRef(
                m_heldType,
                m_location)));
        }
        else
        {
            m_block->AddChild(CloneAstNode(m_expr));
        }

        m_block->Visit(visitor, mod);
    }
}

UniquePtr<Buildable> AstTemplateInstantiation::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_isVisited);

    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    auto chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_heldType != nullptr)
    {
        chunk->Append(BytecodeUtil::Make<Comment>(String("Begin generic instantiation for type `") + m_heldType->ToString() + String("`")));
    }
    else
    {
        Assert(m_exprType != nullptr);

        chunk->Append(BytecodeUtil::Make<Comment>(String("Begin generic instantiation for expression of type `") + m_exprType->ToString() + String("`")));
    }

    Assert(m_block != nullptr);
    chunk->Append(m_block->Build(visitor, mod));

    if (m_heldType != nullptr)
    {
        chunk->Append(BytecodeUtil::Make<Comment>(String("End generic instantiation for type `") + m_heldType->ToString() + String("`")));
    }
    else
    {
        Assert(m_exprType != nullptr);

        chunk->Append(BytecodeUtil::Make<Comment>(String("End generic instantiation for expression of type `") + m_exprType->ToString() + String("`")));
    }

    return chunk;

    // UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // // Build the arguments
    // chunk->Append(Compiler::BuildArgumentsStart(
    //     visitor,
    //     mod,
    //     m_substitutedArgs
    // ));

    // const int stackSizeBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // // Build the original expression.
    // // Usage of arguments in the expression will be replaced with the substituted arguments.
    // chunk->Append(m_expr->Build(visitor, mod));

    // const int stackSizeNow = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // Assert(
    //     stackSizeNow == stackSizeBefore,
    //     "Stack size mismatch detected! Internal record of stack does not match. (%d != %d)",
    //     stackSizeNow,
    //     stackSizeBefore
    // );

    // chunk->Append(Compiler::BuildArgumentsEnd(
    //     visitor,
    //     mod,
    //     m_substitutedArgs.Size()
    // ));

    // return chunk;
}

void AstTemplateInstantiation::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    Assert(m_block != nullptr);
    m_block->Optimize(visitor, mod);
}

RC<AstStatement> AstTemplateInstantiation::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateInstantiation::IsTrue() const
{
    if (m_innerExpr != nullptr)
    {
        return m_innerExpr->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstTemplateInstantiation::MayHaveSideEffects() const
{
    for (auto& arg : m_genericArgs)
    {
        Assert(arg != nullptr);

        if (arg->MayHaveSideEffects())
        {
            return true;
        }
    }

    if (m_innerExpr != nullptr)
    {
        return m_innerExpr->MayHaveSideEffects();
    }

    return true;
}

SymbolTypeRef AstTemplateInstantiation::GetExprType() const
{
    if (m_exprType != nullptr)
    {
        return m_exprType;
    }

    return BuiltinTypes::UNDEFINED;
}

SymbolTypeRef AstTemplateInstantiation::GetHeldType() const
{
    if (m_heldType)
    {
        return m_heldType;
    }

    return AstExpression::GetHeldType();
}

const AstExpression* AstTemplateInstantiation::GetValueOf() const
{
    if (m_innerExpr != nullptr)
    {
        return m_innerExpr.Get();
        // Assert(m_innerExpr.Get() != this);

        // return m_innerExpr->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression* AstTemplateInstantiation::GetDeepValueOf() const
{
    if (m_innerExpr != nullptr)
    {
        Assert(m_innerExpr->GetExpr().Get() != this);

        return m_innerExpr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

AstExpression* AstTemplateInstantiation::GetTarget() const
{
    if (m_targetExpr != nullptr)
    {
        Assert(m_targetExpr.Get() != this);

        return m_targetExpr.Get();
    }

    return AstExpression::GetTarget();
}

} // namespace hyperion::compiler
