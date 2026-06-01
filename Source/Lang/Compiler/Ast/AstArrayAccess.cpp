#include <Lang/Compiler/Ast/AstArrayAccess.hpp>
#include <Lang/Compiler/Ast/AstMemberCallExpression.hpp>
#include <Lang/Compiler/Ast/AstCallExpression.hpp>
#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Lang/Compiler/Ast/AstHasExpression.hpp>
#include <Lang/Compiler/Ast/AstTernaryExpression.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/StorageOperation.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

namespace Hyperion {

static constexpr const char* g_tempArrayStoreVarName = "$__arrayStoreValue";

AstArrayAccess::AstArrayAccess(
    const RC<AstExpression>& target,
    const RC<AstExpression>& index,
    bool operatorOverloadingEnabled,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_target(target),
      m_index(index),
      m_operatorOverloadingEnabled(operatorOverloadingEnabled),
      m_exprType(nullptr)
{
}

void AstArrayAccess::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    Assert(m_index != nullptr);

    m_exprType = BuiltinTypes::s_errorType;

    m_target->Visit(visitor, mod);

    {
        // Visit the index in its own scope so any REF_VARIABLE_FLAG from a parent
        // (e.g. AstMember for store-mode member access) does not leak into index sub-expressions.
        ScopeGuard indexScope(mod, SCOPE_TYPE_NORMAL);
        m_index->Visit(visitor, mod);
    }

    const SymbolType* targetType = m_target->GetExprType();
    Assert(targetType != nullptr);
    targetType = targetType->GetUnaliased();

    ScopeGuard scope(mod, SCOPE_TYPE_NORMAL);

    if (targetType->IsOrHasBase(*BuiltinTypes::s_arrayBaseType)
        || targetType->IsOrHasBase(*BuiltinTypes::s_varArgsBaseType))
    {
        // array type

        Assert(targetType->GetGenericInstanceInfo().m_genericArgs.Size() == 1);

        const SymbolType* elementType = targetType->GetGenericInstanceInfo().m_genericArgs.Front().m_type;
        Assert(elementType != nullptr);

        // supplant "SelfType" placeholder type with the actual target type
        SymbolType* selfAliasType = SymbolType::Alias("SelfType", { targetType });
        selfAliasType->Register(visitor->GetCompilationUnit());
        scope->identifierTable.AddSymbolType(selfAliasType);

        const SymbolType* resolvedType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
            visitor,
            mod,
            elementType,
            m_location);

        Assert(resolvedType != nullptr);
        resolvedType->Register(visitor->GetCompilationUnit());

        elementType = resolvedType;

        m_exprType = elementType;

        SemanticAnalyzer::Helpers::CheckArgTypeCompatible(
            visitor,
            mod,
            m_location,
            m_index->GetExprType(),
            BuiltinTypes::s_int32Type);

        return;
    }

    if (targetType->IsAnyType())
    {
        m_exprType = BuiltinTypes::s_anyType;
    }

    if (m_operatorOverloadingEnabled)
    {
        // Treat it the same as AstBinaryExpression does - look for operator[] or operator[]=
        static const String overloadFunctionNames[] = { "operator[]", "operator[]=" };
        const String& overloadFunctionName = m_accessMode == ACCESS_MODE_STORE
            ? overloadFunctionNames[1]
            : overloadFunctionNames[0];

        bool needsTemporaryVar = false;

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
            needsTemporaryVar = true;

            argumentList->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                RC<AstVariable>(new AstVariable(g_tempArrayStoreVarName, m_location)), // temporary variable to be stored into the array
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
                needsTemporaryVar = true;

                callExpr->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                    RC<AstVariable>(new AstVariable(g_tempArrayStoreVarName, m_location)), // temporary variable to be stored into the array
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
            RC<AstArrayAccess> subExpr = Clone().CastUnchecked<AstArrayAccess>();
            subExpr->SetIsOperatorOverloadingEnabled(false); // don't look for operator[] again

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

        if (needsTemporaryVar)
        {
            // create a temporary variable to hold the value to be stored into the array
            m_tempArrayStoreVarDecl.Reset(new AstVariableDeclaration(
                g_tempArrayStoreVarName,
                RC<AstTypeSpecifier>(new AstTypeSpecifier(
                    RC<AstTypeRef>(new AstTypeRef(BuiltinTypes::s_anyType, m_location)),
                    m_location)),
                nullptr, // no initializer
                IdentifierFlags::LAX,
                m_location));

            m_tempArrayStoreVarDecl->Visit(visitor, mod);
        }

        if (m_overrideExpr != nullptr)
        {
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

        if (m_tempArrayStoreVarDecl != nullptr)
        {
            // store rhs value into the temporary variable
            rhsStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
            Assert(rhsStackLocation != -1);

            m_tempArrayStoreVarDecl->GetIdentifier()->SetStackLocation(rhsStackLocation);

            visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

            // Push the value from the rhs register onto the stack
            auto instrPush = BytecodeUtil::Make<RawOperation<>>();
            instrPush->opcode = PUSH;
            instrPush->Accept<uint8>(rhsRegister);
            chunk->Append(std::move(instrPush));
        }

        if (sideEffects)
        {
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
            chunk->Append(m_overrideExpr->Build(visitor, mod));
        }
        else
        {
            if (sideEffects)
            {
                Assert(rhsRegister == uint8(-1));
                Assert(rhsStackLocation != -1);
                Assert(m_tempArrayStoreVarDecl != nullptr);

                // preserve index register
                visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

                // move rhs from stack back into a register
                rhsRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
                visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage(); // preserve rhs register now

                const int diff = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - rhsStackLocation;

                auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
                instrLoadOffset->GetBuilder().Load(rhsRegister).Local().ByOffset(diff);
                chunk->Append(std::move(instrLoadOffset));

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
                constexpr uint8 subcmd = MAKE_MOV_SUBCMD(MDST_REGISTER, MSRC_REGISTER);

                auto instrMov = BytecodeUtil::Make<RawOperation<>>();
                instrMov->opcode = MOV_UNIFIED;
                instrMov->Accept<uint8>(subcmd);
                instrMov->Accept<uint8>(dstRegister);
                instrMov->Accept<uint8>(rhsRegister);

                chunk->Append(std::move(instrMov));
            }
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

    if (m_tempArrayStoreVarDecl != nullptr)
    {
        // clean up the temporary variable
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        chunk->Append(Compiler::PopStack(visitor, 1));
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
        || m_accessMode == ACCESS_MODE_STORE;
}

const SymbolType* AstArrayAccess::GetExprType() const
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

} // namespace Hyperion
