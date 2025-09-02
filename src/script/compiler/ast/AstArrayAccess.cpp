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
    bool operatorOverloadingEnabled,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_target(target),
      m_index(index),
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

        return;
    }

    if (targetType->IsAnyType())
    {
        m_exprType = BuiltinTypes::ANY;
    }

    if (m_operatorOverloadingEnabled)
    {
        // Treat it the same as AstBinaryExpression does - look for operator[] or operator[]=
        static const String overloadFunctionNames[] = { "operator[]", "operator[]=" };
        const String& overloadFunctionName = m_accessMode == ACCESS_MODE_STORE
            ? overloadFunctionNames[1]
            : overloadFunctionNames[0];

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

        if (m_accessMode == ACCESS_MODE_STORE)
        {
            argumentList->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                nullptr, // placeholder argument, will be filled in at bytecode generation time
                false,
                false,
                false,
                false,
                "value",
                m_location)));
        }

        RC<AstMemberCallExpression> operatorOverloadMemberCall(new AstMemberCallExpression(
            overloadFunctionName,
            CloneAstNode(m_target),
            argumentList, // use right hand side as arg
            m_location));

        operatorOverloadMemberCall->SetAccessMode(GetAccessMode());
        operatorOverloadMemberCall->SetExpressionFlags(GetExpressionFlags());

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

            if (m_accessMode == ACCESS_MODE_STORE)
            {
                callExpr->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                    nullptr, // placeholder argument, will be filled in at bytecode generation time
                    false,
                    false,
                    false,
                    false,
                    "value",
                    m_location)));
            }

            callExpr->SetAccessMode(GetAccessMode());
            callExpr->SetExpressionFlags(GetExpressionFlags());

            m_overrideExpr = std::move(callExpr);
        }
        else if (targetType->IsAnyType())
        {
            // if target is ANY, we need to clone this (without operator overloading enabled)
            // and conditionally call the operator overload if it exists
            RC<AstArrayAccess> subExpr = Clone().CastUnsafe<AstArrayAccess>();
            subExpr->SetIsOperatorOverloadingEnabled(false); // don't look for operator[] again
            subExpr->SetAccessMode(GetAccessMode());
            subExpr->SetExpressionFlags(GetExpressionFlags());

            m_overrideExpr.Reset(new AstTernaryExpression(
                RC<AstHasExpression>(new AstHasExpression(CloneAstNode(m_target), overloadFunctionName, m_location)),
                operatorOverloadMemberCall,
                subExpr,
                m_location));
        }
        else if (targetType->FindMemberDeep(overloadFunctionName) != nullptr)
        {
            m_overrideExpr = std::move(operatorOverloadMemberCall);
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
    Assert(m_target != nullptr);
    Assert(m_index != nullptr);

    const uint8 registerUsageBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const bool targetSideEffects = m_target->MayHaveSideEffects();
    const bool indexSideEffects = m_index->MayHaveSideEffects();
    const bool sideEffects = targetSideEffects
        || indexSideEffects
        || (m_overrideExpr != nullptr && (m_overrideExpr->MayHaveSideEffects() || m_accessMode == ACCESS_MODE_STORE));

    uint8 rhsRegister = uint8(-1);
    int rhsStackLocation = -1;

    if (m_accessMode == ACCESS_MODE_STORE)
    {
        rhsRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister() - 1;
        Assert(rhsRegister != uint8(-1)); // AstBinaryExpression should have reserved a register for the right hand side

        if (sideEffects)
        {
            rhsStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
            visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

            // move rhs to stack
            auto instr = BytecodeUtil::Make<RawOperation<>>();
            instr->opcode = PUSH;
            instr->Accept<uint8>(rhsRegister);
            chunk->Append(std::move(instr));

            rhsRegister = uint8(-1); // mark no longer valid so we don't trip over it
        }
    }

    uint8 r0 = uint8(-1);
    uint8 r1 = uint8(-1);

    if (!m_overrideExpr)
    {
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
    }

    if (m_accessMode == ACCESS_MODE_STORE)
    {
        if (m_overrideExpr != nullptr)
        {
            // RHS should be stored on stack here:
            Assert(rhsStackLocation != -1);

            // build override expression, which will use the rhsRegister as the value to store
            chunk->Append(m_overrideExpr->Build(visitor, mod));
        }
        else
        {
            if (sideEffects)
            {
                Assert(rhsRegister == uint8(-1) && rhsStackLocation != -1);

                // preserve index register
                visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

                // move rhs from stack back into a register
                rhsRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
                visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage(); // preserve rhs register now

                {
                    auto instrLoadOffset = BytecodeUtil::Make<RawOperation<>>();
                    instrLoadOffset->opcode = LOAD_OFFSET;
                    instrLoadOffset->Accept<uint8>(rhsRegister);
                    instrLoadOffset->Accept<uint16>(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - rhsStackLocation);
                    chunk->Append(std::move(instrLoadOffset));
                }

                // unclaim register for index
                visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

                // decrement stack size for rhs
                visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
            }

            // RHS should be in register here, no matter if side effects or not.
            Assert(r0 != uint8(-1) && r1 != uint8(-1) && rhsRegister != uint8(-1));

            auto instr = BytecodeUtil::Make<RawOperation<>>();
            instr->opcode = MOV_ARRAYIDX_REG;
            instr->Accept<uint8>(r0);          // destination
            instr->Accept<uint8>(r1);          // index
            instr->Accept<uint8>(rhsRegister); // rhs

            chunk->Append(std::move(instr));

            // unclaim register for array
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

        if (rhsStackLocation != -1)
        {
            // pop our rhs from the stack
            auto instrPop = BytecodeUtil::Make<RawOperation<>>();
            instrPop->opcode = POP;
            chunk->Append(std::move(instrPop));

            // decrement stack size for rhs
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

            rhsStackLocation = -1;
        }
    }
    else if (m_accessMode == ACCESS_MODE_LOAD)
    {
        if (m_overrideExpr != nullptr)
        {
            // build override expression, which will use the rhsRegister as the value to store
            chunk->Append(m_overrideExpr->Build(visitor, mod));
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
    }
    else
    {
        HYP_UNREACHABLE();
    }

    Assert(visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister() == registerUsageBefore);

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
        if (AstExpression* nestedTarget = m_target->GetTarget())
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

    return AstExpression::GetValueOf();
}

const AstExpression* AstArrayAccess::GetDeepValueOf() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
