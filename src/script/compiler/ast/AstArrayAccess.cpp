#include <script/compiler/ast/AstArrayAccess.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstArrayAccess::AstArrayAccess(
    const RC<AstExpression>& target,
    const RC<AstExpression>& index,
    const RC<AstExpression>& rhs,
    bool operatorOverloadingEnabled,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | (rhs != nullptr ? ACCESS_MODE_STORE : 0)),
      m_target(target),
      m_index(index),
      m_rhs(rhs),
      m_operatorOverloadingEnabled(operatorOverloadingEnabled)
{
}

void AstArrayAccess::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    Assert(m_index != nullptr);

    m_exprType = BuiltinTypes::UNDEFINED;

    m_target->Visit(visitor, mod);
    m_index->Visit(visitor, mod);

    if (m_rhs != nullptr)
    {
        m_rhs->Visit(visitor, mod);
    }

    SymbolTypeRef targetType = m_target->GetExprType();
    Assert(targetType != nullptr);
    targetType = targetType->GetUnaliased();

    if (mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG))
    {
        // TODO: implement ref array access
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location));

        return;
    }

    if (targetType->IsOrHasBase(*BuiltinTypes::ARRAY_BASE))
    {
        // array type

        Assert(targetType->GetGenericInstanceInfo().m_genericArgs.Size() == 1);

        SymbolTypeRef elementType = targetType->GetGenericInstanceInfo().m_genericArgs.Front().m_type;
        Assert(elementType != nullptr);

        elementType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
            visitor,
            mod,
            elementType,
            m_location);

        m_exprType = elementType;

        SemanticAnalyzer::Helpers::CheckArgTypeCompatible(
            visitor,
            mod,
            m_location,
            m_index->GetExprType(),
            BuiltinTypes::INT);

        if (m_rhs != nullptr)
        {
            // assigning to array index
            SemanticAnalyzer::Helpers::CheckArgTypeCompatible(
                visitor,
                mod,
                m_location,
                m_rhs->GetExprType(),
                elementType);
        }

        return;
    }

    if (targetType->IsAnyType())
    {
        m_exprType = BuiltinTypes::ANY;
    }

    if (m_operatorOverloadingEnabled)
    {
        // Treat it the same as AstBinaryExpression does - look for operator[] or operator[]=
        const String overloadFunctionName = m_rhs != nullptr
            ? "operator[]="
            : "operator[]";

        RC<AstArgumentList> argumentList(new AstArgumentList(
            { RC<AstArgument>(new AstArgument(
                CloneAstNode(m_index),
                false,
                false,
                false,
                false,
                "index",
                m_location)) },
            m_location));

        // add right hand side as argument if it exists
        if (m_rhs != nullptr)
        {
            argumentList->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                CloneAstNode(m_rhs),
                false,
                false,
                false,
                false,
                "value",
                m_location)));
        }

        RC<AstExpression> callOperatorOverloadExpr(new AstMemberCallExpression(
            overloadFunctionName,
            CloneAstNode(m_target),
            argumentList, // use right hand side as arg
            m_location));

        if (targetType->IsProxyClass() && targetType->FindMember(overloadFunctionName))
        {
            RC<AstCallExpression> callExpr(new AstCallExpression(
                RC<AstMember>(new AstMember(
                    overloadFunctionName,
                    CloneAstNode(m_target),
                    m_location)),
                { RC<AstArgument>(new AstArgument(
                    CloneAstNode(m_index),
                    false,
                    false,
                    false,
                    false,
                    "index",
                    m_location)) },
                true,
                m_location));

            // add right hand side as argument if it exists
            if (m_rhs != nullptr)
            {
                callExpr->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                    CloneAstNode(m_rhs),
                    false,
                    false,
                    false,
                    false,
                    "value",
                    m_location)));
            }

            m_overrideExpr = std::move(callExpr);
        }
        else if (targetType->IsAnyType())
        {
            // if target is ANY, we need to clone this (without operator overloading enabled)
            // and conditionally call the operator overload if it exists
            RC<AstArrayAccess> subExpr = Clone().CastUnsafe<AstArrayAccess>();
            subExpr->SetIsOperatorOverloadingEnabled(false); // don't look for operator[] again

            m_overrideExpr.Reset(new AstTernaryExpression(
                RC<AstHasExpression>(new AstHasExpression(CloneAstNode(m_target), overloadFunctionName, m_location)),
                callOperatorOverloadExpr,
                subExpr,
                m_location));
        }
        else if (targetType->FindMemberDeep(overloadFunctionName) != nullptr)
        {
            m_overrideExpr = std::move(callOperatorOverloadExpr);
        }
        else
        {
            // Add error
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_invalid_subscript,
                m_location,
                targetType->ToString()));
        }

        if (m_overrideExpr != nullptr)
        {
            m_overrideExpr->SetAccessMode(GetAccessMode());
            m_overrideExpr->SetExpressionFlags(GetExpressionFlags());

            m_overrideExpr->Visit(visitor, mod);

            m_exprType = m_overrideExpr->GetExprType();

            return;
        }
    }
}

