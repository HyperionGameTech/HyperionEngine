#include <Lang/Compiler/Ast/AstIsExpression.hpp>
#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/Instruction.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <iostream>

#include <AstIsExpression.generated.inl>

namespace Hyperion {

AstIsExpression::AstIsExpression(
    const RC<AstExpression>& target,
    const RC<AstTypeSpecifier>& typeSpec,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_typeSpec(typeSpec),
      m_isType(TRI_INDETERMINATE)
{
}

void AstIsExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    m_target->Visit(visitor, mod);

    Assert(m_typeSpec != nullptr);
    m_typeSpec->Visit(visitor, mod);

    if (const auto targetType = m_target->GetExprType())
    {
        if (const auto heldType = m_typeSpec->GetHeldType())
        {
            if (targetType->IsAnyType())
            {
                // 'any' must defer to runtime type check
            }
            else if (targetType->IsNumber() && heldType->IsNumber())
            {
                if (targetType->TypeEqual(*heldType))
                {
                    m_isType = TRI_TRUE;
                }
                else
                {
                    m_isType = TRI_FALSE;
                }
            }
            else if (targetType->TypeCompatible(
                         *heldType,
                         /* strictNumbers */ true,
                         /* strictAny */ false,
                         /* strictEnum */ true,
                         /* strictNull */ true,
                         /* strictDownCasting */ true))
            {
                m_isType = TRI_TRUE;
            }
            else
            {
                m_isType = TRI_FALSE;
            }
        }
    }
}

UniquePtr<Buildable> AstIsExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_isType == TRI_TRUE || m_isType == TRI_FALSE || m_isType == TRI_INDETERMINATE);

    if (m_isType == TRI_TRUE)
    {
        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        return BytecodeUtil::Make<ConstBool>(rp, true);
    }

    if (m_isType == TRI_FALSE)
    {
        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        return BytecodeUtil::Make<ConstBool>(rp, false);
    }

    Assert(m_isType == TRI_INDETERMINATE);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(m_target->Build(visitor, mod));
    uint8 srcReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    Assert(m_typeSpec->GetExpr() != nullptr);
    visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
    chunk->Append(m_typeSpec->GetExpr()->Build(visitor, mod));
    uint8 typeRefReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    const SymbolType* heldType = m_typeSpec->GetHeldType();
    Assert(heldType != nullptr);
    heldType = heldType->GetUnaliased();
    HashCode::ValueType typeNameHash = HashCode::GetHashCode(heldType->GetName()).Value();

    visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
    uint8 dstReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    chunk->Append(BytecodeUtil::Make<IsInstanceComp>(dstReg, srcReg, typeRefReg, typeNameHash));

    {
        constexpr uint8 subcmd = MAKE_MOV_SUBCMD(MDST_REGISTER, MSRC_REGISTER);

        auto instrMovReg = BytecodeUtil::Make<RawOperation<>>();
        instrMovReg->opcode = MOV_UNIFIED;
        instrMovReg->Accept<uint8>(subcmd);
        instrMovReg->Accept<uint8>(srcReg);
        instrMovReg->Accept<uint8>(dstReg);
        chunk->Append(std::move(instrMovReg));
    }

    visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage(); // dstReg
    visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage(); // typeRefReg

    return chunk;
}

void AstIsExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    m_target->Optimize(visitor, mod);

    Assert(m_typeSpec != nullptr);
    m_typeSpec->Optimize(visitor, mod);
}

RC<AstStatement> AstIsExpression::Clone() const
{
    return CloneImpl();
}

const SymbolType* AstIsExpression::GetExprType() const
{
    return BuiltinTypes::s_boolType;
}

Tribool AstIsExpression::IsTrue() const
{
    return m_isType;
}

bool AstIsExpression::MayHaveSideEffects() const
{
    Assert(
        m_target != nullptr && m_typeSpec != nullptr);

    return m_target->MayHaveSideEffects()
        || m_typeSpec->MayHaveSideEffects();
}

} // namespace Hyperion
