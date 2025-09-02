#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/math/MathUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

#include <limits>
#include <iostream>

namespace hyperion::compiler {

AstCallExpression::AstCallExpression(
    const RC<AstExpression>& expr,
    const Array<RC<AstArgument>>& args,
    bool insertSelf,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_args(args),
      m_insertSelf(insertSelf),
      m_returnType(BuiltinTypes::UNDEFINED)
{
    for (auto& arg : m_args)
    {
        Assert(arg != nullptr);
    }
}

void AstCallExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(!m_isVisited);
    m_isVisited = true;

    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    SymbolTypeRef targetType = m_expr->GetExprType();
    Assert(targetType != nullptr);

    Array<RC<AstArgument>> argsWithSelf = m_args;

    if (m_insertSelf)
    {
        auto* valueOf = m_expr->GetValueOf();
        Assert(valueOf != nullptr);

        if (const auto* leftTarget = m_expr->GetTarget())
        {
            const auto selfTarget = CloneAstNode(leftTarget);
            Assert(selfTarget != nullptr);

            RC<AstArgument> selfArg(new AstArgument(
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

    SymbolTypeRef unaliased = targetType->GetUnaliased();
    Assert(unaliased != nullptr);

    SymbolTypeRef callMemberType;
    String callMemberName;

    // check if $invoke is found on the object or its prototype
    if ((callMemberType = unaliased->FindMember("$invoke")) || (callMemberType = unaliased->FindPrototypeMember("$invoke")))
    {
        callMemberName = "$invoke";
    }

    if (callMemberType != nullptr)
    {
        // closure objects have a self parameter for the '$invoke' call.
        RC<AstArgument> closureSelfArg(new AstArgument(
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
            RC<AstMember>(new AstMember(
                callMemberName,
                CloneAstNode(m_expr),
                m_location)),
            CloneAllAstNodes(argsWithSelf),
            false,
            m_location));

        m_overrideExpr->Visit(visitor, mod);

        unaliased = callMemberType->GetUnaliased();
        Assert(unaliased != nullptr);
    }

    if (m_overrideExpr != nullptr)
    {
        return;
    }

    // visit each argument
    for (const RC<AstArgument>& arg : argsWithSelf)
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
        m_returnType = BuiltinTypes::ANY;
        m_substitutedArgs = argsWithSelf; // NOTE: do not clone because we don't need to visit again.
    }
    else if (!unaliased->IsOrHasBase(*BuiltinTypes::FUNCTION_BASE))
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
            m_returnType = BuiltinTypes::UNDEFINED;

            // not a function type
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_function,
                m_location,
                targetType->ToString(true)));

            return;
        }

        Assert(m_returnType != nullptr);

        for (const RC<AstArgument>& arg : m_substitutedArgs)
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
    Assert(m_isVisited);

    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->Build(visitor, mod);
    }

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

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
        numArgsToPop));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        numArgsToPop));

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

RC<AstStatement> AstCallExpression::Clone() const
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

SymbolTypeRef AstCallExpression::GetExprType() const
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

} // namespace hyperion::compiler
