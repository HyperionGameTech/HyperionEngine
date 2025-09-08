#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <iostream>

namespace hyperion {

AstAsExpression::AstAsExpression(
    const RC<AstExpression>& target,
    const RC<AstTypeSpecifier>& typeSpecification,
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

    SymbolTypeRef heldType = m_typeSpecification->GetHeldType();

    if (!heldType)
    {
        return; // should be caught by the type specification
    }

    m_resultType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
        visitor,
        mod,
        heldType,
        m_location);

    if (m_resultType->IsAnyType())
    {
        m_isType = TRI_TRUE;

        return;
    }

    if (targetType->TypeEqual(*m_resultType))
    {
        m_isType = TRI_TRUE;

        return;
    }

    if (!targetType->TypeCompatible(*m_resultType, /* strictNumbers */ false, /* strictAny */ false))
    {
        // not compatible
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_incompatible_cast,
            m_location,
            targetType->ToString(),
            m_resultType->ToString()));

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

    Assert(m_resultType != nullptr);

    if (m_resultType->IsSignedIntegral())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_I32, dstRegister, srcRegister));
    }
    else if (m_resultType->IsUnsignedIntegral())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_U32, dstRegister, srcRegister));
    }
    else if (m_resultType->IsFloat())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_F32, dstRegister, srcRegister));
    }
    else if (m_resultType->IsBoolean())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_BOOL, dstRegister, srcRegister));
    }
    else if (m_resultType->IsOrHasBase(*BuiltinTypes::s_stringType))
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_STRING, dstRegister, srcRegister));
    }
    else if (m_resultType->IsObject())
    {
        // dynamic type needs to load the class into a register (reuse dstRegister)
        const String className = m_resultType->GetName();

        chunk->Append(BytecodeUtil::Make<LoadClass>(dstRegister, CreateNameFromDynamicString(className)));
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_DYNAMIC, dstRegister, srcRegister));
    }
    else
    {
        // type casting not implemented for this case
        DebugLog(
            LogType::Error,
            "AstAsExpression::Build: Type casting not implemented for type '%s'\n",
            m_resultType->ToString().Data());

        // log type chain
        {
            SymbolTypeRef type = m_resultType;
            while (type != nullptr)
            {
                DebugLog(
                    LogType::Error,
                    "    Type: %s (class %d)\n",
                    type->ToString().Data(),
                    static_cast<int>(type->GetTypeClass()));

                type = type->GetBaseType();
            }
        }

        HYP_NOT_IMPLEMENTED();
    }

    { // swap dst and src
        constexpr uint8 subcmd = MAKE_MOV_SUBCMD(MDST_REGISTER, MSRC_REGISTER);

        auto instrMovReg = BytecodeUtil::Make<RawOperation<>>();
        instrMovReg->opcode = MOV_UNIFIED;
        instrMovReg->Accept<uint8>(subcmd);
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

    return BuiltinTypes::s_errorType;
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

} // namespace hyperion