UniquePtr<Buildable> AstArrayAccess::Build(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->Build(visitor, mod);
    }

    Assert(m_target != nullptr);
    Assert(m_index != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const bool targetSideEffects = m_target->MayHaveSideEffects();
    const bool indexSideEffects = m_index->MayHaveSideEffects();

    uint8 rhsRegister = uint8(-1);
    int rhsStackLocation = -1;

    if (m_rhs != nullptr)
    {
        rhsRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        chunk->Append(m_rhs->Build(visitor, mod));

        if (targetSideEffects || indexSideEffects)
        {
            rhsStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
            visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

            // move rhs to stack
            auto instr = BytecodeUtil::Make<RawOperation<>>();
            instr->opcode = PUSH;
            instr->Accept<uint8>(rhsRegister);
            chunk->Append(std::move(instr));
        }
        else
        {
            // preserve register for rhs
            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        }
    }

    uint8 r0, r1;

    Compiler::ExprInfo info {
        m_target.Get(),
        m_index.Get()
    };

    if (!indexSideEffects)
    {
        chunk->Append(Compiler::LoadLeftThenRight(visitor, mod, info));
        const uint8 currentRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = currentRegister - 1;
        r1 = currentRegister;
    }
    else if (indexSideEffects && !targetSideEffects)
    {
        // load the index and store it
        chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
        const uint8 currentRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = currentRegister;
        r1 = currentRegister - 1;
    }
    else
    {
        // load target, store it, then load the index
        chunk->Append(Compiler::LoadLeftAndStore(visitor, mod, info));
        const uint8 currentRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = currentRegister;
        r1 = currentRegister - 1;
    }

    if (m_rhs != nullptr)
    {
        if (targetSideEffects || indexSideEffects)
        {
            // preserve index register
            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

            // pop rhs from stack back into a register
            rhsRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage(); // preserve rhs register

            {
                auto instrLoadOffset = BytecodeUtil::Make<RawOperation<>>();
                instrLoadOffset->opcode = LOAD_OFFSET;
                instrLoadOffset->Accept<uint8>(rhsRegister);
                instrLoadOffset->Accept<uint16>(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - rhsStackLocation);
                chunk->Append(std::move(instrLoadOffset));
            }

            {
                auto instrPop = BytecodeUtil::Make<RawOperation<>>();
                instrPop->opcode = POP;
                chunk->Append(std::move(instrPop));
            }

            // decrement stack size for rhs
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

            // unclaim register for index
            visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        }

        auto instr = BytecodeUtil::Make<RawOperation<>>();
        instr->opcode = MOV_ARRAYIDX_REG;
        instr->Accept<uint8>(r0);          // destination
        instr->Accept<uint8>(r1);          // index
        instr->Accept<uint8>(rhsRegister); // rhs

        chunk->Append(std::move(instr));

        // unclaim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        // unclaim register for rhs
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

        // move result from rhs register to the active register
        const uint8 dstRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (dstRegister != rhsRegister)
        {
            auto instrMov = BytecodeUtil::Make<RawOperation<>>();
            instrMov->opcode = MOV;
            instrMov->Accept<uint8>(dstRegister);
            instrMov->Accept<uint8>(rhsRegister);
            chunk->Append(std::move(instrMov));
        }
    }
    else
    {
        // unclaim register for array
        const uint8 dstRegister = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

        auto instr = BytecodeUtil::Make<RawOperation<>>();
        instr->opcode = LOAD_ARRAYIDX;
        instr->Accept<uint8>(dstRegister); // destination
        instr->Accept<uint8>(r0);          // source
        instr->Accept<uint8>(r1);          // index

        chunk->Append(std::move(instr));
    }

    return chunk;
}

void AstArrayAccess::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Optimize(visitor, mod);

        return;
    }

    Assert(m_target != nullptr);
    Assert(m_index != nullptr);

    m_target->Optimize(visitor, mod);
    m_index->Optimize(visitor, mod);
}

RC<AstStatement> AstArrayAccess::Clone() const
{
    return CloneImpl();
}

Tribool AstArrayAccess::IsTrue() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstArrayAccess::MayHaveSideEffects() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->MayHaveSideEffects();
    }

    return m_target->MayHaveSideEffects() || m_index->MayHaveSideEffects()
        || (m_rhs != nullptr && m_rhs->MayHaveSideEffects())
        || m_accessMode == ACCESS_MODE_STORE;
}

SymbolTypeRef AstArrayAccess::GetExprType() const
{
    return m_exprType;
}

AstExpression* AstArrayAccess::GetTarget() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetTarget();
    }

    if (m_target != nullptr)
    {
        if (auto* nestedTarget = m_target->GetTarget())
        {
            return nestedTarget;
        }

        return m_target.Get();
    }

    return AstExpression::GetTarget();
}

bool AstArrayAccess::IsMutable() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->IsMutable();
    }

    Assert(m_target != nullptr);

    if (!m_target->IsMutable())
    {
        return false;
    }

    return true;
}

const AstExpression* AstArrayAccess::GetValueOf() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetValueOf();
    }

    if (m_rhs != nullptr)
    {
        return m_rhs->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression* AstArrayAccess::GetDeepValueOf() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetDeepValueOf();
    }

    if (m_rhs != nullptr)
    {
        return m_rhs->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
