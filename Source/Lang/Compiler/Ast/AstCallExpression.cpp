#include <Lang/Compiler/Ast/AstCallExpression.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Lang/Compiler/Ast/AstNewExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>
#include <Core/Unicode.hpp>

#include <limits>
#include <iostream>

#include <AstCallExpression.generated.inl>

namespace Hyperion {

AstCallExpression::AstCallExpression(
    const SharedPtr<AstExpression>& expr,
    const Array<SharedPtr<AstArgument>>& args,
    bool insertSelf,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_args(args),
      m_insertSelf(insertSelf),
      m_returnType(BuiltinTypes::s_errorType)
{
    for (auto& arg : m_args)
    {
        Assert(arg != nullptr);
    }
}

void AstCallExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    const SymbolType* targetType = m_expr->GetExprType();
    Assert(targetType != nullptr);

    Array<SharedPtr<AstArgument>> argsWithSelf = m_args;

    if (m_insertSelf)
    {
        const AstExpression* valueOf = m_expr->GetValueOf();
        Assert(valueOf != nullptr);

        const AstExpression* target = m_expr->GetTarget();

        if (target != nullptr)
        {
            const SymbolType* targetType = m_expr->GetTargetType();
            Assert(targetType != nullptr);

            if (!targetType->IsClassType() && target->GetHeldType() != nullptr)
            {
                // static call, don't insert self
                m_insertSelf = false;
            }
            else
            {
                /// \todo : Store self in a temporary variable instead of cloning so we don't evaluate it multiple times
                SharedPtr<AstExpression> selfTarget = CloneAstNode(target);
                Assert(selfTarget != nullptr);

                SharedPtr<AstArgument> selfArg(new AstArgument(
                    selfTarget,
                    false,
                    false,
                    false,
                    false,
                    "self",
                    selfTarget->GetLocation()));

                argsWithSelf.PushFront(std::move(selfArg));
            }
        }
    }

    // check if we are calling a type directly (static invoke)
    if (const SymbolType* heldType = m_expr->GetHeldType())
    {
        SymbolTypeMember member;
        uint32 memberIndex = ~0u;

        if (heldType->FindStaticMember("$invoke", member, memberIndex))
        {
            // transform into static member call
            m_overrideExpr.Reset(new AstCallExpression(
                SharedPtr<AstMember>(new AstMember("$invoke", CloneAstNode(m_expr), m_location)),
                CloneAllAstNodes(argsWithSelf),
                false,
                m_location));
        }
    }

    const SymbolType* unaliased = targetType->GetUnaliased();
    Assert(unaliased != nullptr);

    if (!m_overrideExpr && !unaliased->IsOrHasBase(*BuiltinTypes::s_functionBaseType))
    {
        // Check for $invoke instance member (for closure)
        const SymbolType* callMemberType = unaliased->FindMember("$invoke");

        if (callMemberType != nullptr)
        {
            // closure objects have a self parameter for the '$invoke' call.
            SharedPtr<AstArgument> closureSelfArg(new AstArgument(
                CloneAstNode(m_expr),
                false,
                false,
                false,
                false,
                "$functor",
                m_expr->GetLocation()));

            // insert at front
            argsWithSelf.PushFront(std::move(closureSelfArg));

            m_overrideExpr.Reset(new AstCallExpression(
                SharedPtr<AstMember>(new AstMember("$invoke", CloneAstNode(m_expr), m_location)),
                CloneAllAstNodes(argsWithSelf),
                false,
                m_location));

            unaliased = callMemberType->GetUnaliased();
            Assert(unaliased != nullptr);
        }
    }

    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Visit(visitor, mod);

        return;
    }

    // visit each argument
    for (const SharedPtr<AstArgument>& arg : argsWithSelf)
    {
        Assert(arg != nullptr);

        if (arg->IsPlaceholderArgument())
        {
            continue;
        }

        // note, visit in current module rather than module access
        // this is used so that we can call functions from separate modules,
        // yet still pass variables from the local module.
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }

    if (unaliased->IsAnyType())
    {
        m_returnType = BuiltinTypes::s_anyType;
        m_substitutedArgs = argsWithSelf; // NOTE: do not clone because we don't need to visit again.
    }
    else if (!unaliased->IsOrHasBase(*BuiltinTypes::s_functionBaseType))
    {
        // not a function type
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_function,
            m_location,
            targetType->ToString(true)));

        return;
    }
    else
    {
        bool substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
            visitor,
            mod,
            unaliased,
            argsWithSelf,
            m_location,
            m_returnType,
            m_substitutedArgs);

        if (!substituted)
        {
            m_returnType = BuiltinTypes::s_errorType;

            // not a function type
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_function_arg_substitution_failed,
                m_location));

            return;
        }

        Assert(m_returnType != nullptr);

        for (const SharedPtr<AstArgument>& arg : m_substitutedArgs)
        {
            if (!arg || arg->IsPlaceholderArgument())
            {
                continue;
            }

            arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }

        SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
            visitor,
            mod,
            unaliased,
            m_substitutedArgs,
            m_location);
    }

    if (m_substitutedArgs.Size() > MathUtil::MaxSafeValue<uint8>())
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_maximum_number_of_arguments,
            m_location));
    }
}

UniquePtr<Buildable> AstCallExpression::Build(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->Build(visitor, mod);
    }

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(BytecodeUtil::Make<Comment>("Begin function call: " + ToString()));

    uint16 numArgsToPop = 0;

    // build arguments
    chunk->Append(Compiler::BuildArgumentsStart(
        visitor,
        mod,
        m_substitutedArgs,
        numArgsToPop));

    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        m_expr,
        uint16(m_substitutedArgs.Size())));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        numArgsToPop));

    chunk->Append(BytecodeUtil::Make<Comment>("End function call: " + ToString()));

    return chunk;
}

void AstCallExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Optimize(visitor, mod);

        return;
    }

    // optimize each argument
    for (auto& arg : m_substitutedArgs)
    {
        if (arg != nullptr)
        {
            arg->Optimize(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }
    }
}

SharedPtr<AstStatement> AstCallExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstCallExpression::IsTrue() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->IsTrue();
    }

    // cannot deduce if return value is true
    return Tribool::Indeterminate();
}

bool AstCallExpression::MayHaveSideEffects() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->MayHaveSideEffects();
    }

    // assume a function call has side effects
    // maybe we could detect this later
    return true;
}

const SymbolType* AstCallExpression::GetExprType() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetExprType();
    }

    Assert(m_returnType != nullptr);
    return m_returnType;
}

AstExpression* AstCallExpression::GetTarget() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetTarget();
    }

    return AstExpression::GetTarget();
}

String AstCallExpression::ToString() const
{
    String result = (m_expr ? m_expr->ToString() : "<null>") + "(";

    for (size_t i = 0; i < m_args.Size(); ++i)
    {
        if (i > 0)
            result += ", ";
        const auto& arg = m_args[i];
        result += arg ? arg->ToString() : "<null>";
    }

    result += ")";
    return result;
}

} // namespace Hyperion
