#include <Lang/Compiler/Ast/AstIsExpression.hpp>
#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <iostream>

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
            bool typesMatch = false;

            if (targetType->IsNumber() && heldType->IsNumber())
            {
                typesMatch = targetType->TypeEqual(*heldType);
            }
            else
            {
                typesMatch = targetType->TypeCompatible(
                    *heldType,
                    /* strictNumbers */ true,
                    /* strictAny */ false,
                    /* strictEnum */ true,
                    /* strictNull */ true,
                    /* strictDownCasting */ true);
            }

            if (typesMatch)
            {
                m_isType = TRI_TRUE;
            }
            else
            {
                m_isType = TRI_FALSE;
            }
        }
    }

    if (m_isType == TRI_INDETERMINATE)
    {
        // clang-format off
        // runtime check
        m_overrideExpr = visitor->GetCompilationUnit()->GetAstNodeBuilder()
            .Module(ScriptConfig::GlobalModuleName).Function("IsInstance")
            .Call({
                RC<AstArgument>(new AstArgument(CloneAstNode(m_target), false, false, false, false, "", m_target->GetLocation())),
                RC<AstArgument>(new AstArgument(CloneAstNode(m_typeSpec->GetExpr()), false, false, false, false, "", m_typeSpec->GetLocation()))
            });

        // clang-format on

        m_overrideExpr->Visit(visitor, mod);
    }
}

UniquePtr<Buildable> AstIsExpression::Build(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->Build(visitor, mod);
    }

    Assert(m_isType == TRI_TRUE || m_isType == TRI_FALSE);

    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstBool>(rp, bool(m_isType.Value()));
}

void AstIsExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Optimize(visitor, mod);

        return;
    }

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
