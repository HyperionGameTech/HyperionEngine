#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <iostream>

namespace hyperion::compiler {

AstAsExpression::AstAsExpression(
    const RC<AstExpression>& target,
    const RC<AstPrototypeSpecification>& typeSpecification,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_typeSpecification(typeSpecification),
      m_isType(TRI_INDETERMINATE)
{
}

void AstAsExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    m_target->Visit(visitor, mod);

    Assert(m_typeSpecification != nullptr);
    m_typeSpecification->Visit(visitor, mod);

    auto* targetValueOf = m_target->GetDeepValueOf();
    Assert(targetValueOf != nullptr);

    SymbolTypeRef targetType = targetValueOf->GetExprType();
    if (targetType == nullptr)
    {
        return; // should be caught by the type specification
    }

    targetType = targetType->GetUnaliased();

    auto* typeSpecificationValueOf = m_typeSpecification->GetDeepValueOf();
    Assert(typeSpecificationValueOf != nullptr);

    SymbolTypeRef heldType = typeSpecificationValueOf->GetHeldType();
    if (heldType == nullptr)
    {
        return; // should be caught by the type specification
    }

    heldType = heldType->GetUnaliased();

    if (heldType->IsAnyType())
    {
        m_isType = TRI_TRUE;

        return;
    }

    if (heldType->IsPlaceholderType())
    {
        m_isType = TRI_INDETERMINATE;

        return;
    }

    if (targetType->TypeEqual(*heldType))
    {
        m_isType = TRI_TRUE;

        return;
    }

    if (!targetType->TypeCompatible(*heldType, false))
    {
        // not compatible
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_incompatible_cast,
            m_location,
            targetType->ToString(),
            heldType->ToString()));

        return;
    }
}

UniquePtr<Buildable> AstAsExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    Assert(m_typeSpecification != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    bool typeSpecBuilt = false;

    // if the type spec has side effects, build it in even though it's not needed for the cast
    if (m_typeSpecification->MayHaveSideEffects())
    {
        chunk->Append(m_typeSpecification->Build(visitor, mod));

        typeSpecBuilt = true;
    }

    if (m_isType == TRI_TRUE)
    {
        // just build the target
        chunk->Append(m_target->Build(visitor, mod));

        return chunk;
    }

    const uint8 srcRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // Load the target into src
    chunk->Append(m_target->Build(visitor, mod));

    visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

    const uint8 dstRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto* valueOf = m_typeSpecification->GetDeepValueOf();
    Assert(valueOf != nullptr);

    SymbolTypeRef heldType = valueOf->GetHeldType();
    Assert(heldType != nullptr);
    heldType = heldType->GetUnaliased();

    Assert(!heldType->IsAnyType());

    if (heldType->IsSignedIntegral())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_I32, dstRegister, srcRegister));
    }
    else if (heldType->IsUnsignedIntegral())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_U32, dstRegister, srcRegister));
    }
    else if (heldType->IsFloat())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_F32, dstRegister, srcRegister));
    }
    else if (heldType->IsBoolean())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_BOOL, dstRegister, srcRegister));
    }
    else
    {
        // dynamic type needs to load the type object into the dst register.
        // if the type spec has side effects, it's already built in
        if (!typeSpecBuilt)
        {
            chunk->Append(m_typeSpecification->Build(visitor, mod));

            typeSpecBuilt = true;
        }

        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_DYNAMIC, dstRegister, srcRegister));
    }

    { // swap dst and src
        auto instrMovReg = BytecodeUtil::Make<RawOperation<>>();
        instrMovReg->opcode = MOV_REG;
        instrMovReg->Accept<uint8>(srcRegister);
        instrMovReg->Accept<uint8>(dstRegister);
        chunk->Append(std::move(instrMovReg));
    }

    visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

    return chunk;
}

void AstAsExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    m_target->Optimize(visitor, mod);

    Assert(m_typeSpecification != nullptr);
    m_typeSpecification->Optimize(visitor, mod);
}

RC<AstStatement> AstAsExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypeRef AstAsExpression::GetExprType() const
{
    Assert(m_target != nullptr);
    Assert(m_typeSpecification != nullptr);

    if (SymbolTypeRef heldType = m_typeSpecification->GetHeldType())
    {
        return heldType;
    }

    return BuiltinTypes::UNDEFINED;
}

Tribool AstAsExpression::IsTrue() const
{
    return TRI_INDETERMINATE;
}

bool AstAsExpression::MayHaveSideEffects() const
{
    Assert(
        m_target != nullptr && m_typeSpecification != nullptr);

    return m_target->MayHaveSideEffects()
        || m_typeSpecification->MayHaveSideEffects();
}

} // namespace hyperion::compiler
