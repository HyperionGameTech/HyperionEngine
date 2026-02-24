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
#include <Core/debug/Debug.hpp>

#include <iostream>

namespace Hyperion {

AstAsExpression::AstAsExpression(
    const RC<AstExpression>& target,
    const RC<AstTypeSpecifier>& typeSpecification,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_typeSpecification(typeSpecification),
      m_isType(TRI_INDETERMINATE),
      m_resultType(nullptr)
{
}

void AstAsExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    m_target->Visit(visitor, mod);

    Assert(m_typeSpecification != nullptr);
    m_typeSpecification->Visit(visitor, mod);

    const SymbolType* heldType = m_typeSpecification->GetHeldType();
    Assert(heldType != nullptr);

    const SymbolType* resolvedType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(visitor, mod, heldType, m_location);
    Assert(resolvedType != nullptr);
    resolvedType->Register(visitor->GetCompilationUnit());

    m_resultType = resolvedType->GetUnaliased();

    if (m_resultType->IsAnyType())
    {
        m_isType = TRI_TRUE;

        return;
    }

    const AstExpression* targetValueOf = m_target->GetDeepValueOf();
    Assert(targetValueOf != nullptr);

    const SymbolType* targetType = targetValueOf->GetExprType();

    if (!targetType)
    {
        m_resultType = BuiltinTypes::s_errorType;

        return; // should be caught by the type specification
    }

    targetType = targetType->GetUnaliased();

    if (targetType->TypeEqual(*m_resultType))
    {
        m_isType = TRI_TRUE;

        return;
    }

    SymbolTypeIncompatibilities incompatibilities;

    if (!m_resultType->TypeCompatible(
            *targetType,
            /* strictNumbers */ false,
            /* strictAny */ false,
            /* strictEnum */ false,
            /* strictNull */ true,
            /* strictDownCasting */ false,
            &incompatibilities))
    {
        if (incompatibilities.Any())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_incompatible_cast_more_info,
                m_location,
                targetType->ToString(),
                m_resultType->ToString(),
                (incompatibilities.Size() > 1
                        ? "\n\t* " + String::Join(Map(incompatibilities, &SymbolTypeIncompatibility::details), "\n\t* ")
                        : " " + incompatibilities[0].details)));
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_incompatible_cast,
                m_location,
                targetType->ToString(),
                m_resultType->ToString()));
        }

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

    const SymbolType* resultType = m_resultType;

    if (m_resultType->IsEnumType())
    {
        resultType = m_resultType->GetBaseType();
    }

    if (resultType->IsSignedIntegral())
    {
        switch (resultType->GetConstantBitSize())
        {
        case CBS_8:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_I32, dstRegister, srcRegister)); // @TODO
            break;
        case CBS_16:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_I32, dstRegister, srcRegister)); // @TODO
            break;
        case CBS_32:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_I32, dstRegister, srcRegister));
            break;
        case CBS_64:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_I64, dstRegister, srcRegister));
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
    else if (resultType->IsUnsignedIntegral())
    {
        switch (resultType->GetConstantBitSize())
        {
        case CBS_8:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_U32, dstRegister, srcRegister)); // @TODO
            break;
        case CBS_16:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_U32, dstRegister, srcRegister)); // @TODO
            break;
        case CBS_32:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_U32, dstRegister, srcRegister));
            break;
        case CBS_64:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_U64, dstRegister, srcRegister));
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
    else if (resultType->IsFloat())
    {
        switch (resultType->GetConstantBitSize())
        {
        case CBS_32:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_F32, dstRegister, srcRegister));
            break;
        case CBS_64:
            chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_F64, dstRegister, srcRegister));
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
    else if (resultType->IsBoolean())
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_BOOL, dstRegister, srcRegister));
    }
    else if (resultType->IsOrHasBase(*BuiltinTypes::s_stringType))
    {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_STRING, dstRegister, srcRegister));
    }
    else if (resultType->IsObject())
    {
        // dynamic type needs to load the class into a register (reuse dstRegister)
        const String className = resultType->GetName();

        chunk->Append(BytecodeUtil::Make<LoadClass>(dstRegister, CreateNameFromDynamicString(className)));
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_DYNAMIC, dstRegister, srcRegister));
    }
    else
    {
        // type casting not implemented for this case
        DebugLog(
            LogType::Error,
            "AstAsExpression::Build: Type casting not implemented for type '%s'\n",
            resultType->ToString().Data());

        // log type chain
        {
            const SymbolType* type = resultType;
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

const SymbolType* AstAsExpression::GetExprType() const
{
    Assert(m_target != nullptr);
    Assert(m_typeSpecification != nullptr);

    if (const SymbolType* heldType = m_typeSpecification->GetHeldType())
    {
        return heldType;
    }

    return BuiltinTypes::s_errorType;
}

bool AstAsExpression::IsLiteral() const
{
    Assert(m_target != nullptr);
    Assert(m_typeSpecification != nullptr);

    return m_target->IsLiteral();
}

Tribool AstAsExpression::IsTrue() const
{
    return TRI_INDETERMINATE;
}

bool AstAsExpression::MayHaveSideEffects() const
{
    Assert(m_target != nullptr && m_typeSpecification != nullptr);

    return m_target->MayHaveSideEffects()
        || m_typeSpecification->MayHaveSideEffects();
}

ConstantValue AstAsExpression::GetConstantValue() const
{
    Assert(m_target != nullptr);
    Assert(m_resultType != nullptr);

    const ConstantValue targetValue = m_target->GetConstantValue();

    if (!targetValue.IsValid())
    {
        return ConstantValue(INVALID_CONSTANT_NUMBER);
    }

    const SymbolType* resultType = m_resultType;
    Assert(resultType != nullptr);
    resultType = resultType->GetUnaliased();

    if (resultType->IsAnyType())
    {
        return targetValue;
    }

    if (resultType->IsEnumType())
    {
        const SymbolType* baseType = resultType->GetBaseType();
        Assert(baseType != nullptr);

        resultType = baseType->GetUnaliased();
    }

    if (resultType->IsSignedIntegral())
    {
        return ConstantValue(targetValue.AsInt(), resultType->GetConstantBitSize());
    }

    if (resultType->IsUnsignedIntegral())
    {
        return ConstantValue(targetValue.AsUInt(), resultType->GetConstantBitSize());
    }

    if (resultType->IsFloat())
    {
        return ConstantValue(targetValue.AsFloat(), resultType->GetConstantBitSize());
    }

    if (resultType->IsBoolean())
    {
        return ConstantValue(targetValue.AsBool(), CBS_8);
    }

    return ConstantValue(INVALID_CONSTANT_NUMBER);
}

RC<AstStatement> AstAsExpression::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
